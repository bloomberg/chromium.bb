// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/boot_phase_monitor/arc_boot_phase_monitor_bridge.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/chromeos/arc/boot_phase_monitor/arc_instance_throttle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_util.h"

namespace {

void OnEmitArcBooted(bool success) {
  if (!success)
    VLOG(1) << "Failed to emit arc booted signal.";
}

void RecordAppLaunchDelay(base::TimeDelta delta) {
  VLOG(2) << "Launching the first app took " << delta.InMillisecondsRoundedUp()
          << " ms.";
  UMA_HISTOGRAM_CUSTOM_TIMES("Arc.FirstAppLaunchDelay.TimeDelta", delta,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMinutes(2), 50);
}

}  // namespace

namespace arc {
namespace {

// Singleton factory for ArcBootPhaseMonitorBridge.
class ArcBootPhaseMonitorBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcBootPhaseMonitorBridge,
          ArcBootPhaseMonitorBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcBootPhaseMonitorBridgeFactory";

  static ArcBootPhaseMonitorBridgeFactory* GetInstance() {
    return base::Singleton<ArcBootPhaseMonitorBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcBootPhaseMonitorBridgeFactory>;
  ArcBootPhaseMonitorBridgeFactory() = default;
  ~ArcBootPhaseMonitorBridgeFactory() override = default;
};

}  // namespace

// static
ArcBootPhaseMonitorBridge* ArcBootPhaseMonitorBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcBootPhaseMonitorBridgeFactory::GetForBrowserContext(context);
}

// static
void ArcBootPhaseMonitorBridge::RecordFirstAppLaunchDelayUMA(
    content::BrowserContext* context) {
  auto* bridge = arc::ArcBootPhaseMonitorBridge::GetForBrowserContext(context);
  if (bridge)
    bridge->RecordFirstAppLaunchDelayUMAInternal();
}

ArcBootPhaseMonitorBridge::ArcBootPhaseMonitorBridge(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service),
      account_id_(multi_user_util::GetAccountIdFromProfile(
          Profile::FromBrowserContext(context))),
      binding_(this),
      first_app_launch_delay_recorder_(
          base::BindRepeating(&RecordAppLaunchDelay)) {
  arc_bridge_service_->boot_phase_monitor()->AddObserver(this);
  auto* arc_session_manager = ArcSessionManager::Get();
  DCHECK(arc_session_manager);
  arc_session_manager->AddObserver(this);
}

ArcBootPhaseMonitorBridge::~ArcBootPhaseMonitorBridge() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  arc_bridge_service_->boot_phase_monitor()->RemoveObserver(this);
  auto* arc_session_manager = ArcSessionManager::Get();
  DCHECK(arc_session_manager);
  arc_session_manager->RemoveObserver(this);
}

void ArcBootPhaseMonitorBridge::RecordFirstAppLaunchDelayUMAInternal() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (first_app_launch_delay_recorded_)
    return;
  first_app_launch_delay_recorded_ = true;

  if (boot_completed_) {
    VLOG(2) << "ARC has already fully started. Recording the UMA now.";
    first_app_launch_delay_recorder_.Run(base::TimeDelta());
    return;
  }
  app_launch_time_ = base::TimeTicks::Now();
}

void ArcBootPhaseMonitorBridge::OnInstanceReady() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->boot_phase_monitor(), Init);
  DCHECK(instance);
  mojom::BootPhaseMonitorHostPtr host_proxy;
  binding_.Bind(mojo::MakeRequest(&host_proxy));
  instance->Init(std::move(host_proxy));
}

void ArcBootPhaseMonitorBridge::OnBootCompleted() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(2) << "OnBootCompleted";
  boot_completed_ = true;

  chromeos::SessionManagerClient* session_manager_client =
      chromeos::DBusThreadManager::Get()->GetSessionManagerClient();
  session_manager_client->EmitArcBooted(cryptohome::Identification(account_id_),
                                        base::Bind(&OnEmitArcBooted));

  ArcSessionManager* arc_session_manager = ArcSessionManager::Get();
  DCHECK(arc_session_manager);
  if (arc_session_manager->is_directly_started()) {
    // Unless this is opt-in boot, start monitoring window activation changes to
    // prioritize/throttle the container when needed.
    throttle_ = base::MakeUnique<ArcInstanceThrottle>();
    VLOG(2) << "ArcInstanceThrottle created in OnBootCompleted()";
  }

  if (!app_launch_time_.is_null()) {
    first_app_launch_delay_recorder_.Run(base::TimeTicks::Now() -
                                         app_launch_time_);
  }
}

void ArcBootPhaseMonitorBridge::OnArcInitialStart() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // ARC apps for opt-in finished doing their jobs. Start the throttle.
  throttle_ = base::MakeUnique<ArcInstanceThrottle>();
  VLOG(2) << "ArcInstanceThrottle created in OnArcInitialStart()";
}

void ArcBootPhaseMonitorBridge::OnArcSessionStopped(ArcStopReason stop_reason) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Remove the throttle so that the window observer won't interfere with the
  // container startup when the user opts in to ARC.
  Reset();
  VLOG(2) << "ArcInstanceThrottle has been removed in OnArcSessionStopped()";
}

void ArcBootPhaseMonitorBridge::OnArcSessionRestarting() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Remove the throttle so that the window observer won't interfere with the
  // container restart.
  Reset();
  VLOG(2) << "ArcInstanceThrottle has been removed in OnArcSessionRestarting()";

  // We assume that a crash tends to happen while the user is actively using
  // the instance. For that reason, we try to restart the instance without the
  // restricted cgroups.
  SetArcCpuRestriction(false /* do_restrict */);
}

void ArcBootPhaseMonitorBridge::Reset() {
  throttle_.reset();
  app_launch_time_ = base::TimeTicks();
  first_app_launch_delay_recorded_ = false;
  boot_completed_ = false;
}

}  // namespace arc

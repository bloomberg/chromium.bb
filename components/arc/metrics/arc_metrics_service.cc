// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/metrics/arc_metrics_service.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"

namespace arc {

namespace {

constexpr base::TimeDelta kUmaMinTime = base::TimeDelta::FromMilliseconds(1);
constexpr base::TimeDelta kUmaMaxTime = base::TimeDelta::FromSeconds(60);
constexpr int kUmaNumBuckets = 50;

constexpr base::TimeDelta kRequestProcessListPeriod =
    base::TimeDelta::FromMinutes(5);
constexpr char kArcProcessNamePrefix[] = "org.chromium.arc.";
constexpr char kGmsProcessNamePrefix[] = "com.google.android.gms";
constexpr char kBootProgressEnableScreen[] = "boot_progress_enable_screen";

std::string BootTypeToString(mojom::BootType boot_type) {
  switch (boot_type) {
    case mojom::BootType::UNKNOWN:
      break;
    case mojom::BootType::FIRST_BOOT:
      return ".FirstBoot";
    case mojom::BootType::FIRST_BOOT_AFTER_UPDATE:
      return ".FirstBootAfterUpdate";
    case mojom::BootType::REGULAR_BOOT:
      return ".RegularBoot";
  }
  NOTREACHED();
  return "";
}

// Singleton factory for ArcMetricsService.
class ArcMetricsServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcMetricsService,
          ArcMetricsServiceFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcMetricsServiceFactory";

  static ArcMetricsServiceFactory* GetInstance() {
    return base::Singleton<ArcMetricsServiceFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcMetricsServiceFactory>;
  ArcMetricsServiceFactory() = default;
  ~ArcMetricsServiceFactory() override = default;
};

}  // namespace

// static
ArcMetricsService* ArcMetricsService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcMetricsServiceFactory::GetForBrowserContext(context);
}

ArcMetricsService::ArcMetricsService(content::BrowserContext* context,
                                     ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service),
      binding_(this),
      process_observer_(this),
      weak_ptr_factory_(this) {
  arc_bridge_service_->metrics()->AddObserver(this);
  arc_bridge_service_->process()->AddObserver(&process_observer_);
}

ArcMetricsService::~ArcMetricsService() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  arc_bridge_service_->process()->RemoveObserver(&process_observer_);
  arc_bridge_service_->metrics()->RemoveObserver(this);
}

void ArcMetricsService::OnConnectionReady() {
  VLOG(2) << "Start metrics service.";
  // Retrieve ARC start time from session manager.
  chromeos::SessionManagerClient* session_manager_client =
      chromeos::DBusThreadManager::Get()->GetSessionManagerClient();
  session_manager_client->GetArcStartTime(
      base::BindOnce(&ArcMetricsService::OnArcStartTimeRetrieved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcMetricsService::OnConnectionClosed() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(2) << "Close metrics service.";
  if (binding_.is_bound())
    binding_.Unbind();
}

void ArcMetricsService::OnProcessConnectionReady() {
  VLOG(2) << "Start updating process list.";
  timer_.Start(FROM_HERE, kRequestProcessListPeriod, this,
               &ArcMetricsService::RequestProcessList);
}

void ArcMetricsService::OnProcessConnectionClosed() {
  VLOG(2) << "Stop updating process list.";
  timer_.Stop();
}

void ArcMetricsService::RequestProcessList() {
  mojom::ProcessInstance* process_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->process(), RequestProcessList);
  if (!process_instance)
    return;
  VLOG(2) << "RequestProcessList";
  process_instance->RequestProcessList(base::Bind(
      &ArcMetricsService::ParseProcessList, weak_ptr_factory_.GetWeakPtr()));
}

void ArcMetricsService::ParseProcessList(
    std::vector<mojom::RunningAppProcessInfoPtr> processes) {
  int running_app_count = 0;
  for (const auto& process : processes) {
    const std::string& process_name = process->process_name;
    const mojom::ProcessState& process_state = process->process_state;

    // Processes like the ARC launcher and intent helper are always running
    // and not counted as apps running by users. With the same reasoning,
    // GMS (Google Play Services) and its related processes are skipped as
    // well. The process_state check below filters out system processes,
    // services, apps that are cached because they've run before.
    if (base::StartsWith(process_name, kArcProcessNamePrefix,
                         base::CompareCase::SENSITIVE) ||
        base::StartsWith(process_name, kGmsProcessNamePrefix,
                         base::CompareCase::SENSITIVE) ||
        process_state != mojom::ProcessState::TOP) {
      VLOG(2) << "Skipped " << process_name << " " << process_state;
    } else {
      ++running_app_count;
    }
  }

  UMA_HISTOGRAM_COUNTS_100("Arc.AppCount", running_app_count);
}

void ArcMetricsService::OnArcStartTimeRetrieved(
    base::Optional<base::TimeTicks> arc_start_time) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!arc_start_time.has_value()) {
    LOG(ERROR) << "Failed to retrieve ARC start timeticks.";
    return;
  }
  auto* instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->metrics(), Init);
  if (!instance)
    return;

  // The binding of host interface is deferred until the ARC start time is
  // retrieved here because it prevents race condition of the ARC start
  // time availability in ReportBootProgress().
  if (!binding_.is_bound()) {
    mojom::MetricsHostPtr host_ptr;
    binding_.Bind(mojo::MakeRequest(&host_ptr));
    instance->Init(std::move(host_ptr));
  }
  arc_start_time_ = arc_start_time.value();
  VLOG(2) << "ARC start @" << arc_start_time_;
}

void ArcMetricsService::ReportBootProgress(
    std::vector<mojom::BootProgressEventPtr> events,
    mojom::BootType boot_type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (boot_type == mojom::BootType::UNKNOWN) {
    LOG(WARNING) << "boot_type is unknown. Skip recording UMA.";
    return;
  }
  int64_t arc_start_time_in_ms =
      (arc_start_time_ - base::TimeTicks()).InMilliseconds();
  const std::string suffix = BootTypeToString(boot_type);
  for (const auto& event : events) {
    VLOG(2) << "Report boot progress event:" << event->event << "@"
            << event->uptimeMillis;
    const std::string name = "Arc." + event->event + suffix;
    const base::TimeDelta elapsed_time = base::TimeDelta::FromMilliseconds(
        event->uptimeMillis - arc_start_time_in_ms);
    base::UmaHistogramCustomTimes(name, elapsed_time, kUmaMinTime, kUmaMaxTime,
                                  kUmaNumBuckets);
    if (event->event.compare(kBootProgressEnableScreen) == 0) {
      base::UmaHistogramCustomTimes("Arc.AndroidBootTime" + suffix,
                                    elapsed_time, kUmaMinTime, kUmaMaxTime,
                                    kUmaNumBuckets);
    }
  }
}

ArcMetricsService::ProcessObserver::ProcessObserver(
    ArcMetricsService* arc_metrics_service)
    : arc_metrics_service_(arc_metrics_service) {}

ArcMetricsService::ProcessObserver::~ProcessObserver() = default;

void ArcMetricsService::ProcessObserver::OnConnectionReady() {
  arc_metrics_service_->OnProcessConnectionReady();
}

void ArcMetricsService::ProcessObserver::OnConnectionClosed() {
  arc_metrics_service_->OnProcessConnectionClosed();
}

}  // namespace arc

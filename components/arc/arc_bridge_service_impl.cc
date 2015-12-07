// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_bridge_service_impl.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "base/thread_task_runner_handle.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "mojo/public/cpp/bindings/array.h"
#include "third_party/mojo/src/mojo/edk/embedder/embedder.h"

namespace mojo {

template <>
struct TypeConverter<arc::AppInfo, arc::AppInfoPtr> {
  static arc::AppInfo Convert(const arc::AppInfoPtr& app_info_ptr) {
    return *app_info_ptr;
  }
};

}  // namespace mojo

namespace arc {

ArcBridgeServiceImpl::ArcBridgeServiceImpl(
    scoped_ptr<ArcBridgeBootstrap> bootstrap)
    : bootstrap_(std::move(bootstrap)),
      binding_(this),
      session_started_(false),
      weak_factory_(this) {
  bootstrap_->set_delegate(this);
}

ArcBridgeServiceImpl::~ArcBridgeServiceImpl() {
}

void ArcBridgeServiceImpl::DetectAvailability() {
  chromeos::SessionManagerClient* session_manager_client =
      chromeos::DBusThreadManager::Get()->GetSessionManagerClient();
  session_manager_client->CheckArcAvailability(base::Bind(
      &ArcBridgeServiceImpl::OnArcAvailable, weak_factory_.GetWeakPtr()));
}

void ArcBridgeServiceImpl::HandleStartup() {
  DCHECK(CalledOnValidThread());
  session_started_ = true;
  PrerequisitesChanged();
}

void ArcBridgeServiceImpl::Shutdown() {
  DCHECK(CalledOnValidThread());
  session_started_ = false;
  PrerequisitesChanged();
}

void ArcBridgeServiceImpl::PrerequisitesChanged() {
  DCHECK(CalledOnValidThread());
  if (state() == State::STOPPED) {
    if (!available() || !session_started_)
      return;
    SetState(State::CONNECTING);
    bootstrap_->Start();
  } else {
    if (available() && session_started_)
      return;
    StopInstance();
  }
}

void ArcBridgeServiceImpl::StopInstance() {
  DCHECK(CalledOnValidThread());
  if (state() == State::STOPPED || state() == State::STOPPING) {
    VLOG(1) << "StopInstance() called when ARC is not running";
    return;
  }

  SetState(State::STOPPING);
  bootstrap_->Stop();
}

bool ArcBridgeServiceImpl::RegisterInputDevice(const std::string& name,
                                               const std::string& device_type,
                                               base::ScopedFD fd) {
  DCHECK(CalledOnValidThread());
  if (state() != State::READY) {
    LOG(ERROR) << "Called RegisterInputDevice when the service is not ready";
    return false;
  }
  MojoHandle wrapped_handle;
  MojoResult wrap_result = mojo::embedder::CreatePlatformHandleWrapper(
      mojo::embedder::ScopedPlatformHandle(
          mojo::embedder::PlatformHandle(fd.release())),
      &wrapped_handle);
  if (wrap_result != MOJO_RESULT_OK) {
    LOG(WARNING) << "Pipe failed to wrap handles. Closing: " << wrap_result;
    return false;
  }
  instance_ptr_->RegisterInputDevice(
      name, device_type, mojo::ScopedHandle(mojo::Handle(wrapped_handle)));
  return true;
}

bool ArcBridgeServiceImpl::SendNotificationEventToAndroid(
    const std::string& key, ArcNotificationEvent event) {
  DCHECK(CalledOnValidThread());
  if (key.empty()) {
    LOG(ERROR) << "SendNotificationToAndroid failed: Wrong parameter";
    return false;
  }
  if (state() != State::READY) {
    LOG(ERROR) << "Called SendNotificationEventToAndroid when the service is"
      << "not ready";
    return false;
  }
  instance_ptr_->SendNotificationEventToAndroid(key, event);
  return true;
}

bool ArcBridgeServiceImpl::RefreshAppList() {
  DCHECK(CalledOnValidThread());
  if (state() != State::READY) {
    LOG(ERROR) << "Called RefreshAppList when the service is not ready";
    return false;
  }
  instance_ptr_->RefreshAppList();
  return true;
}

bool ArcBridgeServiceImpl::LaunchApp(const std::string& package,
                                     const std::string& activity) {
  DCHECK(CalledOnValidThread());
  if (state() != State::READY) {
    LOG(ERROR) << "Called LaunchApp when the service is not ready";
    return false;
  }
  instance_ptr_->LaunchApp(package, activity);
  return true;
}

bool ArcBridgeServiceImpl::RequestAppIcon(const std::string& package,
                                          const std::string& activity,
                                          ScaleFactor scale_factor) {
  DCHECK(CalledOnValidThread());
  if (state() != State::READY) {
    LOG(ERROR) << "Called RequestAppIcon when the service is not ready";
    return false;
  }
  instance_ptr_->RequestAppIcon(package, activity, scale_factor);
  return true;
}

void ArcBridgeServiceImpl::OnInstanceBootPhase(InstanceBootPhase phase) {
  DCHECK(CalledOnValidThread());
  // The state can be CONNECTED the first time this is called, and will then
  // transition to READY after BRIDGE_READY has been passed.
  if (state() != State::CONNECTED && state() != State::READY) {
    VLOG(1) << "StopInstance() called while connecting";
    return;
  }
  if (phase == INSTANCE_BOOT_PHASE_BRIDGE_READY) {
    SetState(State::READY);
  }
  FOR_EACH_OBSERVER(Observer, observer_list(), OnInstanceBootPhase(phase));
}

void ArcBridgeServiceImpl::OnNotificationPosted(ArcNotificationDataPtr data) {
  DCHECK(CalledOnValidThread());
  FOR_EACH_OBSERVER(NotificationObserver, notification_observer_list(),
                    OnNotificationPostedFromAndroid(*data.get()));
}

void ArcBridgeServiceImpl::OnNotificationRemoved(const mojo::String& key) {
  DCHECK(CalledOnValidThread());
  FOR_EACH_OBSERVER(NotificationObserver, notification_observer_list(),
                    OnNotificationRemovedFromAndroid(key));
}

void ArcBridgeServiceImpl::OnAppListRefreshed(
    mojo::Array<arc::AppInfoPtr> apps_ptr) {
  DCHECK(CalledOnValidThread());
  std::vector<arc::AppInfo> apps(apps_ptr.To<std::vector<arc::AppInfo>>());
  FOR_EACH_OBSERVER(AppObserver, app_observer_list(), OnAppListRefreshed(apps));
}

void ArcBridgeServiceImpl::OnAppIcon(const mojo::String& package,
                                     const mojo::String& activity,
                                     ScaleFactor scale_factor,
                                     mojo::Array<uint8_t> icon_png_data) {
  DCHECK(CalledOnValidThread());
  FOR_EACH_OBSERVER(
      AppObserver, app_observer_list(),
      OnAppIcon(package, activity, scale_factor, icon_png_data.storage()));
}

void ArcBridgeServiceImpl::OnAcquireDisplayWakeLock(DisplayWakeLockType type) {
  DCHECK(CalledOnValidThread());
  // TODO(ejcaruso): Implement.
  VLOG(1) << "OnAcquireDisplayWakeLock";
}

void ArcBridgeServiceImpl::OnReleaseDisplayWakeLock(DisplayWakeLockType type) {
  DCHECK(CalledOnValidThread());
  // TODO(ejcaruso): Implement.
  VLOG(1) << "OnReleaseDisplayWakeLock";
}

void ArcBridgeServiceImpl::OnArcAvailable(bool arc_available) {
  DCHECK(CalledOnValidThread());
  if (available() == arc_available)
    return;
  SetAvailable(arc_available);
  PrerequisitesChanged();
}

void ArcBridgeServiceImpl::OnConnectionEstablished(
    ArcBridgeInstancePtr instance) {
  DCHECK(CalledOnValidThread());
  if (state() != State::CONNECTING) {
    VLOG(1) << "StopInstance() called while connecting";
    return;
  }

  instance_ptr_ = std::move(instance);

  ArcBridgeHostPtr host;
  binding_.Bind(GetProxy(&host));
  instance_ptr_->Init(std::move(host));

  SetState(State::CONNECTED);
}

void ArcBridgeServiceImpl::OnStopped() {
  DCHECK(CalledOnValidThread());
  SetState(State::STOPPED);
}

}  // namespace arc

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_bridge_service_impl.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "base/thread_task_runner_handle.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/arc/common/arc_host_messages.h"
#include "components/arc/common/arc_instance_messages.h"
#include "ipc/ipc_channel.h"

namespace arc {

namespace {

const base::FilePath::CharType kArcBridgeSocketPath[] =
    FILE_PATH_LITERAL("/home/chronos/ArcBridge/bridge.sock");

}  // namespace

ArcBridgeServiceImpl::ArcBridgeServiceImpl(
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& file_task_runner)
    : ipc_task_runner_(ipc_task_runner),
      file_task_runner_(file_task_runner),
      session_started_(false),
      weak_factory_(this) {
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
  DCHECK(origin_task_runner()->RunsTasksOnCurrentThread());
  session_started_ = true;
  PrerequisitesChanged();
}

void ArcBridgeServiceImpl::Shutdown() {
  DCHECK(origin_task_runner()->RunsTasksOnCurrentThread());
  session_started_ = false;
  PrerequisitesChanged();
}

void ArcBridgeServiceImpl::PrerequisitesChanged() {
  DCHECK(origin_task_runner()->RunsTasksOnCurrentThread());
  if (state() == State::STOPPED) {
    if (!available() || !session_started_)
      return;
    SocketConnect(base::FilePath(kArcBridgeSocketPath));
  } else {
    if (available() && session_started_)
      return;
    StopInstance();
  }
}

void ArcBridgeServiceImpl::StopInstance() {
  DCHECK(origin_task_runner()->RunsTasksOnCurrentThread());
  if (state() == State::STOPPED || state() == State::STOPPING) {
    VLOG(1) << "StopInstance() called when ARC is not running";
    return;
  }
  if (state() == State::CONNECTED || state() == State::CONNECTING) {
    // This was stopped before the D-Bus command to start the instance. Just
    // close the socket (if it was opened).
    if (state() == State::CONNECTED) {
      ipc_channel_.reset();
    }
    SetState(State::STOPPED);
    return;
  }

  SetState(State::STOPPING);
  chromeos::SessionManagerClient* session_manager_client =
      chromeos::DBusThreadManager::Get()->GetSessionManagerClient();
  session_manager_client->StopArcInstance(base::Bind(
      &ArcBridgeServiceImpl::OnInstanceStopped, weak_factory_.GetWeakPtr()));
}

bool ArcBridgeServiceImpl::RegisterInputDevice(const std::string& name,
                                               const std::string& device_type,
                                               base::ScopedFD fd) {
  DCHECK(origin_task_runner()->RunsTasksOnCurrentThread());
  if (state() != State::READY) {
    LOG(ERROR) << "Called RegisterInputDevice when the service is not ready";
    return false;
  }
  return ipc_channel_->Send(new ArcInstanceMsg_RegisterInputDevice(
      name, device_type, base::FileDescriptor(fd.Pass())));
}

bool ArcBridgeServiceImpl::RefreshAppList() {
  DCHECK(origin_task_runner()->RunsTasksOnCurrentThread());
  if (state() != State::READY) {
    LOG(ERROR) << "Called RefreshAppList when the service is not ready";
    return false;
  }
  return ipc_channel_->Send(new ArcInstanceMsg_RefreshApps());
}

bool ArcBridgeServiceImpl::LaunchApp(const std::string& package,
                                     const std::string& activity) {
  DCHECK(origin_task_runner()->RunsTasksOnCurrentThread());
  if (state() != State::READY) {
    LOG(ERROR) << "Called LaunchApp when the service is not ready";
    return false;
  }
  return ipc_channel_->Send(new ArcInstanceMsg_LaunchApp(package, activity));
}

bool ArcBridgeServiceImpl::RequestAppIcon(const std::string& package,
                                          const std::string& activity,
                                          ScaleFactor scale_factor) {
  DCHECK(origin_task_runner()->RunsTasksOnCurrentThread());
  if (state() != State::READY) {
    LOG(ERROR) << "Called RequestAppIcon when the service is not ready";
    return false;
  }
  return ipc_channel_->Send(new ArcInstanceMsg_RequestAppIcon(package,
                                                              activity,
                                                              scale_factor));
}

void ArcBridgeServiceImpl::SocketConnect(const base::FilePath& socket_path) {
  DCHECK(origin_task_runner()->RunsTasksOnCurrentThread());
  if (state() != State::STOPPED) {
    VLOG(1) << "SocketConnect() called when instance is not stopped";
    return;
  }
  SetState(State::CONNECTING);
  base::PostTaskAndReplyWithResult(
      file_task_runner_.get(), FROM_HERE,
      base::Bind(&base::CreateDirectory, socket_path.DirName()),
      base::Bind(&ArcBridgeServiceImpl::SocketConnectAfterEnsureParentDirectory,
                 weak_factory_.GetWeakPtr(), socket_path));
}

void ArcBridgeServiceImpl::SocketConnectAfterEnsureParentDirectory(
    const base::FilePath& socket_path,
    bool directory_present) {
  DCHECK(origin_task_runner()->RunsTasksOnCurrentThread());
  if (state() != State::CONNECTING) {
    VLOG(1) << "StopInstance() called while connecting";
    return;
  }
  if (!directory_present) {
    LOG(ERROR) << "Error creating directory for " << socket_path.value();
    StopInstance();
    return;
  }

  if (!Connect(IPC::ChannelHandle(socket_path.value()),
               IPC::Channel::MODE_OPEN_NAMED_SERVER)) {
    LOG(ERROR) << "Error connecting to " << socket_path.value();
    StopInstance();
    return;
  }

  base::PostTaskAndReplyWithResult(
      file_task_runner_.get(), FROM_HERE,
      // TODO(lhchavez): Tighten the security around the socket by tying it to
      // the user the instance will run as.
      base::Bind(&base::SetPosixFilePermissions, socket_path, 0777),
      base::Bind(&ArcBridgeServiceImpl::SocketConnectAfterSetSocketPermissions,
                 weak_factory_.GetWeakPtr(), socket_path));
}

bool ArcBridgeServiceImpl::Connect(const IPC::ChannelHandle& handle,
                                   IPC::Channel::Mode mode) {
  DCHECK(origin_task_runner()->RunsTasksOnCurrentThread());
  if (state() != State::CONNECTING) {
    VLOG(1) << "StopInstance() called while connecting";
    return false;
  }

  ipc_channel_ =
      IPC::ChannelProxy::Create(handle, mode, this, ipc_task_runner_.get());
  if (!ipc_channel_)
    return false;
  SetState(State::CONNECTED);
  return true;
}

void ArcBridgeServiceImpl::SocketConnectAfterSetSocketPermissions(
    const base::FilePath& socket_path,
    bool socket_permissions_success) {
  DCHECK(origin_task_runner()->RunsTasksOnCurrentThread());
  if (state() != State::CONNECTED) {
    VLOG(1) << "StopInstance() called while connecting";
    return;
  }

  if (!socket_permissions_success) {
    LOG(ERROR) << "Error setting socket permissions for "
               << socket_path.value();
    StopInstance();
    return;
  }

  SetState(State::STARTING);
  chromeos::SessionManagerClient* session_manager_client =
      chromeos::DBusThreadManager::Get()->GetSessionManagerClient();
  session_manager_client->StartArcInstance(
      socket_path.value(), base::Bind(&ArcBridgeServiceImpl::OnInstanceStarted,
                                      weak_factory_.GetWeakPtr()));
}

void ArcBridgeServiceImpl::OnInstanceStarted(bool success) {
  DCHECK(origin_task_runner()->RunsTasksOnCurrentThread());
  if (state() != State::STARTING) {
    VLOG(1) << "StopInstance() called while connecting";
    return;
  }
  if (!success) {
    LOG(ERROR) << "ARC instance unable to start. Shutting down the bridge";
    StopInstance();
    return;
  }
}

void ArcBridgeServiceImpl::OnInstanceBootPhase(InstanceBootPhase phase) {
  DCHECK(origin_task_runner()->RunsTasksOnCurrentThread());
  // The state can be STARTING the first time this is called, and will then
  // transition to READY after BRIDGE_READY has been passed.
  if (state() != State::STARTING && state() != State::READY) {
    VLOG(1) << "StopInstance() called while connecting";
    return;
  }
  if (phase == InstanceBootPhase::BRIDGE_READY) {
    SetState(State::READY);
  }
  FOR_EACH_OBSERVER(Observer, observer_list(), OnInstanceBootPhase(phase));
}

bool ArcBridgeServiceImpl::OnMessageReceived(const IPC::Message& message) {
  DCHECK(origin_task_runner()->RunsTasksOnCurrentThread());
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(ArcBridgeServiceImpl, message)
    IPC_MESSAGE_HANDLER(ArcInstanceHostMsg_InstanceBootPhase,
                        OnInstanceBootPhase)
    IPC_MESSAGE_HANDLER(ArcInstanceHostMsg_AppListRefreshed, OnAppListRefreshed)
    IPC_MESSAGE_HANDLER(ArcInstanceHostMsg_AppIcon, OnAppIcon)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (!handled)
    LOG(ERROR) << "Invalid message with type = " << message.type();
  return handled;
}

void ArcBridgeServiceImpl::OnAppListRefreshed(
    const std::vector<arc::AppInfo>& apps) {
  DCHECK(origin_task_runner()->RunsTasksOnCurrentThread());
  FOR_EACH_OBSERVER(AppObserver, app_observer_list(), OnAppListRefreshed(apps));
}

void ArcBridgeServiceImpl::OnAppIcon(
    const std::string& package,
    const std::string& activity,
    ScaleFactor scale_factor,
    const std::vector<uint8_t>& icon_png_data) {
  DCHECK(origin_task_runner()->RunsTasksOnCurrentThread());
  FOR_EACH_OBSERVER(AppObserver,
                    app_observer_list(),
                    OnAppIcon(package, activity, scale_factor, icon_png_data));
}

void ArcBridgeServiceImpl::OnArcAvailable(bool arc_available) {
  DCHECK(origin_task_runner()->RunsTasksOnCurrentThread());
  if (available() == arc_available)
    return;
  SetAvailable(arc_available);
  PrerequisitesChanged();
}

void ArcBridgeServiceImpl::OnInstanceStopped(bool success) {
  DCHECK(origin_task_runner()->RunsTasksOnCurrentThread());
  // STOPPING is the only valid state for this function.
  // DCHECK on enum classes not supported.
  DCHECK(state() == State::STOPPING);
  ipc_channel_.reset();
  SetState(State::STOPPED);
}

}  // namespace arc

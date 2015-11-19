// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_bridge_service.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/task_runner_util.h"
#include "base/thread_task_runner_handle.h"
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

// Weak pointer.  This class is owned by ChromeBrowserMainPartsChromeos.
ArcBridgeService* g_arc_bridge_service = nullptr;

}  // namespace

ArcBridgeService::ArcBridgeService(
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& file_task_runner)
    : origin_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      ipc_task_runner_(ipc_task_runner),
      file_task_runner_(file_task_runner),
      session_started_(false),
      available_(false),
      state_(State::STOPPED),
      weak_factory_(this) {
  DCHECK(!g_arc_bridge_service);
  g_arc_bridge_service = this;
}

ArcBridgeService::~ArcBridgeService() {
  DCHECK(origin_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(state_ == State::STOPPING || state_ == State::STOPPED);
  DCHECK(g_arc_bridge_service);
  g_arc_bridge_service = nullptr;
}

// static
ArcBridgeService* ArcBridgeService::Get() {
  DCHECK(g_arc_bridge_service);
  DCHECK(g_arc_bridge_service->origin_task_runner_->RunsTasksOnCurrentThread());
  return g_arc_bridge_service;
}

void ArcBridgeService::DetectAvailability() {
  chromeos::SessionManagerClient* session_manager_client =
      chromeos::DBusThreadManager::Get()->GetSessionManagerClient();
  session_manager_client->CheckArcAvailability(base::Bind(
      &ArcBridgeService::OnArcAvailable, weak_factory_.GetWeakPtr()));
}

void ArcBridgeService::HandleStartup() {
  DCHECK(origin_task_runner_->RunsTasksOnCurrentThread());
  session_started_ = true;
  PrerequisitesChanged();
}

void ArcBridgeService::Shutdown() {
  DCHECK(origin_task_runner_->RunsTasksOnCurrentThread());
  session_started_ = false;
  PrerequisitesChanged();
}

void ArcBridgeService::AddObserver(Observer* observer) {
  DCHECK(origin_task_runner_->RunsTasksOnCurrentThread());
  observer_list_.AddObserver(observer);
}

void ArcBridgeService::RemoveObserver(Observer* observer) {
  DCHECK(origin_task_runner_->RunsTasksOnCurrentThread());
  observer_list_.RemoveObserver(observer);
}

void ArcBridgeService::PrerequisitesChanged() {
  DCHECK(origin_task_runner_->RunsTasksOnCurrentThread());
  if (state_ == State::STOPPED) {
    if (!available_ || !session_started_)
      return;
    SocketConnect(base::FilePath(kArcBridgeSocketPath));
  } else {
    if (available_ && session_started_)
      return;
    StopInstance();
  }
}

void ArcBridgeService::StopInstance() {
  DCHECK(origin_task_runner_->RunsTasksOnCurrentThread());
  if (state_ == State::STOPPED || state_ == State::STOPPING) {
    VLOG(1) << "StopInstance() called when ARC is not running";
    return;
  }
  if (state_ == State::CONNECTED || state_ == State::CONNECTING) {
    // This was stopped before the D-Bus command to start the instance. Just
    // close the socket (if it was opened).
    if (state_ == State::CONNECTED) {
      ipc_channel_.reset();
    }
    SetState(State::STOPPED);
    return;
  }

  SetState(State::STOPPING);
  chromeos::SessionManagerClient* session_manager_client =
      chromeos::DBusThreadManager::Get()->GetSessionManagerClient();
  session_manager_client->StopArcInstance(base::Bind(
      &ArcBridgeService::OnInstanceStopped, weak_factory_.GetWeakPtr()));
}

bool ArcBridgeService::RegisterInputDevice(const std::string& name,
                                           const std::string& device_type,
                                           base::ScopedFD fd) {
  DCHECK(origin_task_runner_->RunsTasksOnCurrentThread());
  if (state_ != State::READY) {
    LOG(ERROR) << "Called RegisterInputDevice when the service is not ready";
    return false;
  }
  return ipc_channel_->Send(new ArcInstanceMsg_RegisterInputDevice(
      name, device_type, base::FileDescriptor(fd.Pass())));
}

void ArcBridgeService::SocketConnect(const base::FilePath& socket_path) {
  DCHECK(origin_task_runner_->RunsTasksOnCurrentThread());
  if (state_ != State::STOPPED) {
    VLOG(1) << "SocketConnect() called when instance is not stopped";
    return;
  }
  SetState(State::CONNECTING);
  base::PostTaskAndReplyWithResult(
      file_task_runner_.get(), FROM_HERE,
      base::Bind(&base::CreateDirectory, socket_path.DirName()),
      base::Bind(&ArcBridgeService::SocketConnectAfterEnsureParentDirectory,
                 weak_factory_.GetWeakPtr(), socket_path));
}

void ArcBridgeService::SocketConnectAfterEnsureParentDirectory(
    const base::FilePath& socket_path,
    bool directory_present) {
  DCHECK(origin_task_runner_->RunsTasksOnCurrentThread());
  if (state_ != State::CONNECTING) {
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
      base::Bind(&ArcBridgeService::SocketConnectAfterSetSocketPermissions,
                 weak_factory_.GetWeakPtr(), socket_path));
}

bool ArcBridgeService::Connect(const IPC::ChannelHandle& handle,
                               IPC::Channel::Mode mode) {
  DCHECK(origin_task_runner_->RunsTasksOnCurrentThread());
  if (state_ != State::CONNECTING) {
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

void ArcBridgeService::SocketConnectAfterSetSocketPermissions(
    const base::FilePath& socket_path,
    bool socket_permissions_success) {
  DCHECK(origin_task_runner_->RunsTasksOnCurrentThread());
  if (state_ != State::CONNECTED) {
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
      socket_path.value(), base::Bind(&ArcBridgeService::OnInstanceStarted,
                                      weak_factory_.GetWeakPtr()));
}

void ArcBridgeService::OnInstanceStarted(bool success) {
  DCHECK(origin_task_runner_->RunsTasksOnCurrentThread());
  if (state_ != State::STARTING) {
    VLOG(1) << "StopInstance() called while connecting";
    return;
  }
  if (!success) {
    LOG(ERROR) << "ARC instance unable to start. Shutting down the bridge";
    StopInstance();
    return;
  }
}

void ArcBridgeService::OnInstanceReady() {
  DCHECK(origin_task_runner_->RunsTasksOnCurrentThread());
  if (state_ != State::STARTING) {
    VLOG(1) << "StopInstance() called while connecting";
    return;
  }
  SetState(State::READY);
}

void ArcBridgeService::SetState(State state) {
  DCHECK(origin_task_runner_->RunsTasksOnCurrentThread());
  // DCHECK on enum classes not supported.
  DCHECK(state_ != state);
  state_ = state;
  FOR_EACH_OBSERVER(Observer, observer_list_, OnStateChanged(state_));
}

bool ArcBridgeService::OnMessageReceived(const IPC::Message& message) {
  DCHECK(origin_task_runner_->RunsTasksOnCurrentThread());
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(ArcBridgeService, message)
    IPC_MESSAGE_HANDLER(ArcInstanceHostMsg_InstanceReady, OnInstanceReady)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (!handled)
    LOG(ERROR) << "Invalid message with type = " << message.type();
  return handled;
}

void ArcBridgeService::OnArcAvailable(bool available) {
  DCHECK(origin_task_runner_->RunsTasksOnCurrentThread());
  if (available_ == available)
    return;
  available_ = available;
  FOR_EACH_OBSERVER(Observer, observer_list_, OnAvailableChanged(available_));
  PrerequisitesChanged();
}

void ArcBridgeService::OnInstanceStopped(bool success) {
  DCHECK(origin_task_runner_->RunsTasksOnCurrentThread());
  // STOPPING is the only valid state for this function.
  // DCHECK on enum classes not supported.
  DCHECK(state_ == State::STOPPING);
  ipc_channel_.reset();
  SetState(State::STOPPED);
}

}  // namespace arc

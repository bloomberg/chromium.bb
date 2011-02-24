// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/service_process_util.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "base/win/object_watcher.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "chrome/common/chrome_switches.h"

namespace {

string16 GetServiceProcessReadyEventName() {
  return UTF8ToWide(
      GetServiceProcessScopedVersionedName("_service_ready"));
}

string16 GetServiceProcessShutdownEventName() {
  return UTF8ToWide(
      GetServiceProcessScopedVersionedName("_service_shutdown_evt"));
}

std::string GetServiceProcessAutoRunKey() {
  return GetServiceProcessScopedName("_service_run");
}

class ServiceProcessShutdownMonitor
    : public base::win::ObjectWatcher::Delegate {
 public:
  explicit ServiceProcessShutdownMonitor(Task* shutdown_task)
      : shutdown_task_(shutdown_task) {
  }
  void Start() {
    string16 event_name = GetServiceProcessShutdownEventName();
    CHECK(event_name.length() <= MAX_PATH);
    shutdown_event_.Set(CreateEvent(NULL, TRUE, FALSE, event_name.c_str()));
    watcher_.StartWatching(shutdown_event_.Get(), this);
  }

  // base::ObjectWatcher::Delegate implementation.
  virtual void OnObjectSignaled(HANDLE object) {
    shutdown_task_->Run();
    shutdown_task_.reset();
  }

 private:
  base::win::ScopedHandle shutdown_event_;
  base::win::ObjectWatcher watcher_;
  scoped_ptr<Task> shutdown_task_;
};

}  // namespace

bool ForceServiceProcessShutdown(const std::string& version,
                                 base::ProcessId process_id) {
  base::win::ScopedHandle shutdown_event;
  std::string versioned_name = version;
  versioned_name.append("_service_shutdown_evt");
  string16 event_name =
      UTF8ToWide(GetServiceProcessScopedName(versioned_name));
  shutdown_event.Set(OpenEvent(EVENT_MODIFY_STATE, FALSE, event_name.c_str()));
  if (!shutdown_event.IsValid())
    return false;
  SetEvent(shutdown_event.Get());
  return true;
}

bool CheckServiceProcessReady() {
  string16 event_name = GetServiceProcessReadyEventName();
  base::win::ScopedHandle event(
      OpenEvent(SYNCHRONIZE | READ_CONTROL, false, event_name.c_str()));
  if (!event.IsValid())
    return false;
  // Check if the event is signaled.
  return WaitForSingleObject(event, 0) == WAIT_OBJECT_0;
}

struct ServiceProcessState::StateData {
  // An event that is signaled when a service process is ready.
  base::win::ScopedHandle ready_event;
  scoped_ptr<ServiceProcessShutdownMonitor> shutdown_monitor;
};

bool ServiceProcessState::InitializeState() {
  DCHECK(!state_);
  state_ = new StateData;
  return true;
}

bool ServiceProcessState::TakeSingletonLock() {
  DCHECK(state_);
  string16 event_name = GetServiceProcessReadyEventName();
  CHECK(event_name.length() <= MAX_PATH);
  base::win::ScopedHandle service_process_ready_event;
  service_process_ready_event.Set(
      CreateEvent(NULL, TRUE, FALSE, event_name.c_str()));
  DWORD error = GetLastError();
  if ((error == ERROR_ALREADY_EXISTS) || (error == ERROR_ACCESS_DENIED))
    return false;
  DCHECK(service_process_ready_event.IsValid());
  state_->ready_event.Set(service_process_ready_event.Take());
  return true;
}

bool ServiceProcessState::SignalReady(
    base::MessageLoopProxy* message_loop_proxy, Task* shutdown_task) {
  DCHECK(state_);
  DCHECK(state_->ready_event.IsValid());
  if (!SetEvent(state_->ready_event.Get())) {
    return false;
  }
  if (shutdown_task) {
    state_->shutdown_monitor.reset(
        new ServiceProcessShutdownMonitor(shutdown_task));
    state_->shutdown_monitor->Start();
  }
  return true;
}

bool ServiceProcessState::AddToAutoRun(CommandLine* cmd_line) {
  return base::win::AddCommandToAutoRun(
      HKEY_CURRENT_USER,
      UTF8ToWide(GetServiceProcessAutoRunKey()),
      cmd_line->command_line_string());
}

bool ServiceProcessState::RemoveFromAutoRun() {
  return base::win::RemoveCommandFromAutoRun(
      HKEY_CURRENT_USER, UTF8ToWide(GetServiceProcessAutoRunKey()));
}

void ServiceProcessState::TearDownState() {
  delete state_;
  state_ = NULL;
}

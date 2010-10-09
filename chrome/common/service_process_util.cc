// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/shared_memory.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/service_process_util.h"
#include "chrome/installer/util/version.h"

#if defined(OS_WIN)
#include "base/object_watcher.h"
#include "base/scoped_handle_win.h"
#endif

// TODO(hclam): Split this file for different platforms.

namespace {

// This should be more than enough to hold a version string assuming each part
// of the version string is an int64.
const uint32 kMaxVersionStringLength = 256;

// Return a name that is scoped to this instance of the service process. We
// use the user-data-dir as a scoping prefix.
std::string GetServiceProcessScopedName(const std::string& append_str) {
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
#if defined(OS_WIN)
  std::string scoped_name = WideToUTF8(user_data_dir.value());
#elif defined(OS_POSIX)
  std::string scoped_name = user_data_dir.value();
#endif  // defined(OS_WIN)
  std::replace(scoped_name.begin(), scoped_name.end(), '\\', '!');
  scoped_name.append(append_str);
  return scoped_name;
}

// Return a name that is scoped to this instance of the service process. We
// use the user-data-dir and the version as a scoping prefix.
std::string GetServiceProcessScopedVersionedName(
    const std::string& append_str) {
  std::string versioned_str;
  chrome::VersionInfo version_info;
  DCHECK(version_info.is_valid());
  versioned_str.append(version_info.Version());
  versioned_str.append(append_str);
  return GetServiceProcessScopedName(versioned_str);
}

// Gets the name of the shared memory used by the service process to write its
// version. The name is not versioned.
std::string GetServiceProcessSharedMemName() {
  return GetServiceProcessScopedName("_service_shmem");
}

// Gets the name of the lock file for service process.
FilePath GetServiceProcessLockFilePath() {
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  chrome::VersionInfo version_info;
#if defined(OS_WIN)
  user_data_dir.Append(ASCIIToWide(version_info.Version()));
#elif defined(OS_POSIX)
  user_data_dir.Append(version_info.Version());
#endif  // defined(OS_WIN)
  return user_data_dir.Append(FILE_PATH_LITERAL("Service Process Lock"));
}

enum ServiceProcessRunningState {
  SERVICE_NOT_RUNNING,
  SERVICE_OLDER_VERSION_RUNNING,
  SERVICE_SAME_VERSION_RUNNING,
  SERVICE_NEWER_VERSION_RUNNING,
};

ServiceProcessRunningState GetServiceProcessRunningState(
    std::string* service_version_out) {
  std::string version;
  scoped_ptr<base::SharedMemory> shared_mem_version;
  shared_mem_version.reset(new base::SharedMemory());
  if (shared_mem_version.get() &&
      shared_mem_version->Open(GetServiceProcessSharedMemName(), true) &&
      shared_mem_version->Map(kMaxVersionStringLength) &&
      memchr(shared_mem_version->memory(), '\0', kMaxVersionStringLength)) {
    version =
        static_cast<const char *>(shared_mem_version->memory());
  } else {
    return SERVICE_NOT_RUNNING;
  }
  // At this time we have a version string. Set the out param if it exists.
  if (service_version_out)
    *service_version_out = version;

  scoped_ptr<installer::Version> service_version;
  service_version.reset(
      installer::Version::GetVersionFromString(ASCIIToUTF16(version)));
  // If the version string is invalid, treat it like an older version.
  if (!service_version.get())
    return SERVICE_OLDER_VERSION_RUNNING;

  // Get the version of the currently *running* instance of Chrome.
  chrome::VersionInfo version_info;
  if (!version_info.is_valid()) {
    NOTREACHED() << "Failed to get current file version";
    // Our own version is invalid. This is an error case. Pretend that we
    // are out of date.
    return SERVICE_NEWER_VERSION_RUNNING;
  }
  scoped_ptr<installer::Version> running_version(
      installer::Version::GetVersionFromString(
          ASCIIToUTF16(version_info.Version())));
  if (!running_version.get()) {
    NOTREACHED() << "Failed to parse version info";
    // Our own version is invalid. This is an error case. Pretend that we
    // are out of date.
    return SERVICE_NEWER_VERSION_RUNNING;
  }

  if (running_version->IsHigherThan(service_version.get())) {
    return SERVICE_OLDER_VERSION_RUNNING;
  } else if (service_version->IsHigherThan(running_version.get())) {
    return SERVICE_NEWER_VERSION_RUNNING;
  }
  return SERVICE_SAME_VERSION_RUNNING;
}

#if defined(OS_WIN)
string16 GetServiceProcessEventName() {
  return UTF8ToWide(
      GetServiceProcessScopedVersionedName("Service Process Lock"));
}

string16 GetServiceProcessShutdownEventName() {
  return UTF8ToWide(
      GetServiceProcessScopedVersionedName("_service_shutdown_evt"));
}

class ServiceProcessShutdownMonitor : public base::ObjectWatcher::Delegate {
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
  ScopedHandle shutdown_event_;
  base::ObjectWatcher watcher_;
  scoped_ptr<Task> shutdown_task_;
};

#endif  // defined(OS_WIN)

struct ServiceProcessGlobalState {
  scoped_ptr<base::SharedMemory> shared_mem_version;
#if defined(OS_WIN)
  // An event that is signaled when a service process is running.
  ScopedHandle running_event;
  scoped_ptr<ServiceProcessShutdownMonitor> shutdown_monitor;
#endif  // defined(OS_WIN)
};

// TODO(sanjeevr): Remove this ugly global pointer and move all methods and
// data members used by the service process into one singleton class which
// could be owned by the service process.
ServiceProcessGlobalState* g_service_globals = NULL;

}  // namespace

// Gets the name of the service process IPC channel.
std::string GetServiceProcessChannelName() {
  return GetServiceProcessScopedVersionedName("_service_ipc");
}

std::string GetServiceProcessAutoRunKey() {
  return GetServiceProcessScopedName("_service_run");
}

bool TakeServiceProcessSingletonLock() {
  DCHECK(g_service_globals == NULL);
  std::string running_version;
  ServiceProcessRunningState state =
      GetServiceProcessRunningState(&running_version);
  switch (state) {
    case SERVICE_SAME_VERSION_RUNNING:
    case SERVICE_NEWER_VERSION_RUNNING:
      return false;
    case SERVICE_OLDER_VERSION_RUNNING:
      // If an older version is running, kill it.
      ForceServiceProcessShutdown(running_version);
      break;
    case SERVICE_NOT_RUNNING:
      break;
  }
  g_service_globals = new ServiceProcessGlobalState;
  // TODO(sanjeevr): We can probably use the shared mem as the sole singleton
  // mechanism. For that the shared mem class needs to return whether it created
  // new instance or opened an existing one.
#if defined(OS_WIN)
  string16 event_name = GetServiceProcessEventName();
  CHECK(event_name.length() <= MAX_PATH);
  ScopedHandle service_process_running_event;
  service_process_running_event.Set(
      CreateEvent(NULL, TRUE, FALSE, event_name.c_str()));
  DWORD error = GetLastError();
  if ((error == ERROR_ALREADY_EXISTS) || (error == ERROR_ACCESS_DENIED)) {
    delete g_service_globals;
    g_service_globals = NULL;
    return false;
  }
  DCHECK(service_process_running_event.IsValid());
  g_service_globals->running_event.Set(service_process_running_event.Take());
#else
  // TODO(sanjeevr): Implement singleton mechanism for other platforms.
  NOTIMPLEMENTED();
#endif
  // Now that we have the singleton, let is also write the version we are using
  // to shared memory. This can be used by a newer service to signal us to exit.
  chrome::VersionInfo version_info;
  if (!version_info.is_valid()) {
    NOTREACHED() << "Failed to get current file version";
    return false;
  }
  if (version_info.Version().length() >= kMaxVersionStringLength) {
    NOTREACHED() << "Version string length is << " <<
        version_info.Version().length() << "which is longer than" <<
        kMaxVersionStringLength;
    return false;
  }

  scoped_ptr<base::SharedMemory> shared_mem_version;
  shared_mem_version.reset(new base::SharedMemory());
  if (!shared_mem_version.get())
    return false;

  uint32 alloc_size = kMaxVersionStringLength;
  if (!shared_mem_version->Create(GetServiceProcessSharedMemName(), false,
                                  true, alloc_size))
    return false;

  if (!shared_mem_version->Map(alloc_size))
    return false;

  memset(shared_mem_version->memory(), 0, alloc_size);
  memcpy(shared_mem_version->memory(), version_info.Version().c_str(),
         version_info.Version().length());
  g_service_globals->shared_mem_version.reset(shared_mem_version.release());
  return true;
}

void SignalServiceProcessRunning(Task* shutdown_task) {
#if defined(OS_WIN)
  DCHECK(g_service_globals != NULL);
  DCHECK(g_service_globals->running_event.IsValid());
  SetEvent(g_service_globals->running_event.Get());
  g_service_globals->shutdown_monitor.reset(
      new ServiceProcessShutdownMonitor(shutdown_task));
  g_service_globals->shutdown_monitor->Start();
#else
  // TODO(hclam): Implement better mechanism for these platform.
  // Also we need to save shutdown task. For now we just delete the shutdown
  // task because we have not way to listen for shutdown requests.
  delete shutdown_task;
  const FilePath path = GetServiceProcessLockFilePath();
  FILE* file = file_util::OpenFile(path, "wb+");
  if (!file)
    return;
  LOG(INFO) << "Created Service Process lock file: " << path.value();
  file_util::TruncateFile(file) && file_util::CloseFile(file);
#endif
}

void SignalServiceProcessStopped() {
  delete g_service_globals;
  g_service_globals = NULL;
#if !defined(OS_WIN)
  // TODO(hclam): Implement better mechanism for these platform.
  const FilePath path = GetServiceProcessLockFilePath();
  file_util::Delete(path, false);
#endif
}

bool ForceServiceProcessShutdown(const std::string& version) {
#if defined(OS_WIN)
  ScopedHandle shutdown_event;
  std::string versioned_name = version;
  versioned_name.append("_service_shutdown_evt");
  string16 event_name =
      UTF8ToWide(GetServiceProcessScopedName(versioned_name));
  shutdown_event.Set(OpenEvent(EVENT_MODIFY_STATE, FALSE, event_name.c_str()));
  if (!shutdown_event.IsValid())
    return false;
  SetEvent(shutdown_event.Get());
  return true;
#else  // defined(OS_WIN)
  NOTIMPLEMENTED();
  return false;
#endif    // defined(OS_WIN)
}

bool CheckServiceProcessRunning() {
#if defined(OS_WIN)
  string16 event_name = GetServiceProcessEventName();
  ScopedHandle event(
      OpenEvent(SYNCHRONIZE | READ_CONTROL, false, event_name.c_str()));
  if (!event.IsValid())
    return false;
  // Check if the event is signaled.
  return WaitForSingleObject(event, 0) == WAIT_OBJECT_0;
#else
  // TODO(hclam): Implement better mechanism for these platform.
  const FilePath path = GetServiceProcessLockFilePath();
  return file_util::PathExists(path);
#endif
}


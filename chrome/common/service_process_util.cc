// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/service_process_util.h"

#if defined(OS_WIN)
#include "base/scoped_handle_win.h"
#endif

// TODO(hclam): Split this file for different platforms.
std::string GetServiceProcessChannelName() {
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);

  // TODO(sanjeevr): We need to actually figure out the right way to determine
  // a channel name. The below is to facilitate testing only.
#if defined(OS_WIN)
  std::string channel_name = WideToUTF8(user_data_dir.value());
#elif defined(OS_POSIX)
  std::string channel_name = user_data_dir.value();
#endif  // defined(OS_WIN)
  std::replace(channel_name.begin(), channel_name.end(), '\\', '!');
  channel_name.append("_service_ipc");
  return channel_name;
}

// Gets the name of the lock file for service process.
static FilePath GetServiceProcessLockFilePath(ServiceProcessType type) {
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  return user_data_dir.Append(FILE_PATH_LITERAL("Service Process Lock"));
}

#if defined(OS_WIN)
static std::wstring GetServiceProcessEventName(
    ServiceProcessType type) {
  FilePath path = GetServiceProcessLockFilePath(type);
  std::wstring event_name = path.value();
  std::replace(event_name.begin(), event_name.end(), '\\', '!');
  return event_name;
}

// An event that is signaled when a service process is running. This
// variable is used only in the service process.
static ScopedHandle service_process_running_event;
#endif

void SignalServiceProcessRunning(ServiceProcessType type) {
#if defined(OS_WIN)
  std::wstring event_name = GetServiceProcessEventName(type);
  service_process_running_event.Set(
      CreateEvent(NULL, true, true, event_name.c_str()));
  DCHECK(service_process_running_event.IsValid());
#else
  // TODO(hclam): Implement better mechanism for these platform.
  const FilePath path = GetServiceProcessLockFilePath(type);
  FILE* file = file_util::OpenFile(path, "wb+");
  if (!file)
    return;
  LOG(INFO) << "Created Service Process lock file: " << path.value();
  file_util::TruncateFile(file) && file_util::CloseFile(file);
#endif
}

void SignalServiceProcessStopped(ServiceProcessType type) {
#if defined(OS_WIN)
  // Close the handle to the event.
  service_process_running_event.Close();
#else
  // TODO(hclam): Implement better mechanism for these platform.
  const FilePath path = GetServiceProcessLockFilePath(type);
  file_util::Delete(path, false);
#endif
}

bool CheckServiceProcessRunning(ServiceProcessType type) {
#if defined(OS_WIN)
  std::wstring event_name = GetServiceProcessEventName(type);
  ScopedHandle event(
      OpenEvent(SYNCHRONIZE | READ_CONTROL, false, event_name.c_str()));
  if (!event.IsValid())
    return false;
  // Check if the event is signaled.
  return WaitForSingleObject(event, 0) == WAIT_OBJECT_0;
#else
  // TODO(hclam): Implement better mechanism for these platform.
  const FilePath path = GetServiceProcessLockFilePath(type);
  return file_util::PathExists(path);
#endif
}

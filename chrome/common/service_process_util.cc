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
static FilePath GetServiceProcessLockFilePath() {
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  return user_data_dir.Append(FILE_PATH_LITERAL("Service Process Lock"));
}

#if defined(OS_WIN)
static std::wstring GetServiceProcessEventName() {
  FilePath path = GetServiceProcessLockFilePath();
  std::wstring event_name = path.value();
  std::replace(event_name.begin(), event_name.end(), '\\', '!');
  return event_name;
}

// An event that is signaled when a service process is running. This
// variable is used only in the service process.
static HANDLE g_service_process_running_event;
#endif

bool TakeServiceProcessSingletonLock() {
#if defined(OS_WIN)
  std::wstring event_name = GetServiceProcessEventName();
  DCHECK(g_service_process_running_event == NULL);
  ScopedHandle service_process_running_event;
  service_process_running_event.Set(
      CreateEvent(NULL, TRUE, FALSE, event_name.c_str()));
  DWORD error = GetLastError();
  if ((error == ERROR_ALREADY_EXISTS) || (error == ERROR_ACCESS_DENIED))
    return false;
  DCHECK(service_process_running_event.IsValid());
  g_service_process_running_event = service_process_running_event.Take();
#endif
  // TODO(sanjeevr): Implement singleton mechanism for other platforms.
  return true;
}

void SignalServiceProcessRunning() {
#if defined(OS_WIN)
  DCHECK(g_service_process_running_event != NULL);
  SetEvent(g_service_process_running_event);
#else
  // TODO(hclam): Implement better mechanism for these platform.
  const FilePath path = GetServiceProcessLockFilePath();
  FILE* file = file_util::OpenFile(path, "wb+");
  if (!file)
    return;
  LOG(INFO) << "Created Service Process lock file: " << path.value();
  file_util::TruncateFile(file) && file_util::CloseFile(file);
#endif
}

void SignalServiceProcessStopped() {
#if defined(OS_WIN)
  // Close the handle to the event.
  CloseHandle(g_service_process_running_event);
  g_service_process_running_event = NULL;
#else
  // TODO(hclam): Implement better mechanism for these platform.
  const FilePath path = GetServiceProcessLockFilePath();
  file_util::Delete(path, false);
#endif
}

bool CheckServiceProcessRunning() {
#if defined(OS_WIN)
  std::wstring event_name = GetServiceProcessEventName();
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

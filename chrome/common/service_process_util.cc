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

// TODO(hclam): |type| is not used at all. Different types of service process
// should have a different instance of process and channel.
std::string GetServiceProcessChannelName(ServiceProcessType type) {
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
  return user_data_dir.Append(FILE_PATH_LITERAL("Service Process.Lock"));
}

bool CreateServiceProcessLockFile(ServiceProcessType type) {
  const FilePath path = GetServiceProcessLockFilePath(type);
  FILE* file = file_util::OpenFile(path, "wb+");
  if (!file)
    return false;
  LOG(INFO) << "Created Service Process lock file: " << path.value();
  return file_util::TruncateFile(file) && file_util::CloseFile(file);
}

bool DeleteServiceProcessLockFile(ServiceProcessType type) {
  const FilePath path = GetServiceProcessLockFilePath(type);
  return file_util::Delete(path, false);
}

bool CheckServiceProcessRunning(ServiceProcessType type) {
  const FilePath path = GetServiceProcessLockFilePath(type);
  return file_util::PathExists(path);
}

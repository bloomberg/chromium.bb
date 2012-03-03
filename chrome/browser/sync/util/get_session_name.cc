// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/get_session_name.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/message_loop_proxy.h"
#include "base/sys_info.h"

#if defined(OS_LINUX)
#include "base/linux_util.h"
#elif defined(OS_WIN)
#include <windows.h>
#endif

namespace browser_sync {

#if defined(OS_WIN)
namespace internal {

std::string GetComputerName() {
  char computer_name[MAX_COMPUTERNAME_LENGTH + 1];
  DWORD size = sizeof(computer_name);
  if (GetComputerNameA(computer_name, &size))
    return computer_name;
  return std::string();
}

}  // namespace internal
#endif

namespace {

void DoGetSessionNameTask(std::string* session_name) {
#if defined(OS_CHROMEOS)
  *session_name = "Chromebook";
#elif defined(OS_LINUX)
  *session_name = base::GetLinuxDistro();
#elif defined(OS_MACOSX)
  *session_name = internal::GetHardwareModelName();
#elif defined(OS_WIN)
  *session_name = internal::GetComputerName();
#else
  session_name->clear();
#endif

  if (*session_name == "Unknown" || session_name->empty())
    *session_name = base::SysInfo::OperatingSystemName();
}

void GetSessionNameReply(
    const base::Callback<void(const std::string&)>& done_callback,
    std::string* session_name) {
  done_callback.Run(*session_name);
}

}  // namespace

void GetSessionName(
    const scoped_refptr<base::TaskRunner>& task_runner,
    const base::Callback<void(const std::string&)>& done_callback) {
  std::string* session_name = new std::string();
  task_runner->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&DoGetSessionNameTask,
                 base::Unretained(session_name)),
      base::Bind(&GetSessionNameReply,
                 done_callback,
                 base::Owned(session_name)));
}

}  // namespace browser_sync

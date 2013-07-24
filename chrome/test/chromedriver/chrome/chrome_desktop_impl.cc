// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/chrome_desktop_impl.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/kill.h"
#include "base/sys_info.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chrome/test/chromedriver/chrome/automation_extension.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/devtools_http_client.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/web_view_impl.h"

#if defined(OS_POSIX)
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

bool KillProcess(base::ProcessHandle process_id) {
#if defined(OS_POSIX)
  kill(process_id, SIGKILL);
  base::Time deadline = base::Time::Now() + base::TimeDelta::FromSeconds(5);
  while (base::Time::Now() < deadline) {
    pid_t pid = HANDLE_EINTR(waitpid(process_id, NULL, WNOHANG));
    if (pid == process_id)
      return true;
    if (pid == -1) {
      if (errno == ECHILD) {
        // The wait may fail with ECHILD if another process also waited for
        // the same pid, causing the process state to get cleaned up.
        return true;
      }
      LOG(WARNING) << "Error waiting for process " << process_id;
    }
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(50));
  }
  return false;
#endif

#if defined(OS_WIN)
  if (!base::KillProcess(process_id, 0, true)) {
    int exit_code;
    return base::GetTerminationStatus(process_id, &exit_code) !=
        base::TERMINATION_STATUS_STILL_RUNNING;
  }
  return true;
#endif
}

}  // namespace

ChromeDesktopImpl::ChromeDesktopImpl(
    scoped_ptr<DevToolsHttpClient> client,
    ScopedVector<DevToolsEventListener>& devtools_event_listeners,
    Log* log,
    base::ProcessHandle process,
    base::ScopedTempDir* user_data_dir,
    base::ScopedTempDir* extension_dir)
    : ChromeImpl(client.Pass(),
                 devtools_event_listeners,
                 log),
      process_(process),
      quit_(false) {
  if (user_data_dir->IsValid())
    CHECK(user_data_dir_.Set(user_data_dir->Take()));
  if (extension_dir->IsValid())
    CHECK(extension_dir_.Set(extension_dir->Take()));
}

ChromeDesktopImpl::~ChromeDesktopImpl() {
  if (!quit_) {
    base::FilePath user_data_dir = user_data_dir_.Take();
    base::FilePath extension_dir = extension_dir_.Take();
    LOG(WARNING) << "chrome detaches, user should take care of directory:"
                 << user_data_dir.value() << " and " << extension_dir.value();
  }
  base::CloseProcessHandle(process_);
}

Status ChromeDesktopImpl::GetAutomationExtension(
    AutomationExtension** extension) {
  if (!automation_extension_) {
    base::Time deadline = base::Time::Now() + base::TimeDelta::FromSeconds(10);
    std::string id;
    while (base::Time::Now() < deadline) {
      WebViewsInfo views_info;
      Status status = devtools_http_client_->GetWebViewsInfo(&views_info);
      if (status.IsError())
        return status;

      for (size_t i = 0; i < views_info.GetSize(); ++i) {
        if (views_info.Get(i).url.find(
                "chrome-extension://aapnijgdinlhnhlmodcfapnahmbfebeb") == 0) {
          id = views_info.Get(i).id;
          break;
        }
      }
      if (!id.empty())
        break;
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
    }
    if (id.empty())
      return Status(kUnknownError, "automation extension cannot be found");

    scoped_ptr<WebView> web_view(new WebViewImpl(
        id, GetBuildNo(), devtools_http_client_->CreateClient(id), log_));
    Status status = web_view->ConnectIfNecessary();
    if (status.IsError())
      return status;

    // Wait for the extension background page to load.
    status = web_view->WaitForPendingNavigations(
        std::string(), 5 * 60 * 1000);
    if (status.IsError())
      return status;

    automation_extension_.reset(new AutomationExtension(web_view.Pass()));
  }
  *extension = automation_extension_.get();
  return Status(kOk);
}

std::string ChromeDesktopImpl::GetOperatingSystemName() {
  return base::SysInfo::OperatingSystemName();
}

Status ChromeDesktopImpl::Quit() {
  quit_ = true;
  if (!KillProcess(process_))
    return Status(kUnknownError, "cannot kill Chrome");
  return Status(kOk);
}

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/chrome_desktop_impl.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/sys_info.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "chrome/test/chromedriver/chrome/automation_extension.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/devtools_http_client.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/web_view_impl.h"

ChromeDesktopImpl::ChromeDesktopImpl(
    scoped_ptr<DevToolsHttpClient> client,
    const std::string& version,
    int build_no,
    const std::list<DevToolsEventLogger*>& devtools_event_loggers,
    base::ProcessHandle process,
    base::ScopedTempDir* user_data_dir,
    base::ScopedTempDir* extension_dir)
    : ChromeImpl(client.Pass(), version, build_no, devtools_event_loggers),
      process_(process) {
  if (user_data_dir->IsValid())
    CHECK(user_data_dir_.Set(user_data_dir->Take()));
  if (extension_dir->IsValid())
    CHECK(extension_dir_.Set(extension_dir->Take()));
}

ChromeDesktopImpl::~ChromeDesktopImpl() {
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
        id, devtools_http_client_->CreateClient(id)));
    Status status = web_view->ConnectIfNecessary();
    if (status.IsError())
      return status;

    // Wait for the extension background page to load.
    status = web_view->WaitForPendingNavigations(std::string());
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
  if (!base::KillProcess(process_, 0, true)) {
    int exit_code;
    if (base::GetTerminationStatus(process_, &exit_code) ==
        base::TERMINATION_STATUS_STILL_RUNNING)
      return Status(kUnknownError, "cannot kill Chrome");
  }
  return Status(kOk);
}

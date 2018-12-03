// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/web_test/web_test_browser_context.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/push_messaging_service.h"
#include "content/public/browser/resource_context.h"
#include "content/shell/browser/shell_url_request_context_getter.h"
#include "content/shell/browser/web_test/web_test_background_fetch_delegate.h"
#include "content/shell/browser/web_test/web_test_download_manager_delegate.h"
#include "content/shell/browser/web_test/web_test_permission_manager.h"
#include "content/shell/browser/web_test/web_test_push_messaging_service.h"
#include "content/shell/browser/web_test/web_test_url_request_context_getter.h"
#include "content/test/mock_background_sync_controller.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"

#if defined(OS_WIN)
#include "base/base_paths_win.h"
#elif defined(OS_LINUX)
#include "base/nix/xdg_util.h"
#elif defined(OS_MACOSX)
#include "base/base_paths_mac.h"
#include "base/mac/foundation_util.h"
#endif

namespace content {

WebTestBrowserContext::WebTestBrowserContext(bool off_the_record,
                                             net::NetLog* net_log)
    : ShellBrowserContext(off_the_record, net_log) {
  // Overrides geolocation coordinates for testing.
  geolocation_overrider_ =
      std::make_unique<device::ScopedGeolocationOverrider>(0, 0);
}

WebTestBrowserContext::~WebTestBrowserContext() {
  BrowserContext::NotifyWillBeDestroyed(this);
}

ShellURLRequestContextGetter*
WebTestBrowserContext::CreateURLRequestContextGetter(
    ProtocolHandlerMap* protocol_handlers,
    URLRequestInterceptorScopedVector request_interceptors) {
  return new WebTestURLRequestContextGetter(
      ignore_certificate_errors(), IsOffTheRecord(), GetPath(),
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}),
      protocol_handlers, std::move(request_interceptors), net_log());
}

DownloadManagerDelegate* WebTestBrowserContext::GetDownloadManagerDelegate() {
  if (!download_manager_delegate_) {
    download_manager_delegate_.reset(new WebTestDownloadManagerDelegate());
    download_manager_delegate_->SetDownloadManager(
        BrowserContext::GetDownloadManager(this));
    download_manager_delegate_->SetDownloadBehaviorForTesting(
        GetPath().Append(FILE_PATH_LITERAL("downloads")));
  }

  return download_manager_delegate_.get();
}

PushMessagingService* WebTestBrowserContext::GetPushMessagingService() {
  if (!push_messaging_service_) {
    push_messaging_service_ = std::make_unique<WebTestPushMessagingService>();
  }
  return push_messaging_service_.get();
}

PermissionControllerDelegate*
WebTestBrowserContext::GetPermissionControllerDelegate() {
  if (!permission_manager_.get())
    permission_manager_ = std::make_unique<WebTestPermissionManager>();
  return permission_manager_.get();
}

BackgroundFetchDelegate* WebTestBrowserContext::GetBackgroundFetchDelegate() {
  if (!background_fetch_delegate_) {
    background_fetch_delegate_ =
        std::make_unique<WebTestBackgroundFetchDelegate>(this);
  }
  return background_fetch_delegate_.get();
}

BackgroundSyncController* WebTestBrowserContext::GetBackgroundSyncController() {
  if (!background_sync_controller_) {
    background_sync_controller_ =
        std::make_unique<MockBackgroundSyncController>();
  }
  return background_sync_controller_.get();
}

WebTestPermissionManager* WebTestBrowserContext::GetWebTestPermissionManager() {
  return static_cast<WebTestPermissionManager*>(
      GetPermissionControllerDelegate());
}

}  // namespace content

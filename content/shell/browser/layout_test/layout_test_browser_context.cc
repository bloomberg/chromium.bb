// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_browser_context.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/push_messaging_service.h"
#include "content/public/browser/resource_context.h"
#include "content/shell/browser/layout_test/layout_test_download_manager_delegate.h"
#include "content/shell/browser/layout_test/layout_test_permission_manager.h"
#include "content/shell/browser/layout_test/layout_test_push_messaging_service.h"
#include "content/shell/browser/layout_test/layout_test_url_request_context_getter.h"
#include "content/shell/browser/shell_url_request_context_getter.h"
#include "content/test/mock_background_sync_controller.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_file_job.h"

#if defined(OS_WIN)
#include "base/base_paths_win.h"
#elif defined(OS_LINUX)
#include "base/nix/xdg_util.h"
#elif defined(OS_MACOSX)
#include "base/base_paths_mac.h"
#include "base/mac/foundation_util.h"
#endif

namespace content {
namespace {

bool GetBuildDirectory(base::FilePath* result) {
  if (!base::PathService::Get(base::DIR_EXE, result))
    return false;

#if defined(OS_MACOSX)
  if (base::mac::AmIBundled()) {
    // The bundled app executables (Chromium, TestShell, etc) live three
    // levels down from the build directory, eg:
    // Chromium.app/Contents/MacOS/Chromium
    *result = result->DirName().DirName().DirName();
  }
#endif
  return true;
}

class MojomProtocolHandler : public net::URLRequestJobFactory::ProtocolHandler {
  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    base::FilePath prefix;
    GURL url = request->url();
    if (!base::EndsWith(url.path(), ".mojom",
                        base::CompareCase::INSENSITIVE_ASCII)) {
      return nullptr;
    }
    if (!GetBuildDirectory(&prefix))
      return nullptr;

    prefix = prefix.AppendASCII("gen");

    base::FilePath path = prefix.AppendASCII(url.path().substr(2) + ".js");
    if (path.ReferencesParent())
      return nullptr;

    return new net::URLRequestFileJob(
        request, network_delegate, path,
        BrowserThread::GetTaskRunnerForThread(BrowserThread::FILE));
  }
};

}  // namespace

LayoutTestBrowserContext::LayoutTestBrowserContext(bool off_the_record,
                                                   net::NetLog* net_log)
    : ShellBrowserContext(off_the_record, net_log) {
  ignore_certificate_errors_ = true;
}

LayoutTestBrowserContext::~LayoutTestBrowserContext() {
}

ShellURLRequestContextGetter*
LayoutTestBrowserContext::CreateURLRequestContextGetter(
    ProtocolHandlerMap* protocol_handlers,
    URLRequestInterceptorScopedVector request_interceptors) {
  protocol_handlers->insert(std::make_pair(
      "layout-test-mojom", make_linked_ptr(new MojomProtocolHandler)));
  return new LayoutTestURLRequestContextGetter(
      ignore_certificate_errors(), GetPath(),
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO),
      BrowserThread::GetTaskRunnerForThread(BrowserThread::FILE),
      protocol_handlers, std::move(request_interceptors), net_log());
}

DownloadManagerDelegate*
LayoutTestBrowserContext::GetDownloadManagerDelegate() {
  if (!download_manager_delegate_) {
    download_manager_delegate_.reset(new LayoutTestDownloadManagerDelegate());
    download_manager_delegate_->SetDownloadManager(
        BrowserContext::GetDownloadManager(this));
    download_manager_delegate_->SetDownloadBehaviorForTesting(
        GetPath().Append(FILE_PATH_LITERAL("downloads")));
  }

  return download_manager_delegate_.get();
}

PushMessagingService* LayoutTestBrowserContext::GetPushMessagingService() {
  if (!push_messaging_service_)
    push_messaging_service_.reset(new LayoutTestPushMessagingService());
  return push_messaging_service_.get();
}

PermissionManager* LayoutTestBrowserContext::GetPermissionManager() {
  if (!permission_manager_.get())
    permission_manager_.reset(new LayoutTestPermissionManager());
  return permission_manager_.get();
}

BackgroundSyncController*
LayoutTestBrowserContext::GetBackgroundSyncController() {
  if (!background_sync_controller_)
    background_sync_controller_.reset(new MockBackgroundSyncController());
  return background_sync_controller_.get();
}

LayoutTestPermissionManager*
LayoutTestBrowserContext::GetLayoutTestPermissionManager() {
  return static_cast<LayoutTestPermissionManager*>(GetPermissionManager());
}

}  // namespace content

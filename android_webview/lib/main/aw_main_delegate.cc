// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/lib/main/aw_main_delegate.h"

#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/in_process_renderer/in_process_renderer_client.h"
#include "android_webview/browser/scoped_allow_wait_for_legacy_web_view_api.h"
#include "android_webview/common/aw_switches.h"
#include "android_webview/lib/aw_browser_dependency_factory_impl.h"
#include "android_webview/native/aw_geolocation_permission_context.h"
#include "android_webview/native/aw_quota_manager_bridge_impl.h"
#include "android_webview/native/aw_web_contents_view_delegate.h"
#include "android_webview/renderer/aw_content_renderer_client.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "webkit/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

namespace android_webview {

namespace {

// TODO(boliu): Remove these global Allows once the underlying issues
// are resolved. See AwMainDelegate::RunProcess below.

base::LazyInstance<scoped_ptr<ScopedAllowWaitForLegacyWebViewApi> >
    g_allow_wait_in_ui_thread = LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<scoped_ptr<base::ThreadRestrictions::ScopedAllowIO> >
    g_allow_io_in_ui_thread = LAZY_INSTANCE_INITIALIZER;

bool UIAndRendererCompositorThreadsNotMerged() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kNoMergeUIAndRendererCompositorThreads);
}
}

AwMainDelegate::AwMainDelegate() {
}

AwMainDelegate::~AwMainDelegate() {
}

bool AwMainDelegate::BasicStartupComplete(int* exit_code) {
  content::SetContentClient(&content_client_);

  webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl
      ::EnableVirtualizedContext();

  CommandLine* cl = CommandLine::ForCurrentProcess();
  if (UIAndRendererCompositorThreadsNotMerged()) {
    cl->AppendSwitch(switches::kEnableWebViewSynchronousAPIs);
  } else {
    // Set the command line to enable synchronous API compatibility.
    cl->AppendSwitch(switches::kEnableSynchronousRendererCompositor);
  }
  return false;
}

void AwMainDelegate::PreSandboxStartup() {
  // TODO(torne): When we have a separate renderer process, we need to handle
  // being passed open FDs for the resource paks here.
}

void AwMainDelegate::SandboxInitialized(const std::string& process_type) {
  // TODO(torne): Adjust linux OOM score here.
}

int AwMainDelegate::RunProcess(
    const std::string& process_type,
    const content::MainFunctionParams& main_function_params) {
  if (process_type.empty()) {
    AwBrowserDependencyFactoryImpl::InstallInstance();

    browser_runner_.reset(content::BrowserMainRunner::Create());
    int exit_code = browser_runner_->Initialize(main_function_params);
    DCHECK(exit_code < 0);

    if (!UIAndRendererCompositorThreadsNotMerged()) {
      // This is temporary until we remove the browser compositor
      g_allow_wait_in_ui_thread.Get().reset(
          new ScopedAllowWaitForLegacyWebViewApi);

      // TODO(boliu): This is a HUGE hack to work around the fact that
      // cc::WorkerPool joins on worker threads on the UI thread.
      // See crbug.com/239423.
      g_allow_io_in_ui_thread.Get().reset(
          new base::ThreadRestrictions::ScopedAllowIO);
    }

    // Return 0 so that we do NOT trigger the default behavior. On Android, the
    // UI message loop is managed by the Java application.
    return 0;
  }

  return -1;
}

void AwMainDelegate::ProcessExiting(const std::string& process_type) {
  // TODO(torne): Clean up resources when we handle them.

  logging::CloseLogFile();
}

content::ContentBrowserClient*
    AwMainDelegate::CreateContentBrowserClient() {
  content_browser_client_.reset(new AwContentBrowserClient(this));
  return content_browser_client_.get();
}

content::ContentRendererClient*
    AwMainDelegate::CreateContentRendererClient() {
  // None of this makes sense for multiprocess.
  DCHECK(CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess));
  // During transition period allow running in either threading mode; eventually
  // only the compositor/UI thread merge mode will be supported.
  const bool no_merge_threads = UIAndRendererCompositorThreadsNotMerged();
  content_renderer_client_.reset(
      no_merge_threads ? new AwContentRendererClient() :
                         new InProcessRendererClient());
  return content_renderer_client_.get();
}

AwQuotaManagerBridge* AwMainDelegate::CreateAwQuotaManagerBridge(
    AwBrowserContext* browser_context) {
  return new AwQuotaManagerBridgeImpl(browser_context);
}

content::GeolocationPermissionContext*
    AwMainDelegate::CreateGeolocationPermission(
        AwBrowserContext* browser_context) {
  return AwGeolocationPermissionContext::Create(browser_context);
}

content::WebContentsViewDelegate* AwMainDelegate::CreateViewDelegate(
    content::WebContents* web_contents) {
  return AwWebContentsViewDelegate::Create(web_contents);
}

}  // namespace android_webview

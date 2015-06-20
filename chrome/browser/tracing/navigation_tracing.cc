// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/navigation_tracing.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/tracing/crash_service_uploader.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/browser/background_tracing_reactive_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(tracing::NavigationTracingObserver);

using content::RenderFrameHost;

namespace tracing {

namespace {

const char kNavigationTracingConfig[] = "navigation-config";

void OnUploadComplete(TraceCrashServiceUploader* uploader,
                      const base::Closure& done_callback,
                      bool success,
                      const std::string& feedback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  done_callback.Run();
}

void UploadCallback(const scoped_refptr<base::RefCountedString>& file_contents,
                    scoped_ptr<base::DictionaryValue> metadata,
                    base::Closure callback) {
  TraceCrashServiceUploader* uploader = new TraceCrashServiceUploader(
      g_browser_process->system_request_context());

  uploader->DoUpload(
      file_contents->data(), metadata.Pass(),
      content::TraceUploader::UploadProgressCallback(),
      base::Bind(&OnUploadComplete, base::Owned(uploader), callback));
}

}  // namespace

void SetupNavigationTracing() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kEnableNavigationTracing) ||
      !command_line.HasSwitch(switches::kTraceUploadURL)) {
    NOTREACHED();
    return;
  }

  scoped_ptr<content::BackgroundTracingReactiveConfig> config;
  config.reset(new content::BackgroundTracingReactiveConfig());

  content::BackgroundTracingReactiveConfig::TracingRule rule;
  rule.type = content::BackgroundTracingReactiveConfig::
      TRACE_FOR_10S_OR_TRIGGER_OR_FULL;
  rule.trigger_name = kNavigationTracingConfig;
  rule.category_preset =
      content::BackgroundTracingConfig::CategoryPreset::BENCHMARK_DEEP;
  config->configs.push_back(rule);

  content::BackgroundTracingManager::GetInstance()->SetActiveScenario(
      config.Pass(), base::Bind(&UploadCallback),
      content::BackgroundTracingManager::NO_DATA_FILTERING);
}

NavigationTracingObserver::NavigationTracingObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  if (navigation_handle == -1) {
    navigation_handle =
        content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
            kNavigationTracingConfig);
  }
}

NavigationTracingObserver::~NavigationTracingObserver() {
}

void NavigationTracingObserver::DidStartProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    bool is_error_page,
    bool is_iframe_srcdoc) {
  if (!render_frame_host->GetParent() && !is_error_page) {
    content::BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        navigation_handle,
        content::BackgroundTracingManager::StartedFinalizingCallback());
  }
}

content::BackgroundTracingManager::TriggerHandle
    NavigationTracingObserver::navigation_handle = -1;

}  // namespace tracing

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/navigation_tracing.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/tracing/crash_service_uploader.h"
#include "chrome/common/chrome_switches.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/public/browser/background_tracing_config.h"
#include "content/public/browser/background_tracing_manager.h"
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
                    std::unique_ptr<const base::DictionaryValue> metadata,
                    base::Closure callback) {
  TraceCrashServiceUploader* uploader = new TraceCrashServiceUploader(
      g_browser_process->system_request_context());

  uploader->DoUpload(
      file_contents->data(), content::TraceUploader::UNCOMPRESSED_UPLOAD,
      std::move(metadata), content::TraceUploader::UploadProgressCallback(),
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

  base::DictionaryValue dict;
  dict.SetString("mode", "REACTIVE_TRACING_MODE");

  std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
  {
    std::unique_ptr<base::DictionaryValue> rules_dict(
        new base::DictionaryValue());
    rules_dict->SetString("rule", "TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL");
    rules_dict->SetString("trigger_name", kNavigationTracingConfig);
    rules_dict->SetBoolean("stop_tracing_on_repeated_reactive", true);
    rules_dict->SetString("category", "BENCHMARK_DEEP");
    rules_list->Append(std::move(rules_dict));
  }
  {
    std::unique_ptr<base::DictionaryValue> rules_dict(
        new base::DictionaryValue());
    rules_dict->SetString("rule",
        "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
    rules_dict->SetString("category", "BENCHMARK_MEMORY_HEAVY");
    rules_dict->SetString("histogram_name", "V8.GCLowMemoryNotification");
    rules_dict->SetInteger("trigger_delay", 5);
    rules_dict->SetInteger("histogram_lower_value", 0);
    rules_dict->SetInteger("histogram_upper_value", 10000);
    rules_list->Append(std::move(rules_dict));
  }
  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<content::BackgroundTracingConfig> config(
      content::BackgroundTracingConfig::FromDict(&dict));
  DCHECK(config);

  content::BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), base::Bind(&UploadCallback),
      content::BackgroundTracingManager::NO_DATA_FILTERING);
}

bool NavigationTracingObserver::IsEnabled() {
  return content::BackgroundTracingManager::GetInstance()->HasActiveScenario();
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

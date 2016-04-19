// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/background_tracing_field_trial.h"

#include <string>
#include <utility>

#include "base/json/json_reader.h"
#include "base/metrics/field_trial.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/tracing/crash_service_uploader.h"
#include "chrome/common/variations/variations_util.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/background_tracing_config.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace tracing {

namespace {

const char kBackgroundTracingFieldTrial[] = "BackgroundTracing";
const char kBackgroundTracingConfig[] = "config";
const char kBackgroundTracingUploadUrl[] = "upload_url";

ConfigTextFilterForTesting g_config_text_filter_for_testing = nullptr;

void OnUploadComplete(TraceCrashServiceUploader* uploader,
                      const base::Closure& done_callback,
                      bool success,
                      const std::string& feedback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  done_callback.Run();
}

void UploadCallback(const std::string& upload_url,
                    const scoped_refptr<base::RefCountedString>& file_contents,
                    std::unique_ptr<const base::DictionaryValue> metadata,
                    base::Closure callback) {
  TraceCrashServiceUploader* uploader = new TraceCrashServiceUploader(
      g_browser_process->system_request_context());

  if (GURL(upload_url).is_valid())
    uploader->SetUploadURL(upload_url);

  uploader->DoUpload(
      file_contents->data(), content::TraceUploader::UNCOMPRESSED_UPLOAD,
      std::move(metadata), content::TraceUploader::UploadProgressCallback(),
      base::Bind(&OnUploadComplete, base::Owned(uploader), callback));
}

}  // namespace

void SetConfigTextFilterForTesting(ConfigTextFilterForTesting predicate) {
  g_config_text_filter_for_testing = predicate;
}

void SetupBackgroundTracingFieldTrial() {
  std::string config_text = variations::GetVariationParamValue(
      kBackgroundTracingFieldTrial, kBackgroundTracingConfig);
  std::string upload_url = variations::GetVariationParamValue(
      kBackgroundTracingFieldTrial, kBackgroundTracingUploadUrl);

  if (config_text.empty())
    return;

  if (g_config_text_filter_for_testing)
    (*g_config_text_filter_for_testing)(&config_text);

  std::unique_ptr<base::Value> value = base::JSONReader::Read(config_text);
  if (!value)
    return;

  const base::DictionaryValue* dict = nullptr;
  if (!value->GetAsDictionary(&dict))
    return;

  std::unique_ptr<content::BackgroundTracingConfig> config =
      content::BackgroundTracingConfig::FromDict(dict);
  if (!config)
    return;

  content::BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), base::Bind(&UploadCallback, upload_url),
      content::BackgroundTracingManager::ANONYMIZE_DATA);
}

}  // namespace tracing

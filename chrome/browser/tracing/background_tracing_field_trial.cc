// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

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

void OnUploadProgress(int64, int64) {
  // We don't actually care about the progress, but TraceUploader::DoUpload
  // requires we pass something valid.
}

void OnUploadComplete(TraceCrashServiceUploader* uploader,
                      base::Closure done_callback,
                      bool success,
                      const std::string& feedback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  done_callback.Run();
}

void UploadCallback(const std::string& upload_url,
                    const scoped_refptr<base::RefCountedString>& file_contents,
                    base::Closure callback) {
  TraceCrashServiceUploader* uploader = new TraceCrashServiceUploader(
      g_browser_process->system_request_context());

  uploader->SetUploadURL(upload_url);
  uploader->DoUpload(
      file_contents->data(), base::Bind(&OnUploadProgress),
      base::Bind(&OnUploadComplete, base::Owned(uploader), callback));
}

}  // namespace

void SetupBackgroundTracingFieldTrial() {
  std::string config_text = variations::GetVariationParamValue(
      kBackgroundTracingFieldTrial, kBackgroundTracingConfig);
  std::string upload_url = variations::GetVariationParamValue(
      kBackgroundTracingFieldTrial, kBackgroundTracingUploadUrl);

  if (!GURL(upload_url).is_valid())
    return;

  if (config_text.empty())
    return;

  scoped_ptr<base::Value> value = base::JSONReader::Read(config_text);
  if (!value)
    return;

  const base::DictionaryValue* dict = nullptr;
  if (!value->GetAsDictionary(&dict))
    return;

  scoped_ptr<content::BackgroundTracingConfig> config =
      content::BackgroundTracingConfig::FromDict(dict);
  if (!config)
    return;

  content::BackgroundTracingManager::GetInstance()->SetActiveScenario(
      config.Pass(), base::Bind(&UploadCallback, upload_url),
      content::BackgroundTracingManager::ANONYMIZE_DATA);
}

}  // namespace tracing

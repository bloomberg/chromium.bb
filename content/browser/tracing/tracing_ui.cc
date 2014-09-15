// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/tracing_ui.h"

#include <set>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/format_macros.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "content/browser/tracing/grit/tracing_resources.h"
#include "content/browser/tracing/trace_uploader.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"

namespace content {
namespace {

const char kUploadURL[] = "https://clients2.google.com/cr/staging_report";

void OnGotCategories(const WebUIDataSource::GotDataCallback& callback,
                     const std::set<std::string>& categorySet) {
  scoped_ptr<base::ListValue> category_list(new base::ListValue());
  for (std::set<std::string>::const_iterator it = categorySet.begin();
       it != categorySet.end(); it++) {
    category_list->AppendString(*it);
  }

  base::RefCountedString* res = new base::RefCountedString();
  base::JSONWriter::Write(category_list.get(), &res->data());
  callback.Run(res);
}

bool GetTracingOptions(const std::string& data64,
                       base::debug::CategoryFilter* category_filter,
                       base::debug::TraceOptions* tracing_options) {
  std::string data;
  if (!base::Base64Decode(data64, &data)) {
    LOG(ERROR) << "Options were not base64 encoded.";
    return false;
  }

  scoped_ptr<base::Value> optionsRaw(base::JSONReader::Read(data));
  if (!optionsRaw) {
    LOG(ERROR) << "Options were not valid JSON";
    return false;
  }
  base::DictionaryValue* options;
  if (!optionsRaw->GetAsDictionary(&options)) {
    LOG(ERROR) << "Options must be dict";
    return false;
  }

  if (!category_filter) {
    LOG(ERROR) << "category_filter can't be passed as NULL";
    return false;
  }

  if (!tracing_options) {
    LOG(ERROR) << "tracing_options can't be passed as NULL";
    return false;
  }

  bool options_ok = true;
  std::string category_filter_string;
  options_ok &= options->GetString("categoryFilter", &category_filter_string);
  *category_filter = base::debug::CategoryFilter(category_filter_string);

  options_ok &= options->GetBoolean("useSystemTracing",
                                    &tracing_options->enable_systrace);
  options_ok &=
      options->GetBoolean("useSampling", &tracing_options->enable_sampling);

  bool use_continuous_tracing;
  options_ok &=
      options->GetBoolean("useContinuousTracing", &use_continuous_tracing);

  if (use_continuous_tracing)
    tracing_options->record_mode = base::debug::RECORD_CONTINUOUSLY;
  else
    tracing_options->record_mode = base::debug::RECORD_UNTIL_FULL;

  if (!options_ok) {
    LOG(ERROR) << "Malformed options";
    return false;
  }
  return true;
}

void OnRecordingEnabledAck(const WebUIDataSource::GotDataCallback& callback);

bool BeginRecording(const std::string& data64,
                    const WebUIDataSource::GotDataCallback& callback) {
  base::debug::CategoryFilter category_filter("");
  base::debug::TraceOptions tracing_options;
  if (!GetTracingOptions(data64, &category_filter, &tracing_options))
    return false;

  return TracingController::GetInstance()->EnableRecording(
      category_filter,
      tracing_options,
      base::Bind(&OnRecordingEnabledAck, callback));
}

void OnRecordingEnabledAck(const WebUIDataSource::GotDataCallback& callback) {
  base::RefCountedString* res = new base::RefCountedString();
  callback.Run(res);
}

void OnTraceBufferPercentFullResult(
    const WebUIDataSource::GotDataCallback& callback, float result) {
  std::string str = base::DoubleToString(result);
  callback.Run(base::RefCountedString::TakeString(&str));
}

void OnMonitoringEnabledAck(const WebUIDataSource::GotDataCallback& callback);

bool EnableMonitoring(const std::string& data64,
                      const WebUIDataSource::GotDataCallback& callback) {
  base::debug::TraceOptions tracing_options;
  base::debug::CategoryFilter category_filter("");
  if (!GetTracingOptions(data64, &category_filter, &tracing_options))
    return false;

  return TracingController::GetInstance()->EnableMonitoring(
      category_filter,
      tracing_options,
      base::Bind(OnMonitoringEnabledAck, callback));
}

void OnMonitoringEnabledAck(const WebUIDataSource::GotDataCallback& callback) {
  base::RefCountedString* res = new base::RefCountedString();
  callback.Run(res);
}

void OnMonitoringDisabled(const WebUIDataSource::GotDataCallback& callback) {
  base::RefCountedString* res = new base::RefCountedString();
  callback.Run(res);
}

void GetMonitoringStatus(const WebUIDataSource::GotDataCallback& callback) {
  bool is_monitoring;
  base::debug::CategoryFilter category_filter("");
  base::debug::TraceOptions options;
  TracingController::GetInstance()->GetMonitoringStatus(
      &is_monitoring, &category_filter, &options);

  scoped_ptr<base::DictionaryValue>
    monitoring_options(new base::DictionaryValue());
  monitoring_options->SetBoolean("isMonitoring", is_monitoring);
  monitoring_options->SetString("categoryFilter", category_filter.ToString());
  monitoring_options->SetBoolean("useSystemTracing", options.enable_systrace);
  monitoring_options->SetBoolean(
      "useContinuousTracing",
      options.record_mode == base::debug::RECORD_CONTINUOUSLY);
  monitoring_options->SetBoolean("useSampling", options.enable_sampling);

  std::string monitoring_options_json;
  base::JSONWriter::Write(monitoring_options.get(), &monitoring_options_json);

  base::RefCountedString* monitoring_options_base64 =
    new base::RefCountedString();
  base::Base64Encode(monitoring_options_json,
                     &monitoring_options_base64->data());
  callback.Run(monitoring_options_base64);
}

void TracingCallbackWrapper(const WebUIDataSource::GotDataCallback& callback,
                            base::RefCountedString* data) {
  callback.Run(data);
}

bool OnBeginJSONRequest(const std::string& path,
                        const WebUIDataSource::GotDataCallback& callback) {
  if (path == "json/categories") {
    return TracingController::GetInstance()->GetCategories(
        base::Bind(OnGotCategories, callback));
  }

  const char* beginRecordingPath = "json/begin_recording?";
  if (StartsWithASCII(path, beginRecordingPath, true)) {
    std::string data = path.substr(strlen(beginRecordingPath));
    return BeginRecording(data, callback);
  }
  if (path == "json/get_buffer_percent_full") {
    return TracingController::GetInstance()->GetTraceBufferPercentFull(
        base::Bind(OnTraceBufferPercentFullResult, callback));
  }
  if (path == "json/end_recording") {
    return TracingController::GetInstance()->DisableRecording(
        TracingControllerImpl::CreateStringSink(
            base::Bind(TracingCallbackWrapper, callback)));
  }

  const char* enableMonitoringPath = "json/begin_monitoring?";
  if (path.find(enableMonitoringPath) == 0) {
    std::string data = path.substr(strlen(enableMonitoringPath));
    return EnableMonitoring(data, callback);
  }
  if (path == "json/end_monitoring") {
    return TracingController::GetInstance()->DisableMonitoring(
        base::Bind(OnMonitoringDisabled, callback));
  }
  if (path == "json/capture_monitoring") {
    TracingController::GetInstance()->CaptureMonitoringSnapshot(
        TracingControllerImpl::CreateStringSink(
            base::Bind(TracingCallbackWrapper, callback)));
    return true;
  }
  if (path == "json/get_monitoring_status") {
    GetMonitoringStatus(callback);
    return true;
  }

  LOG(ERROR) << "Unhandled request to " << path;
  return false;
}

bool OnTracingRequest(const std::string& path,
                      const WebUIDataSource::GotDataCallback& callback) {
  if (StartsWithASCII(path, "json/", true)) {
    if (!OnBeginJSONRequest(path, callback)) {
      std::string error("##ERROR##");
      callback.Run(base::RefCountedString::TakeString(&error));
    }
    return true;
  }
  return false;
}

}  // namespace


////////////////////////////////////////////////////////////////////////////////
//
// TracingUI
//
////////////////////////////////////////////////////////////////////////////////

TracingUI::TracingUI(WebUI* web_ui)
    : WebUIController(web_ui),
      weak_factory_(this) {
  web_ui->RegisterMessageCallback(
        "doUpload",
        base::Bind(&TracingUI::DoUpload, base::Unretained(this)));

  // Set up the chrome://tracing/ source.
  BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();

  WebUIDataSource* source = WebUIDataSource::Create(kChromeUITracingHost);
  source->SetJsonPath("strings.js");
  source->SetDefaultResource(IDR_TRACING_HTML);
  source->AddResourcePath("tracing.js", IDR_TRACING_JS);
  source->SetRequestFilter(base::Bind(OnTracingRequest));
  WebUIDataSource::Add(browser_context, source);
  TracingControllerImpl::GetInstance()->RegisterTracingUI(this);
}

TracingUI::~TracingUI() {
  TracingControllerImpl::GetInstance()->UnregisterTracingUI(this);
}

void TracingUI::OnMonitoringStateChanged(bool is_monitoring) {
  web_ui()->CallJavascriptFunction(
      "onMonitoringStateChanged", base::FundamentalValue(is_monitoring));
}

void TracingUI::DoUpload(const base::ListValue* args) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  std::string upload_url = kUploadURL;
  if (command_line.HasSwitch(switches::kTraceUploadURL)) {
    upload_url =
        command_line.GetSwitchValueASCII(switches::kTraceUploadURL);
  }
  if (!GURL(upload_url).is_valid()) {
    upload_url.clear();
  }

  if (upload_url.empty()) {
    web_ui()->CallJavascriptFunction("onUploadError",
        base::StringValue("Upload URL empty or invalid"));
    return;
  }

  std::string file_contents;
  if (!args || args->empty() || !args->GetString(0, &file_contents)) {
    web_ui()->CallJavascriptFunction("onUploadError",
                                     base::StringValue("Missing data"));
    return;
  }

  TraceUploader::UploadProgressCallback progress_callback =
      base::Bind(&TracingUI::OnTraceUploadProgress,
      weak_factory_.GetWeakPtr());
  TraceUploader::UploadDoneCallback done_callback =
      base::Bind(&TracingUI::OnTraceUploadComplete,
      weak_factory_.GetWeakPtr());

#if defined(OS_WIN)
  const char product[] = "Chrome";
#elif defined(OS_MACOSX)
  const char product[] = "Chrome_Mac";
#elif defined(OS_LINUX)
  const char product[] = "Chrome_Linux";
#elif defined(OS_ANDROID)
  const char product[] = "Chrome_Android";
#elif defined(OS_CHROMEOS)
  const char product[] = "Chrome_ChromeOS";
#else
#error Platform not supported.
#endif

  // GetProduct() returns a string like "Chrome/aa.bb.cc.dd", split out
  // the part before the "/".
  std::vector<std::string> product_components;
  base::SplitString(content::GetContentClient()->GetProduct(), '/',
                    &product_components);
  DCHECK_EQ(2U, product_components.size());
  std::string version;
  if (product_components.size() == 2U) {
    version = product_components[1];
  } else {
    version = "unknown";
  }

  BrowserContext* browser_context =
      web_ui()->GetWebContents()->GetBrowserContext();
  TraceUploader* uploader = new TraceUploader(
      product, version, upload_url, browser_context->GetRequestContext());

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, base::Bind(
     &TraceUploader::DoUpload,
     base::Unretained(uploader),
     file_contents,
     progress_callback,
     done_callback));
  // TODO(mmandlis): Add support for stopping the upload in progress.
}

void TracingUI::OnTraceUploadProgress(int64 current, int64 total) {
  DCHECK(current <= total);
  int percent = (current / total) * 100;
  web_ui()->CallJavascriptFunction(
        "onUploadProgress",
        base::FundamentalValue(percent),
        base::StringValue(base::StringPrintf("%" PRId64, current)),
        base::StringValue(base::StringPrintf("%" PRId64, total)));
}

void TracingUI::OnTraceUploadComplete(bool success,
                                      const std::string& report_id,
                                      const std::string& error_message) {
  if (success) {
    web_ui()->CallJavascriptFunction("onUploadComplete",
                                     base::StringValue(report_id));
  } else {
    web_ui()->CallJavascriptFunction("onUploadError",
                                     base::StringValue(error_message));
  }
}

}  // namespace content

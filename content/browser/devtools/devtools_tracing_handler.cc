// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_tracing_handler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "content/browser/devtools/devtools_http_handler_impl.h"
#include "content/browser/devtools/devtools_protocol_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/tracing_controller.h"

namespace content {

namespace {

const char kRecordUntilFull[]   = "record-until-full";
const char kRecordContinuously[] = "record-continuously";
const char kEnableSampling[] = "enable-sampling";

void ReadFile(
    const base::FilePath& path,
    const base::Callback<void(const scoped_refptr<base::RefCountedString>&)>
        callback) {
  std::string trace_data;
  if (!base::ReadFileToString(path, &trace_data))
    LOG(ERROR) << "Failed to read file: " << path.value();
  base::DeleteFile(path, false);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(callback, make_scoped_refptr(
          base::RefCountedString::TakeString(&trace_data))));
}

}  // namespace

DevToolsTracingHandler::DevToolsTracingHandler()
    : weak_factory_(this) {
  RegisterCommandHandler(devtools::Tracing::start::kName,
                         base::Bind(&DevToolsTracingHandler::OnStart,
                                    base::Unretained(this)));
  RegisterCommandHandler(devtools::Tracing::end::kName,
                         base::Bind(&DevToolsTracingHandler::OnEnd,
                                    base::Unretained(this)));
}

DevToolsTracingHandler::~DevToolsTracingHandler() {
}

void DevToolsTracingHandler::BeginReadingRecordingResult(
    const base::FilePath& path) {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&ReadFile, path,
                 base::Bind(&DevToolsTracingHandler::ReadRecordingResult,
                            weak_factory_.GetWeakPtr())));
}

void DevToolsTracingHandler::ReadRecordingResult(
    const scoped_refptr<base::RefCountedString>& trace_data) {
  if (trace_data->data().size()) {
    scoped_ptr<base::Value> trace_value(base::JSONReader::Read(
        trace_data->data()));
    DictionaryValue* dictionary = NULL;
    bool ok = trace_value->GetAsDictionary(&dictionary);
    DCHECK(ok);
    ListValue* list = NULL;
    ok = dictionary->GetList("traceEvents", &list);
    DCHECK(ok);
    std::string buffer;
    for (size_t i = 0; i < list->GetSize(); ++i) {
      std::string item;
      base::Value* item_value;
      list->Get(i, &item_value);
      base::JSONWriter::Write(item_value, &item);
      if (buffer.size())
        buffer.append(",");
      buffer.append(item);
      if (i % 1000 == 0) {
        OnTraceDataCollected(buffer);
        buffer.clear();
      }
    }
    if (buffer.size())
      OnTraceDataCollected(buffer);
  }

  SendNotification(devtools::Tracing::tracingComplete::kName, NULL);
}

void DevToolsTracingHandler::OnTraceDataCollected(
    const std::string& trace_fragment) {
  // Hand-craft protocol notification message so we can substitute JSON
  // that we already got as string as a bare object, not a quoted string.
  std::string message = base::StringPrintf(
      "{ \"method\": \"%s\", \"params\": { \"%s\": [ %s ] } }",
      devtools::Tracing::dataCollected::kName,
      devtools::Tracing::dataCollected::kParamValue,
      trace_fragment.c_str());
  SendRawMessage(message);
}

TracingController::Options DevToolsTracingHandler::TraceOptionsFromString(
    const std::string& options) {
  std::vector<std::string> split;
  std::vector<std::string>::iterator iter;
  int ret = 0;

  base::SplitString(options, ',', &split);
  for (iter = split.begin(); iter != split.end(); ++iter) {
    if (*iter == kRecordUntilFull) {
      ret &= ~TracingController::RECORD_CONTINUOUSLY;
    } else if (*iter == kRecordContinuously) {
      ret |= TracingController::RECORD_CONTINUOUSLY;
    } else if (*iter == kEnableSampling) {
      ret |= TracingController::ENABLE_SAMPLING;
    }
  }
  return static_cast<TracingController::Options>(ret);
}

scoped_refptr<DevToolsProtocol::Response>
DevToolsTracingHandler::OnStart(
    scoped_refptr<DevToolsProtocol::Command> command) {
  std::string categories;
  base::DictionaryValue* params = command->params();
  if (params)
    params->GetString(devtools::Tracing::start::kParamCategories, &categories);

  TracingController::Options options = TracingController::DEFAULT_OPTIONS;
  if (params && params->HasKey(devtools::Tracing::start::kParamOptions)) {
    std::string options_param;
    params->GetString(devtools::Tracing::start::kParamOptions, &options_param);
    options = TraceOptionsFromString(options_param);
  }

  TracingController::GetInstance()->EnableRecording(
      categories, options,
      TracingController::EnableRecordingDoneCallback());
  return command->SuccessResponse(NULL);
}

scoped_refptr<DevToolsProtocol::Response>
DevToolsTracingHandler::OnEnd(
    scoped_refptr<DevToolsProtocol::Command> command) {
  TracingController::GetInstance()->DisableRecording(
      base::FilePath(),
      base::Bind(&DevToolsTracingHandler::BeginReadingRecordingResult,
                 weak_factory_.GetWeakPtr()));
  return command->SuccessResponse(NULL);
}

}  // namespace content

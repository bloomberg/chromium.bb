// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_json/safe_json_parser_impl.h"

#include <utility>

#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/tuple.h"
#include "base/values.h"
#include "components/safe_json/safe_json_parser_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_host.h"
#include "grit/components_strings.h"
#include "ipc/ipc_message_macros.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;
using content::UtilityProcessHost;

namespace safe_json {

SafeJsonParserImpl::SafeJsonParserImpl(const std::string& unsafe_json,
                                       const SuccessCallback& success_callback,
                                       const ErrorCallback& error_callback)
    : unsafe_json_(unsafe_json),
      success_callback_(success_callback),
      error_callback_(error_callback) {}

SafeJsonParserImpl::~SafeJsonParserImpl() {
}

void SafeJsonParserImpl::StartWorkOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  UtilityProcessHost* host = UtilityProcessHost::Create(
      this, base::ThreadTaskRunnerHandle::Get().get());
  host->SetName(
      l10n_util::GetStringUTF16(IDS_UTILITY_PROCESS_JSON_PARSER_NAME));
  host->Send(new SafeJsonParserMsg_ParseJSON(unsafe_json_));
}

void SafeJsonParserImpl::OnJSONParseSucceeded(const base::ListValue& wrapper) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  const base::Value* value = NULL;
  CHECK(wrapper.Get(0, &value));

  parsed_json_.reset(value->DeepCopy());
  ReportResults();
}

void SafeJsonParserImpl::OnJSONParseFailed(const std::string& error_message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  error_ = error_message;
  ReportResults();
}

void SafeJsonParserImpl::ReportResults() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  caller_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SafeJsonParserImpl::ReportResultsOnOriginThread, this));
}

void SafeJsonParserImpl::ReportResultsOnOriginThread() {
  DCHECK(caller_task_runner_->RunsTasksOnCurrentThread());
  if (error_.empty() && parsed_json_) {
    if (!success_callback_.is_null())
      success_callback_.Run(std::move(parsed_json_));
  } else {
    if (!error_callback_.is_null())
      error_callback_.Run(error_);
  }
}

bool SafeJsonParserImpl::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SafeJsonParserImpl, message)
    IPC_MESSAGE_HANDLER(SafeJsonParserHostMsg_ParseJSON_Succeeded,
                        OnJSONParseSucceeded)
    IPC_MESSAGE_HANDLER(SafeJsonParserHostMsg_ParseJSON_Failed,
                        OnJSONParseFailed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SafeJsonParserImpl::Start() {
  caller_task_runner_ = base::SequencedTaskRunnerHandle::Get();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SafeJsonParserImpl::StartWorkOnIOThread, this));
}

}  // namespace safe_json

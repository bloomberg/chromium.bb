// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_json_parser.h"

#include "chrome/common/extensions/chrome_utility_extensions_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_host.h"

using content::BrowserThread;
using content::UtilityProcessHost;

SafeJsonParser::SafeJsonParser(const std::string& unsafe_json,
                               const SuccessCallback& success_callback,
                               const ErrorCallback& error_callback)
    : unsafe_json_(unsafe_json),
      success_callback_(success_callback),
      error_callback_(error_callback) {}

void SafeJsonParser::Start() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&SafeJsonParser::StartWorkOnIOThread, this));
}

SafeJsonParser::~SafeJsonParser() {}

void SafeJsonParser::StartWorkOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  UtilityProcessHost* host =
      UtilityProcessHost::Create(this, base::MessageLoopProxy::current().get());
  host->Send(new ChromeUtilityMsg_ParseJSON(unsafe_json_));
}

void SafeJsonParser::OnJSONParseSucceeded(const base::ListValue& wrapper) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  const base::Value* value = NULL;
  CHECK(wrapper.Get(0, &value));

  parsed_json_.reset(value->DeepCopy());
  ReportResults();
}

void SafeJsonParser::OnJSONParseFailed(const std::string& error_message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  error_ = error_message;
  ReportResults();
}

void SafeJsonParser::ReportResults() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&SafeJsonParser::ReportResultOnUIThread, this));
}

void SafeJsonParser::ReportResultOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (error_.empty() && parsed_json_) {
    if (!success_callback_.is_null())
      success_callback_.Run(parsed_json_.Pass());
  } else {
    if (!error_callback_.is_null())
      error_callback_.Run(error_);
  }
}

bool SafeJsonParser::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SafeJsonParser, message)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_ParseJSON_Succeeded,
                        OnJSONParseSucceeded)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_ParseJSON_Failed,
                        OnJSONParseFailed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

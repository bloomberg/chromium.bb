// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome_impl.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/test/chromedriver/devtools_client.h"
#include "chrome/test/chromedriver/net/net_util.h"
#include "chrome/test/chromedriver/net/url_request_context_getter.h"
#include "chrome/test/chromedriver/status.h"
#include "googleurl/src/gurl.h"

namespace {

Status FetchPagesInfo(URLRequestContextGetter* context_getter,
                      int port,
                      std::list<std::string>* debugger_urls) {
  std::string url = base::StringPrintf(
      "http://127.0.0.1:%d/json", port);
  std::string data;
  if (!FetchUrl(GURL(url), context_getter, &data))
    return Status(kChromeNotReachable);
  return internal::ParsePagesInfo(data, debugger_urls);
}

}  // namespace

ChromeImpl::ChromeImpl(base::ProcessHandle process,
                       URLRequestContextGetter* context_getter,
                       base::ScopedTempDir* user_data_dir,
                       int port)
    : process_(process),
      context_getter_(context_getter),
      port_(port) {
  if (user_data_dir->IsValid()) {
    CHECK(user_data_dir_.Set(user_data_dir->Take()));
  }
}

ChromeImpl::~ChromeImpl() {
  base::CloseProcessHandle(process_);
}

Status ChromeImpl::Init() {
  base::Time deadline = base::Time::Now() + base::TimeDelta::FromSeconds(20);
  std::list<std::string> debugger_urls;
  while (base::Time::Now() < deadline) {
    FetchPagesInfo(context_getter_, port_, &debugger_urls);
    if (debugger_urls.empty())
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
    else
      break;
  }
  if (debugger_urls.empty())
    return Status(kUnknownError, "unable to discover open pages");
  client_.reset(new DevToolsClient(context_getter_, debugger_urls.front()));
  return Status(kOk);
}

Status ChromeImpl::Load(const std::string& url) {
  base::DictionaryValue params;
  params.SetString("url", url);
  return client_->SendCommand("Page.navigate", params);
}

Status ChromeImpl::Quit() {
  if (!base::KillProcess(process_, 0, true))
    return Status(kUnknownError, "cannot kill Chrome");
  return Status(kOk);
}

namespace internal {

Status ParsePagesInfo(const std::string& data,
                      std::list<std::string>* debugger_urls) {
  scoped_ptr<base::Value> value(base::JSONReader::Read(data));
  if (!value.get())
    return Status(kUnknownError, "DevTools returned invalid JSON");
  base::ListValue* list;
  if (!value->GetAsList(&list))
    return Status(kUnknownError, "DevTools did not return list");

  std::list<std::string> internal_urls;
  for (size_t i = 0; i < list->GetSize(); ++i) {
    base::DictionaryValue* info;
    if (!list->GetDictionary(i, &info))
      return Status(kUnknownError, "DevTools contains non-dictionary item");
    std::string debugger_url;
    if (!info->GetString("webSocketDebuggerUrl", &debugger_url))
      return Status(kUnknownError, "DevTools did not include debugger URL");
    internal_urls.push_back(debugger_url);
  }
  debugger_urls->swap(internal_urls);
  return Status(kOk);
}

}  // namespace internal

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/devtools_http_client.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/version.h"
#include "chrome/test/chromedriver/chrome/web_view_impl.h"
#include "chrome/test/chromedriver/net/net_util.h"
#include "chrome/test/chromedriver/net/url_request_context_getter.h"

namespace {

Status FakeCloseFrontends() {
  return Status(kOk);
}

}  // namespace

WebViewInfo::WebViewInfo(const std::string& id,
                         const std::string& debugger_url,
                         const std::string& url,
                         Type type)
    : id(id), debugger_url(debugger_url), url(url), type(type) {}

WebViewInfo::~WebViewInfo() {}

bool WebViewInfo::IsFrontend() const {
  return url.find("chrome-devtools://") == 0u;
}

WebViewsInfo::WebViewsInfo() {}

WebViewsInfo::WebViewsInfo(const std::vector<WebViewInfo>& info)
    : views_info(info) {}

WebViewsInfo::~WebViewsInfo() {}

const WebViewInfo& WebViewsInfo::Get(int index) const {
  return views_info[index];
}

size_t WebViewsInfo::GetSize() const {
  return views_info.size();
}

const WebViewInfo* WebViewsInfo::GetForId(const std::string& id) const {
  for (size_t i = 0; i < views_info.size(); ++i) {
    if (views_info[i].id == id)
      return &views_info[i];
  }
  return NULL;
}

DevToolsHttpClient::DevToolsHttpClient(
    int port,
    scoped_refptr<URLRequestContextGetter> context_getter,
    const SyncWebSocketFactory& socket_factory)
    : context_getter_(context_getter),
      socket_factory_(socket_factory),
      server_url_(base::StringPrintf("http://127.0.0.1:%d", port)),
      web_socket_url_prefix_(
          base::StringPrintf("ws://127.0.0.1:%d/devtools/page/", port)) {}

DevToolsHttpClient::~DevToolsHttpClient() {}

Status DevToolsHttpClient::GetVersion(std::string* version) {
  std::string data;
  if (!FetchUrl(server_url_ + "/json/version", context_getter_, &data))
    return Status(kChromeNotReachable);

  return internal::ParseVersionInfo(data, version);
}

Status DevToolsHttpClient::GetWebViewsInfo(WebViewsInfo* views_info) {
  std::string data;
  if (!FetchUrl(server_url_ + "/json", context_getter_, &data))
    return Status(kChromeNotReachable);

  return internal::ParseWebViewsInfo(data, views_info);
}

scoped_ptr<DevToolsClient> DevToolsHttpClient::CreateClient(
    const std::string& id) {
  return scoped_ptr<DevToolsClient>(new DevToolsClientImpl(
      socket_factory_,
      web_socket_url_prefix_ + id,
      base::Bind(
          &DevToolsHttpClient::CloseFrontends, base::Unretained(this), id)));
}

Status DevToolsHttpClient::CloseWebView(const std::string& id) {
  std::string data;
  if (!FetchUrl(server_url_ + "/json/close/" + id, context_getter_, &data))
    return Status(kOk);  // Closing the last web view leads chrome to quit.

  // Wait for the target window to be completely closed.
  base::Time deadline = base::Time::Now() + base::TimeDelta::FromSeconds(20);
  while (base::Time::Now() < deadline) {
    WebViewsInfo views_info;
    Status status = GetWebViewsInfo(&views_info);
    if (status.code() == kChromeNotReachable)
      return Status(kOk);
    if (status.IsError())
      return status;
    if (!views_info.GetForId(id))
      return Status(kOk);
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(50));
  }
  return Status(kUnknownError, "failed to close window in 20 seconds");
}

Status DevToolsHttpClient::CloseFrontends(const std::string& for_client_id) {
  WebViewsInfo views_info;
  Status status = GetWebViewsInfo(&views_info);
  if (status.IsError())
    return status;

  // Close frontends. Usually frontends are docked in the same page, although
  // some may be in tabs (undocked, chrome://inspect, the DevTools
  // discovery page, etc.). Tabs can be closed via the DevTools HTTP close
  // URL, but docked frontends can only be closed, by design, by connecting
  // to them and clicking the close button. Close the tab frontends first
  // in case one of them is debugging a docked frontend, which would prevent
  // the code from being able to connect to the docked one.
  std::list<std::string> tab_frontend_ids;
  std::list<std::string> docked_frontend_ids;
  for (size_t i = 0; i < views_info.GetSize(); ++i) {
    const WebViewInfo& view_info = views_info.Get(i);
    if (view_info.IsFrontend()) {
      if (view_info.type == WebViewInfo::kPage)
        tab_frontend_ids.push_back(view_info.id);
      else if (view_info.type == WebViewInfo::kOther)
        docked_frontend_ids.push_back(view_info.id);
      else
        return Status(kUnknownError, "unknown type of DevTools frontend");
    }
  }

  for (std::list<std::string>::const_iterator it = tab_frontend_ids.begin();
       it != tab_frontend_ids.end(); ++it) {
    status = CloseWebView(*it);
    if (status.IsError())
      return status;
  }

  for (std::list<std::string>::const_iterator it = docked_frontend_ids.begin();
       it != docked_frontend_ids.end(); ++it) {
    scoped_ptr<DevToolsClient> client(new DevToolsClientImpl(
        socket_factory_,
        web_socket_url_prefix_ + *it,
        base::Bind(&FakeCloseFrontends)));
    scoped_ptr<WebViewImpl> web_view(new WebViewImpl(*it, client.Pass()));

    status = web_view->ConnectIfNecessary();
    // Ignore disconnected error, because the debugger might have closed when
    // its container page was closed above.
    if (status.IsError() && status.code() != kDisconnected)
      return status;

    scoped_ptr<base::Value> result;
    status = web_view->EvaluateScript(
        "", "document.querySelector('*[id^=\"close-button-\"').click();",
        &result);
    // Ignore disconnected error, because it may be closed already.
    if (status.IsError() && status.code() != kDisconnected)
      return status;
  }

  // Wait until DevTools UI disconnects from the given web view.
  base::Time deadline = base::Time::Now() + base::TimeDelta::FromSeconds(20);
  while (base::Time::Now() < deadline) {
    status = GetWebViewsInfo(&views_info);
    if (status.IsError())
      return status;

    const WebViewInfo* view_info = views_info.GetForId(for_client_id);
    if (!view_info) {
      return Status(kDisconnected,
                    "DevTools client closed during closing UI debuggers");
    }
    if (view_info->debugger_url.size())
      return Status(kOk);

    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(50));
  }
  return Status(kUnknownError, "failed to close UI debuggers");
}

namespace internal {

Status ParseWebViewsInfo(const std::string& data,
                         WebViewsInfo* views_info) {
  scoped_ptr<base::Value> value(base::JSONReader::Read(data));
  if (!value.get())
    return Status(kUnknownError, "DevTools returned invalid JSON");
  base::ListValue* list;
  if (!value->GetAsList(&list))
    return Status(kUnknownError, "DevTools did not return list");

  std::vector<WebViewInfo> temp_views_info;
  for (size_t i = 0; i < list->GetSize(); ++i) {
    base::DictionaryValue* info;
    if (!list->GetDictionary(i, &info))
      return Status(kUnknownError, "DevTools contains non-dictionary item");
    std::string id;
    if (!info->GetString("id", &id))
      return Status(kUnknownError, "DevTools did not include id");
    std::string type;
    if (!info->GetString("type", &type))
      return Status(kUnknownError, "DevTools did not include type");
    std::string url;
    if (!info->GetString("url", &url))
      return Status(kUnknownError, "DevTools did not include url");
    std::string debugger_url;
    info->GetString("webSocketDebuggerUrl", &debugger_url);
    if (type == "page")
      temp_views_info.push_back(
          WebViewInfo(id, debugger_url, url, WebViewInfo::kPage));
    else if (type == "other")
      temp_views_info.push_back(
          WebViewInfo(id, debugger_url, url, WebViewInfo::kOther));
    else
      return Status(kUnknownError, "DevTools returned unknown type:" + type);
  }
  *views_info = WebViewsInfo(temp_views_info);
  return Status(kOk);
}

Status ParseVersionInfo(const std::string& data,
                        std::string* version) {
  scoped_ptr<base::Value> value(base::JSONReader::Read(data));
  if (!value.get())
    return Status(kUnknownError, "version info not in JSON");
  base::DictionaryValue* dict;
  if (!value->GetAsDictionary(&dict))
    return Status(kUnknownError, "version info not a dictionary");
  if (!dict->GetString("Browser", version)) {
    return Status(
        kUnknownError,
        "Chrome version must be >= " + GetMinimumSupportedChromeVersion(),
        Status(kUnknownError, "version info doesn't include string 'Browser'"));
  }
  return Status(kOk);
}

}  // namespace internal

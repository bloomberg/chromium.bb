// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/chrome_impl.h"

#include <algorithm>
#include <list>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"
#include "chrome/test/chromedriver/chrome/javascript_dialog_manager.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/version.h"
#include "chrome/test/chromedriver/chrome/web_view_impl.h"
#include "chrome/test/chromedriver/net/net_util.h"
#include "chrome/test/chromedriver/net/sync_websocket_impl.h"
#include "chrome/test/chromedriver/net/url_request_context_getter.h"
#include "googleurl/src/gurl.h"

namespace {

typedef std::list<internal::WebViewInfo> WebViewInfoList;

Status FetchVersionInfo(URLRequestContextGetter* context_getter,
                        int port,
                        std::string* version) {
  std::string url = base::StringPrintf(
      "http://127.0.0.1:%d/json/version", port);
  std::string data;
  if (!FetchUrl(GURL(url), context_getter, &data))
    return Status(kChromeNotReachable);
  return internal::ParseVersionInfo(data, version);
}

Status FetchWebViewsInfo(URLRequestContextGetter* context_getter,
                         int port,
                         WebViewInfoList* info_list) {
  std::string url = base::StringPrintf("http://127.0.0.1:%d/json", port);
  std::string data;
  if (!FetchUrl(GURL(url), context_getter, &data))
    return Status(kChromeNotReachable);

  return internal::ParsePagesInfo(data, info_list);
}

const internal::WebViewInfo* GetWebViewFromList(
    const std::string& id,
    const WebViewInfoList& info_list) {
  for (WebViewInfoList::const_iterator it = info_list.begin();
       it != info_list.end(); ++it) {
    if (it->id == id)
      return &(*it);
  }
  return NULL;
}

Status CloseWebView(URLRequestContextGetter* context_getter,
                    int port,
                    const std::string& web_view_id) {
  WebViewInfoList info_list;
  Status status = FetchWebViewsInfo(context_getter, port, &info_list);
  if (status.IsError())
    return status;
  if (!GetWebViewFromList(web_view_id, info_list))
    return Status(kOk);

  std::string url = base::StringPrintf(
      "http://127.0.0.1:%d/json/close/%s", port, web_view_id.c_str());
  std::string data;
  if (!FetchUrl(GURL(url), context_getter, &data))
    return Status(kOk);  // Closing the last web view leads chrome to quit.
  if (data != "Target is closing")
    return Status(kOk);

  // Wait for the target window to be completely closed.
  base::Time deadline = base::Time::Now() + base::TimeDelta::FromSeconds(20);
  while (base::Time::Now() < deadline) {
    info_list.clear();
    status = FetchWebViewsInfo(context_getter, port, &info_list);
    if (status.code() == kChromeNotReachable)
      return Status(kOk);
    if (status.IsError())
      return status;
    if (!GetWebViewFromList(web_view_id, info_list))
      return Status(kOk);
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(50));
  }

  return Status(kUnknownError, "failed to close window in 20 seconds");
}

Status FakeCloseWebView() {
  // This is for the docked DevTools frontend only.
  return Status(kUnknownError,
                "docked DevTools frontend should be closed by Javascript");
}

Status FakeCloseDevToolsFrontend() {
  // This is for the docked DevTools frontend only.
  return Status(kOk);
}

Status CloseDevToolsFrontend(ChromeImpl* chrome,
                             const SyncWebSocketFactory& socket_factory,
                             URLRequestContextGetter* context_getter,
                             int port,
                             const std::string& web_view_id) {
  WebViewInfoList info_list;
  Status status = FetchWebViewsInfo(context_getter, port, &info_list);
  if (status.IsError())
    return status;

  std::list<std::string> tab_frontend_ids;
  std::list<std::string> docked_frontend_ids;
  // Filter out DevTools frontend.
  for (WebViewInfoList::const_iterator it = info_list.begin();
       it != info_list.end(); ++it) {
    if (it->IsFrontend()) {
      if (it->type == internal::WebViewInfo::kPage)
        tab_frontend_ids.push_back(it->id);
      else if (it->type == internal::WebViewInfo::kOther)
        docked_frontend_ids.push_back(it->id);
      else
        return Status(kUnknownError, "unknown type of DevTools frontend");
    }
  }

  // Close tab DevTools frontend as if closing a normal web view.
  for (std::list<std::string>::const_iterator it = tab_frontend_ids.begin();
       it != tab_frontend_ids.end(); ++it) {
    status = CloseWebView(context_getter, port, *it);
    if (status.IsError())
      return status;
  }

  // Close docked DevTools frontend by Javascript.
  for (std::list<std::string>::const_iterator it = docked_frontend_ids.begin();
       it != docked_frontend_ids.end(); ++it) {
    std::string ws_url = base::StringPrintf(
        "ws://127.0.0.1:%d/devtools/page/%s", port, it->c_str());
    scoped_ptr<WebViewImpl> web_view(new WebViewImpl(
        *it,
        new DevToolsClientImpl(socket_factory, ws_url,
                               base::Bind(&FakeCloseDevToolsFrontend)),
        chrome, base::Bind(&FakeCloseWebView)));

    status = web_view->ConnectIfNecessary();
    if (status.IsError())
      return status;

    scoped_ptr<base::Value> result;
    status = web_view->EvaluateScript(
        "", "document.getElementById('close-button-right').click();", &result);
    // Ignore disconnected error, because the DevTools frontend is closed.
    if (status.IsError() && status.code() != kDisconnected)
      return status;
  }

  // Wait until DevTools UI disconnects from the given web view.
  base::Time deadline = base::Time::Now() + base::TimeDelta::FromSeconds(20);
  bool web_view_still_open = false;
  while (base::Time::Now() < deadline) {
    info_list.clear();
    status = FetchWebViewsInfo(context_getter, port, &info_list);
    if (status.IsError())
      return status;

    web_view_still_open = false;
    for (WebViewInfoList::const_iterator it = info_list.begin();
         it != info_list.end(); ++it) {
      if (it->id == web_view_id) {
        if (!it->debugger_url.empty())
          return Status(kOk);
        web_view_still_open = true;
        break;
      }
    }
    if (!web_view_still_open)
      return Status(kUnknownError, "window closed while closing DevTools");

    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(50));
  }

  return Status(kUnknownError, "failed to close DevTools frontend");
}

}  // namespace

ChromeImpl::ChromeImpl(URLRequestContextGetter* context_getter,
                       int port,
                       const SyncWebSocketFactory& socket_factory)
    : context_getter_(context_getter),
      port_(port),
      socket_factory_(socket_factory),
      version_("unknown version"),
      build_no_(0) {}

ChromeImpl::~ChromeImpl() {
  web_view_map_.clear();
}

std::string ChromeImpl::GetVersion() {
  return version_;
}

Status ChromeImpl::GetWebViews(std::list<WebView*>* web_views) {
  WebViewInfoList info_list;
  Status status = FetchWebViewsInfo(
      context_getter_, port_, &info_list);
  if (status.IsError())
    return status;

  std::list<WebView*> internal_web_views;
  for (WebViewInfoList::const_iterator it = info_list.begin();
       it != info_list.end(); ++it) {
    if (it->type != internal::WebViewInfo::kPage)
      continue;
    WebViewMap::const_iterator found = web_view_map_.find(it->id);
    if (found != web_view_map_.end()) {
      internal_web_views.push_back(found->second.get());
      continue;
    }

    std::string ws_url = base::StringPrintf(
        "ws://127.0.0.1:%d/devtools/page/%s", port_, it->id.c_str());
    DevToolsClientImpl::FrontendCloserFunc frontend_closer_func = base::Bind(
        &CloseDevToolsFrontend, this, socket_factory_,
        context_getter_, port_, it->id);
    web_view_map_[it->id] = make_linked_ptr(new WebViewImpl(
        it->id,
        new DevToolsClientImpl(socket_factory_, ws_url, frontend_closer_func),
        this,
        base::Bind(&CloseWebView, context_getter_, port_, it->id)));
    internal_web_views.push_back(web_view_map_[it->id].get());
  }

  web_views->swap(internal_web_views);
  return Status(kOk);
}

Status ChromeImpl::IsJavaScriptDialogOpen(bool* is_open) {
  JavaScriptDialogManager* manager;
  Status status = GetDialogManagerForOpenDialog(&manager);
  if (status.IsError())
    return status;
  *is_open = manager != NULL;
  return Status(kOk);
}

Status ChromeImpl::GetJavaScriptDialogMessage(std::string* message) {
  JavaScriptDialogManager* manager;
  Status status = GetDialogManagerForOpenDialog(&manager);
  if (status.IsError())
    return status;
  if (!manager)
    return Status(kNoAlertOpen);

  return manager->GetDialogMessage(message);
}

Status ChromeImpl::HandleJavaScriptDialog(bool accept,
                                          const std::string& prompt_text) {
  JavaScriptDialogManager* manager;
  Status status = GetDialogManagerForOpenDialog(&manager);
  if (status.IsError())
    return status;
  if (!manager)
    return Status(kNoAlertOpen);

  return manager->HandleDialog(accept, prompt_text);
}

void ChromeImpl::OnWebViewClose(WebView* web_view) {
  web_view_map_.erase(web_view->GetId());
}

Status ChromeImpl::Init() {
  base::Time deadline = base::Time::Now() + base::TimeDelta::FromSeconds(20);
  std::string version;
  Status status(kOk);
  while (base::Time::Now() < deadline) {
    status = FetchVersionInfo(context_getter_, port_, &version);
    if (status.IsOk())
      break;
    if (status.code() != kChromeNotReachable)
      return status;
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
  }
  if (status.IsError())
    return status;
  status = ParseAndCheckVersion(version);
  if (status.IsError())
    return status;

  WebViewInfoList info_list;
  while (base::Time::Now() < deadline) {
    FetchWebViewsInfo(context_getter_, port_, &info_list);
    if (info_list.empty())
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
    else
      return Status(kOk);
  }
  return Status(kUnknownError, "unable to discover open pages");
}

int ChromeImpl::GetPort() const {
  return port_;
}

Status ChromeImpl::GetDialogManagerForOpenDialog(
    JavaScriptDialogManager** manager) {
  std::list<WebView*> web_views;
  Status status = GetWebViews(&web_views);
  if (status.IsError())
    return status;

  for (std::list<WebView*>::const_iterator it = web_views.begin();
       it != web_views.end(); ++it) {
    if ((*it)->GetJavaScriptDialogManager()->IsDialogOpen()) {
      *manager = (*it)->GetJavaScriptDialogManager();
      return Status(kOk);
    }
  }
  *manager = NULL;
  return Status(kOk);
}

Status ChromeImpl::ParseAndCheckVersion(const std::string& version) {
  if (version.empty()) {
    // Content Shell has an empty product version and a fake user agent.
    // There's no way to detect the actual version, so assume it is tip of tree.
    version_ = "content shell";
    build_no_ = 9999;
    return Status(kOk);
  }
  std::string prefix = "Chrome/";
  if (version.find(prefix) != 0u)
    return Status(kUnknownError, "unrecognized Chrome version: " + version);

  std::string stripped_version = version.substr(prefix.length());
  int build_no;
  std::vector<std::string> version_parts;
  base::SplitString(stripped_version, '.', &version_parts);
  if (version_parts.size() != 4 ||
      !base::StringToInt(version_parts[2], &build_no)) {
    return Status(kUnknownError, "unrecognized Chrome version: " + version);
  }

  if (build_no < kMinimumSupportedChromeBuildNo) {
    return Status(kUnknownError, "Chrome version must be >= " +
        GetMinimumSupportedChromeVersion());
  }
  version_ = stripped_version;
  build_no_ = build_no;
  return Status(kOk);
}

namespace internal {

WebViewInfo::WebViewInfo(const std::string& id,
                         const std::string& debugger_url,
                         const std::string& url,
                         Type type)
    : id(id), debugger_url(debugger_url), url(url), type(type) {}

bool WebViewInfo::IsFrontend() const {
  return url.find("chrome-devtools://") == 0u;
}

Status ParsePagesInfo(const std::string& data,
                      std::list<WebViewInfo>* info_list) {
  scoped_ptr<base::Value> value(base::JSONReader::Read(data));
  if (!value.get())
    return Status(kUnknownError, "DevTools returned invalid JSON");
  base::ListValue* list;
  if (!value->GetAsList(&list))
    return Status(kUnknownError, "DevTools did not return list");

  std::list<WebViewInfo> info_list_tmp;
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
      info_list_tmp.push_back(
          WebViewInfo(id, debugger_url, url, internal::WebViewInfo::kPage));
    else if (type == "other")
      info_list_tmp.push_back(
          WebViewInfo(id, debugger_url, url, internal::WebViewInfo::kOther));
    else
      return Status(kUnknownError, "DevTools returned unknown type:" + type);
  }
  info_list->swap(info_list_tmp);
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
        kUnknownError, "Chrome version must be >= 26",
        Status(kUnknownError, "version info doesn't include string 'Browser'"));
  }
  return Status(kOk);
}

}  // namespace internal

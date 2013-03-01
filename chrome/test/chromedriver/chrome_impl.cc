// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome_impl.h"

#include <algorithm>
#include <list>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/string_split.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/test/chromedriver/devtools_client_impl.h"
#include "chrome/test/chromedriver/javascript_dialog_manager.h"
#include "chrome/test/chromedriver/net/net_util.h"
#include "chrome/test/chromedriver/net/sync_websocket_impl.h"
#include "chrome/test/chromedriver/net/url_request_context_getter.h"
#include "chrome/test/chromedriver/status.h"
#include "chrome/test/chromedriver/version.h"
#include "chrome/test/chromedriver/web_view_impl.h"
#include "googleurl/src/gurl.h"

namespace {

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

Status FetchPagesInfo(URLRequestContextGetter* context_getter,
                      int port,
                      std::list<std::string>* page_ids) {
  std::string url = base::StringPrintf("http://127.0.0.1:%d/json", port);
  std::string data;
  if (!FetchUrl(GURL(url), context_getter, &data))
    return Status(kChromeNotReachable);
  return internal::ParsePagesInfo(data, page_ids);
}

Status CloseWebView(URLRequestContextGetter* context_getter,
                    int port,
                    const std::string& web_view_id) {
  std::list<std::string> ids;
  Status status = FetchPagesInfo(context_getter, port, &ids);
  if (status.IsError())
    return status;
  if (std::find(ids.begin(), ids.end(), web_view_id) == ids.end())
    return Status(kOk);

  bool is_last_web_view = ids.size() == 1;

  std::string url = base::StringPrintf(
      "http://127.0.0.1:%d/json/close/%s", port, web_view_id.c_str());
  std::string data;
  if (!FetchUrl(GURL(url), context_getter, &data))
    return is_last_web_view ? Status(kOk) : Status(kChromeNotReachable);
  if (data != "Target is closing")
    return Status(kOk);

  // Wait for the target window to be completely closed.
  base::Time deadline = base::Time::Now() + base::TimeDelta::FromSeconds(20);
  while (base::Time::Now() < deadline) {
    ids.clear();
    status = FetchPagesInfo(context_getter, port, &ids);
    if (is_last_web_view && status.code() == kChromeNotReachable)
      return Status(kOk);  // Closing the last web view leads chrome to quit.
    if (status.IsError())
      return status;
    if (std::find(ids.begin(), ids.end(), web_view_id) == ids.end())
      return Status(kOk);
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(50));
  }

  return Status(kUnknownError, "failed to close window in 20 seconds");
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
  std::list<std::string> ids;
  Status status = FetchPagesInfo(context_getter_, port_, &ids);
  if (status.IsError())
    return status;

  std::list<WebView*> internal_web_views;
  for (std::list<std::string>::const_iterator it = ids.begin();
       it != ids.end(); ++it) {
    WebViewMap::const_iterator found = web_view_map_.find(*it);
    if (found != web_view_map_.end()) {
      internal_web_views.push_back(found->second.get());
      continue;
    }

    std::string ws_url = base::StringPrintf(
        "ws://127.0.0.1:%d/devtools/page/%s", port_, it->c_str());
    web_view_map_[*it] = make_linked_ptr(new WebViewImpl(
        *it, new DevToolsClientImpl(socket_factory_, ws_url),
        this, base::Bind(&CloseWebView, context_getter_, port_, *it)));
    internal_web_views.push_back(web_view_map_[*it].get());
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
  }
  if (status.IsError())
    return status;
  status = ParseAndCheckVersion(version);
  if (status.IsError())
    return status;

  std::list<std::string> page_ids;
  while (base::Time::Now() < deadline) {
    FetchPagesInfo(context_getter_, port_, &page_ids);
    if (page_ids.empty())
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
    else
      break;
  }
  if (page_ids.empty())
    return Status(kUnknownError, "unable to discover open pages");

  return Status(kOk);
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

Status ParsePagesInfo(const std::string& data,
                      std::list<std::string>* page_ids) {
  scoped_ptr<base::Value> value(base::JSONReader::Read(data));
  if (!value.get())
    return Status(kUnknownError, "DevTools returned invalid JSON");
  base::ListValue* list;
  if (!value->GetAsList(&list))
    return Status(kUnknownError, "DevTools did not return list");

  std::list<std::string> ids;
  for (size_t i = 0; i < list->GetSize(); ++i) {
    base::DictionaryValue* info;
    if (!list->GetDictionary(i, &info))
      return Status(kUnknownError, "DevTools contains non-dictionary item");
    std::string type;
    if (!info->GetString("type", &type))
      return Status(kUnknownError, "DevTools did not include type");
    if (type != "page")
      continue;
    std::string id;
    if (!info->GetString("id", &id))
      return Status(kUnknownError, "DevTools did not include id");
    ids.push_back(id);
  }
  page_ids->swap(ids);
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

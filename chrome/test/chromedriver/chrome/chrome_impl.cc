// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/chrome_impl.h"

#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/devtools_http_client.h"
#include "chrome/test/chromedriver/chrome/javascript_dialog_manager.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/web_view_impl.h"

ChromeImpl::~ChromeImpl() {
  web_views_.clear();
}

std::string ChromeImpl::GetVersion() {
  return version_;
}

int ChromeImpl::GetBuildNo() {
  return build_no_;
}

Status ChromeImpl::GetWebViewIds(std::list<std::string>* web_view_ids) {
  WebViewsInfo views_info;
  Status status = devtools_http_client_->GetWebViewsInfo(&views_info);
  if (status.IsError())
    return status;

  // Check if some web views are closed.
  WebViewList::iterator it = web_views_.begin();
  while (it != web_views_.end()) {
    if (!views_info.GetForId((*it)->GetId())) {
      it = web_views_.erase(it);
    } else {
      ++it;
    }
  }

  // Check for newly-opened web views.
  for (size_t i = 0; i < views_info.GetSize(); ++i) {
    const WebViewInfo& view = views_info.Get(i);
    if (view.type != WebViewInfo::kPage)
      continue;

    bool found = false;
    for (WebViewList::const_iterator web_view_iter = web_views_.begin();
         web_view_iter != web_views_.end(); ++web_view_iter) {
      if ((*web_view_iter)->GetId() == view.id) {
        found = true;
        break;
      }
    }
    if (!found) {
      web_views_.push_back(make_linked_ptr(new WebViewImpl(
          view.id, devtools_http_client_->CreateClient(view.id))));
    }
  }

  std::list<std::string> web_view_ids_tmp;
  for (WebViewList::const_iterator web_view_iter = web_views_.begin();
       web_view_iter != web_views_.end(); ++web_view_iter) {
    web_view_ids_tmp.push_back((*web_view_iter)->GetId());
  }
  web_view_ids->swap(web_view_ids_tmp);
  return Status(kOk);
}

Status ChromeImpl::GetWebViewById(const std::string& id, WebView** web_view) {
  for (WebViewList::iterator it = web_views_.begin();
       it != web_views_.end(); ++it) {
    if ((*it)->GetId() == id) {
      *web_view = (*it).get();
      return Status(kOk);
    }
  }
  return Status(kUnknownError, "web view not found");
}

Status ChromeImpl::CloseWebView(const std::string& id) {
  Status status = devtools_http_client_->CloseWebView(id);
  if (status.IsError())
    return status;
  for (WebViewList::iterator iter = web_views_.begin();
       iter != web_views_.end(); ++iter) {
    if ((*iter)->GetId() == id) {
      web_views_.erase(iter);
      break;
    }
  }
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

ChromeImpl::ChromeImpl(
    scoped_ptr<DevToolsHttpClient> client,
    const std::string& version,
    int build_no)
    : devtools_http_client_(client.Pass()),
      version_(version),
      build_no_(build_no) {}

Status ChromeImpl::GetDialogManagerForOpenDialog(
    JavaScriptDialogManager** manager) {
  std::list<std::string> web_view_ids;
  Status status = GetWebViewIds(&web_view_ids);
  if (status.IsError())
    return status;

  for (std::list<std::string>::const_iterator it = web_view_ids.begin();
       it != web_view_ids.end(); ++it) {
    WebView* web_view;
    status = GetWebViewById(*it, &web_view);
    if (status.IsError())
      return status;
    if (web_view->GetJavaScriptDialogManager()->IsDialogOpen()) {
      *manager = web_view->GetJavaScriptDialogManager();
      return Status(kOk);
    }
  }
  *manager = NULL;
  return Status(kOk);
}

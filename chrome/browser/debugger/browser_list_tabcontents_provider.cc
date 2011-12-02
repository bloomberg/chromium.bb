// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/browser_list_tabcontents_provider.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "grit/devtools_frontend_resources.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/resource/resource_bundle.h"

using content::DevToolsHttpHandlerDelegate;

DevToolsHttpHandlerDelegate::InspectableTabs
BrowserListTabContentsProvider::GetInspectableTabs() {
  DevToolsHttpHandlerDelegate::InspectableTabs tabs;
  for (BrowserList::const_iterator it = BrowserList::begin(),
       end = BrowserList::end(); it != end; ++it) {
    TabStripModel* model = (*it)->tabstrip_model();
    for (int i = 0, size = model->count(); i < size; ++i)
      tabs.push_back(model->GetTabContentsAt(i)->tab_contents());
  }
  return tabs;
}

std::string BrowserListTabContentsProvider::GetDiscoveryPageHTML() {
  return ResourceBundle::GetSharedInstance().GetRawDataResource(
      IDR_DEVTOOLS_FRONTEND_HTML).as_string();
}

net::URLRequestContext*
BrowserListTabContentsProvider::GetURLRequestContext() {
  net::URLRequestContextGetter* getter =
      Profile::Deprecated::GetDefaultRequestContext();
  return getter ? getter->GetURLRequestContext() : NULL;
}

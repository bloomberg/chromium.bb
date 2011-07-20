// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/browser_list_tabcontents_provider.h"

#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser_list.h"

DevToolsHttpProtocolHandler::InspectableTabs
BrowserListTabContentsProvider::GetInspectableTabs() {
  DevToolsHttpProtocolHandler::InspectableTabs tabs;
  for (BrowserList::const_iterator it = BrowserList::begin(),
       end = BrowserList::end(); it != end; ++it) {
    TabStripModel* model = (*it)->tabstrip_model();
    for (int i = 0, size = model->count(); i < size; ++i)
      tabs.push_back(model->GetTabContentsAt(i));
  }
  return tabs;
}

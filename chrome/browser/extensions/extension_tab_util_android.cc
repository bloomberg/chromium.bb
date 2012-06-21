// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tab_util.h"

#include "base/logging.h"
#include "googleurl/src/gurl.h"

using base::DictionaryValue;
using base::ListValue;
using content::WebContents;

int ExtensionTabUtil::GetWindowId(const Browser* browser) {
  NOTIMPLEMENTED();
  return -1;
}

int ExtensionTabUtil::GetWindowIdOfTabStripModel(
    const TabStripModel* tab_strip_model) {
  NOTIMPLEMENTED();
  return -1;
}

int ExtensionTabUtil::GetTabId(const WebContents* web_contents) {
  NOTIMPLEMENTED();
  return -1;
}

int ExtensionTabUtil::GetWindowIdOfTab(const WebContents* web_contents) {
  NOTIMPLEMENTED();
  return -1;
}

DictionaryValue* ExtensionTabUtil::CreateTabValue(const WebContents* contents) {
  NOTIMPLEMENTED();
  return NULL;
}

ListValue* ExtensionTabUtil::CreateTabList(const Browser* browser) {
  NOTIMPLEMENTED();
  return NULL;
}

DictionaryValue* ExtensionTabUtil::CreateTabValue(const WebContents* contents,
                                                  TabStripModel* tab_strip,
                                                  int tab_index) {
  NOTIMPLEMENTED();
  return NULL;
}

DictionaryValue* ExtensionTabUtil::CreateTabValueActive(
    const WebContents* contents,
    bool active) {
  NOTIMPLEMENTED();
  return NULL;
}

bool ExtensionTabUtil::GetTabStripModel(const WebContents* web_contents,
                                        TabStripModel** tab_strip_model,
                                        int* tab_index) {
  NOTIMPLEMENTED();
  return false;
}

bool ExtensionTabUtil::GetDefaultTab(Browser* browser,
                                     TabContents** contents,
                                     int* tab_id) {
  NOTIMPLEMENTED();
  return false;
}

bool ExtensionTabUtil::GetTabById(int tab_id,
                                  Profile* profile,
                                  bool include_incognito,
                                  Browser** browser,
                                  TabStripModel** tab_strip,
                                  TabContents** contents,
                                  int* tab_index) {
  NOTIMPLEMENTED();
  return false;
}

GURL ExtensionTabUtil::ResolvePossiblyRelativeURL(const std::string& url_string,
    const extensions::Extension* extension) {
  NOTIMPLEMENTED();
  return GURL();
}

bool ExtensionTabUtil::IsCrashURL(const GURL& url) {
  NOTIMPLEMENTED();
  return false;
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tab_util.h"

#include "base/logging.h"
#include "chrome/browser/sessions/session_id.h"
#include "url/gurl.h"

using content::WebContents;

namespace extensions {

// static
int ExtensionTabUtil::GetWindowId(const Browser* browser) {
  NOTIMPLEMENTED();
  return -1;
}

// static
int ExtensionTabUtil::GetWindowIdOfTabStripModel(
    const TabStripModel* tab_strip_model) {
  NOTIMPLEMENTED();
  return -1;
}

// static
int ExtensionTabUtil::GetTabId(const WebContents* web_contents) {
  return SessionID::IdForTab(web_contents);
}

// static
int ExtensionTabUtil::GetWindowIdOfTab(const WebContents* web_contents) {
  NOTIMPLEMENTED();
  return -1;
}

// static
base::DictionaryValue* ExtensionTabUtil::CreateTabValue(
    WebContents* contents,
    TabStripModel* tab_strip,
    int tab_index,
    const Extension* extension) {
  NOTIMPLEMENTED();
  return NULL;
}

// static
base::ListValue* ExtensionTabUtil::CreateTabList(const Browser* browser,
                                                 const Extension* extension) {
  NOTIMPLEMENTED();
  return NULL;
}

// static
base::DictionaryValue* ExtensionTabUtil::CreateTabValue(
    WebContents* contents,
    TabStripModel* tab_strip,
    int tab_index) {
  NOTIMPLEMENTED();
  return NULL;
}

// static
bool ExtensionTabUtil::GetTabStripModel(const WebContents* web_contents,
                                        TabStripModel** tab_strip_model,
                                        int* tab_index) {
  NOTIMPLEMENTED();
  return false;
}

// static
bool ExtensionTabUtil::GetDefaultTab(Browser* browser,
                                     content::WebContents** contents,
                                     int* tab_id) {
  NOTIMPLEMENTED();
  return false;
}

// static
bool ExtensionTabUtil::GetTabById(int tab_id,
                                  Profile* profile,
                                  bool include_incognito,
                                  Browser** browser,
                                  TabStripModel** tab_strip,
                                  content::WebContents** contents,
                                  int* tab_index) {
  NOTIMPLEMENTED();
  return false;
}

// static
GURL ExtensionTabUtil::ResolvePossiblyRelativeURL(const std::string& url_string,
                                                  const Extension* extension) {
  NOTIMPLEMENTED();
  return GURL();
}

// static
bool ExtensionTabUtil::IsCrashURL(const GURL& url) {
  NOTIMPLEMENTED();
  return false;
}

// static
void ExtensionTabUtil::CreateTab(WebContents* web_contents,
                                 const std::string& extension_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture) {
  NOTIMPLEMENTED();
}

// static
void ExtensionTabUtil::ForEachTab(
    const base::Callback<void(WebContents*)>& callback) {
  NOTIMPLEMENTED();
}

// static
WindowController* ExtensionTabUtil::GetWindowControllerOfTab(
    const WebContents* web_contents) {
  NOTIMPLEMENTED();
  return NULL;
}

}  // namespace extensions

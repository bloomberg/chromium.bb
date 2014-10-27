// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tab_util.h"

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "url/gurl.h"

using content::WebContents;

namespace extensions {

namespace keys = tabs_constants;

ExtensionTabUtil::OpenTabParams::OpenTabParams()
    : create_browser_if_needed(false) {
}

ExtensionTabUtil::OpenTabParams::~OpenTabParams() {
}

// Opens a new tab for a given extension. Returns NULL and sets |error| if an
// error occurs.
base::DictionaryValue* ExtensionTabUtil::OpenTab(
    ChromeUIThreadExtensionFunction* function,
    const OpenTabParams& params,
    std::string* error) {
  NOTIMPLEMENTED();
  return NULL;
}

Browser* ExtensionTabUtil::GetBrowserFromWindowID(
    ChromeUIThreadExtensionFunction* function,
    int window_id,
    std::string* error) {
  NOTREACHED();
  return NULL;
}

Browser* ExtensionTabUtil::GetBrowserFromWindowID(
    const ChromeExtensionFunctionDetails& details,
    int window_id,
    std::string* error) {
  NOTREACHED();
  return NULL;
}

int ExtensionTabUtil::GetWindowId(const Browser* browser) {
  NOTREACHED();
  return -1;
}

int ExtensionTabUtil::GetWindowIdOfTabStripModel(
    const TabStripModel* tab_strip_model) {
  NOTREACHED();
  return -1;
}

int ExtensionTabUtil::GetTabId(const WebContents* web_contents) {
  NOTIMPLEMENTED();
  return -1;
}

std::string ExtensionTabUtil::GetTabStatusText(bool is_loading) {
  NOTIMPLEMENTED();
  return keys::kStatusValueComplete;
}

int ExtensionTabUtil::GetWindowIdOfTab(const WebContents* web_contents) {
  NOTIMPLEMENTED();
  return -1;
}

base::DictionaryValue* ExtensionTabUtil::CreateTabValue(
    WebContents* contents,
    TabStripModel* tab_strip,
    int tab_index,
    const Extension* extension) {
  NOTREACHED();
  return NULL;
}

base::ListValue* ExtensionTabUtil::CreateTabList(
    const Browser* browser,
    const Extension* extension) {
  return new base::ListValue();
}

base::DictionaryValue* ExtensionTabUtil::CreateTabValue(
    WebContents* contents,
    TabStripModel* tab_strip,
    int tab_index) {
  NOTREACHED();
  return NULL;
}

void ExtensionTabUtil::ScrubTabValueForExtension(
    WebContents* contents,
    const Extension* extension,
    base::DictionaryValue* tab_info) {
  // TODO(oshima): Move this to common impl.
}

void ExtensionTabUtil::ScrubTabForExtension(const Extension* extension,
                                            api::tabs::Tab* tab) {

  // TODO(oshima): Move this to common impl.
}

bool ExtensionTabUtil::GetTabStripModel(const WebContents* web_contents,
                                        TabStripModel** tab_strip_model,
                                        int* tab_index) {
  NOTIMPLEMENTED();
  return false;
}

bool ExtensionTabUtil::GetDefaultTab(Browser* browser,
                                     WebContents** contents,
                                     int* tab_id) {
  NOTIMPLEMENTED();
  return false;
}

bool ExtensionTabUtil::GetTabById(int tab_id,
                                  content::BrowserContext* browser_context,
                                  bool include_incognito,
                                  Browser** browser,
                                  TabStripModel** tab_strip,
                                  WebContents** contents,
                                  int* tab_index) {
  NOTIMPLEMENTED();
  return false;
}

GURL ExtensionTabUtil::ResolvePossiblyRelativeURL(const std::string& url_string,
                                                  const Extension* extension) {
  // TODO(oshima): Move this to common impl.
  return GURL(url_string);
}

bool ExtensionTabUtil::IsCrashURL(const GURL& url) {
  // TODO(oshima): Move this to common impl.
  return false;
}

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
  // TODO(oshima): Move this to common impl.
}

// static
WindowController* ExtensionTabUtil::GetWindowControllerOfTab(
    const WebContents* web_contents) {
  NOTIMPLEMENTED();
  return NULL;
}

void ExtensionTabUtil::OpenOptionsPage(const Extension* extension,
                                       Browser* browser) {
  // NOTIMPLEMENTED();
}

}  // namespace extensions

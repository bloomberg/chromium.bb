// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_TAB_UTIL_H__
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_TAB_UTIL_H__
#pragma once

#include <string>

class Browser;
class Profile;
class TabContents;
class TabContentsWrapper;
class TabStripModel;

namespace base {
class DictionaryValue;
class ListValue;
}

// Provides various utility functions that help manipulate tabs.
class ExtensionTabUtil {
 public:
  static int GetWindowId(const Browser* browser);
  static int GetWindowIdOfTabStripModel(const TabStripModel* tab_strip_model);
  static int GetTabId(const TabContents* tab_contents);
  static bool GetTabIdFromArgument(const base::ListValue &args,
                                   int argument_index,
                                   int *tab_id, std::string* error_message);
  static std::string GetTabStatusText(bool is_loading);
  static int GetWindowIdOfTab(const TabContents* tab_contents);
  static std::string GetWindowTypeText(const Browser* browser);
  static std::string GetWindowShowStateText(const Browser* browser);
  static base::ListValue* CreateTabList(const Browser* browser);
  static base::DictionaryValue* CreateTabValue(
      const TabContents* tab_contents);
  static base::DictionaryValue* CreateTabValue(const TabContents* tab_contents,
                                               TabStripModel* tab_strip,
                                               int tab_index);
  // Create a tab value, overriding its kSelectedKey to the provided boolean.
  static base::DictionaryValue* CreateTabValueActive(
      const TabContents* tab_contents,
      bool active);
  static base::DictionaryValue* CreateWindowValue(const Browser* browser,
                                                  bool populate_tabs);
  // Gets the |tab_strip_model| and |tab_index| for the given |tab_contents|.
  static bool GetTabStripModel(const TabContents* tab_contents,
                               TabStripModel** tab_strip_model,
                               int* tab_index);
  static bool GetDefaultTab(Browser* browser,
                            TabContentsWrapper** contents,
                            int* tab_id);
  // Any out parameter (|browser|, |tab_strip|, |contents|, & |tab_index|) may
  // be NULL and will not be set within the function.
  static bool GetTabById(int tab_id, Profile* profile, bool incognito_enabled,
                         Browser** browser,
                         TabStripModel** tab_strip,
                         TabContentsWrapper** contents,
                         int* tab_index);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_TAB_UTIL_H__

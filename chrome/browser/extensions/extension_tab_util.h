// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_TAB_UTIL_H__
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_TAB_UTIL_H__

#include <string>

#include "base/callback.h"
#include "ui/base/window_open_disposition.h"

class Browser;
class GURL;
class Profile;
class TabStripModel;

namespace base {
class DictionaryValue;
class ListValue;
}

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
class WindowController;
}

namespace gfx {
class Rect;
}

// Provides various utility functions that help manipulate tabs.
class ExtensionTabUtil {
 public:
  static int GetWindowId(const Browser* browser);
  static int GetWindowIdOfTabStripModel(const TabStripModel* tab_strip_model);
  static int GetTabId(const content::WebContents* web_contents);
  static std::string GetTabStatusText(bool is_loading);
  static int GetWindowIdOfTab(const content::WebContents* web_contents);
  static base::ListValue* CreateTabList(
      const Browser* browser,
      const extensions::Extension* extension);

  // Creates a Tab object (see chrome/common/extensions/api/tabs.json) with
  // information about the state of a browser tab.  Depending on the
  // permissions of the extension, the object may or may not include sensitive
  // data such as the tab's URL.
  static base::DictionaryValue* CreateTabValue(
      const content::WebContents* web_contents,
      const extensions::Extension* extension) {
    return CreateTabValue(web_contents, NULL, -1, extension);
  }
  static base::DictionaryValue* CreateTabValue(
      const content::WebContents* web_contents,
      TabStripModel* tab_strip,
      int tab_index,
      const extensions::Extension* extension);

  // Creates a Tab object but performs no extension permissions checks; the
  // returned object will contain privacy-sensitive data.
  static base::DictionaryValue* CreateTabValue(
      const content::WebContents* web_contents) {
    return CreateTabValue(web_contents, NULL, -1);
  }
  static base::DictionaryValue* CreateTabValue(
      const content::WebContents* web_contents,
      TabStripModel* tab_strip,
      int tab_index);

  // Removes any privacy-sensitive fields from a Tab object if appropriate,
  // given the permissions of the extension and the tab in question.  The
  // tab_info object is modified in place.
  static void ScrubTabValueForExtension(const content::WebContents* contents,
                                        const extensions::Extension* extension,
                                        base::DictionaryValue* tab_info);

  // Gets the |tab_strip_model| and |tab_index| for the given |web_contents|.
  static bool GetTabStripModel(const content::WebContents* web_contents,
                               TabStripModel** tab_strip_model,
                               int* tab_index);
  static bool GetDefaultTab(Browser* browser,
                            content::WebContents** contents,
                            int* tab_id);
  // Any out parameter (|browser|, |tab_strip|, |contents|, & |tab_index|) may
  // be NULL and will not be set within the function.
  static bool GetTabById(int tab_id, Profile* profile, bool incognito_enabled,
                         Browser** browser,
                         TabStripModel** tab_strip,
                         content::WebContents** contents,
                         int* tab_index);

  // Takes |url_string| and returns a GURL which is either valid and absolute
  // or invalid. If |url_string| is not directly interpretable as a valid (it is
  // likely a relative URL) an attempt is made to resolve it. |extension| is
  // provided so it can be resolved relative to its extension base
  // (chrome-extension://<id>/). Using the source frame url would be more
  // correct, but because the api shipped with urls resolved relative to their
  // extension base, we decided it wasn't worth breaking existing extensions to
  // fix.
  static GURL ResolvePossiblyRelativeURL(const std::string& url_string,
      const extensions::Extension* extension);

  // Returns true if |url| is used for testing crashes.
  static bool IsCrashURL(const GURL& url);

  // Opens a tab for the specified |web_contents|.
  static void CreateTab(content::WebContents* web_contents,
                        const std::string& extension_id,
                        WindowOpenDisposition disposition,
                        const gfx::Rect& initial_pos,
                        bool user_gesture);

  // Executes the specified callback for all tabs in all browser windows.
  static void ForEachTab(
      const base::Callback<void(content::WebContents*)>& callback);

  static extensions::WindowController* GetWindowControllerOfTab(
      const content::WebContents* web_contents);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_TAB_UTIL_H__

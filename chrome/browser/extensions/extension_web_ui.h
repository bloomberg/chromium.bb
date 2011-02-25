// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WEB_UI_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WEB_UI_H_
#pragma once

#include <string>

#include "base/scoped_ptr.h"
#include "chrome/browser/extensions/extension_bookmark_manager_api.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/favicon_service.h"
#include "chrome/common/extensions/extension.h"
#include "content/browser/webui/web_ui.h"

class GURL;
class ListValue;
class PrefService;
class Profile;
class RenderViewHost;
class TabContents;
struct ViewHostMsg_DomMessage_Params;

// This class implements WebUI for extensions and allows extensions to put UI in
// the main tab contents area. For example, each extension can specify an
// "options_page", and that page is displayed in the tab contents area and is
// hosted by this class.
class ExtensionWebUI
    : public WebUI,
      public ExtensionFunctionDispatcher::Delegate {
 public:
  static const char kExtensionURLOverrides[];

  explicit ExtensionWebUI(TabContents* tab_contents, const GURL& url);

  virtual ~ExtensionWebUI();

  ExtensionFunctionDispatcher* extension_function_dispatcher() const {
    return extension_function_dispatcher_.get();
  }

  // WebUI
  virtual void RenderViewCreated(RenderViewHost* render_view_host);
  virtual void RenderViewReused(RenderViewHost* render_view_host);
  virtual void ProcessWebUIMessage(const ViewHostMsg_DomMessage_Params& params);

  // ExtensionFunctionDispatcher::Delegate
  virtual Browser* GetBrowser();
  virtual gfx::NativeView GetNativeViewOfHost();
  virtual gfx::NativeWindow GetCustomFrameNativeWindow();
  virtual TabContents* associated_tab_contents() const;

  virtual ExtensionBookmarkManagerEventRouter*
      extension_bookmark_manager_event_router();

  // BrowserURLHandler
  static bool HandleChromeURLOverride(GURL* url, Profile* profile);

  // Register and unregister a dictionary of one or more overrides.
  // Page names are the keys, and chrome-extension: URLs are the values.
  // (e.g. { "newtab": "chrome-extension://<id>/my_new_tab.html" }
  static void RegisterChromeURLOverrides(Profile* profile,
      const Extension::URLOverrideMap& overrides);
  static void UnregisterChromeURLOverrides(Profile* profile,
      const Extension::URLOverrideMap& overrides);
  static void UnregisterChromeURLOverride(const std::string& page,
                                          Profile* profile,
                                          Value* override);

  // Called from BrowserPrefs
  static void RegisterUserPrefs(PrefService* prefs);

  // Get the favicon for the extension by getting an icon from the manifest.
  static void GetFaviconForURL(Profile* profile,
                               FaviconService::GetFaviconRequest* request,
                               const GURL& page_url);

 private:
  // Unregister the specified override, and if it's the currently active one,
  // ensure that something takes its place.
  static void UnregisterAndReplaceOverride(const std::string& page,
                                           Profile* profile,
                                           ListValue* list,
                                           Value* override);

  // When the RenderViewHost changes (RenderViewCreated and RenderViewReused),
  // we need to reset the ExtensionFunctionDispatcher so it's talking to the
  // right one, as well as being linked to the correct URL.
  void ResetExtensionFunctionDispatcher(RenderViewHost* render_view_host);

  void ResetExtensionBookmarkManagerEventRouter();

  scoped_ptr<ExtensionFunctionDispatcher> extension_function_dispatcher_;

  // TODO(aa): This seems out of place. Why is it not with the event routers for
  // the other extension APIs?
  scoped_ptr<ExtensionBookmarkManagerEventRouter>
      extension_bookmark_manager_event_router_;

  // The URL this WebUI was created for.
  GURL url_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WEB_UI_H_

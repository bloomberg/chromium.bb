// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_DOM_UI_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_DOM_UI_H_

#include <string>

#include "base/scoped_ptr.h"
#include "chrome/browser/dom_ui/dom_ui.h"
#include "chrome/browser/extensions/extension_bookmark_manager_api.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/favicon_service.h"
#include "chrome/common/extensions/extension.h"

class ListValue;
class PrefService;
class Profile;
class RefCountedMemory;
class RenderViewHost;
class TabContents;

// This class implements DOMUI for extensions and allows extensions to put UI in
// the main tab contents area.
class ExtensionDOMUI
    : public DOMUI,
      public ExtensionFunctionDispatcher::Delegate {
 public:
  static const wchar_t kExtensionURLOverrides[];

  explicit ExtensionDOMUI(TabContents* tab_contents);

  ExtensionFunctionDispatcher* extension_function_dispatcher() const {
    return extension_function_dispatcher_.get();
  }

  // DOMUI
  virtual void RenderViewCreated(RenderViewHost* render_view_host);
  virtual void RenderViewReused(RenderViewHost* render_view_host);
  virtual void ProcessDOMUIMessage(const std::string& message,
                                   const ListValue* content,
                                   const GURL& source_url,
                                   int request_id,
                                   bool has_callback);

  // ExtensionFunctionDispatcher::Delegate
  virtual Browser* GetBrowser() const;
  virtual gfx::NativeView GetNativeViewOfHost();
  virtual gfx::NativeWindow GetCustomFrameNativeWindow();
  virtual TabContents* associated_tab_contents() { return tab_contents(); }
  virtual Profile* GetProfile();

  virtual ExtensionBookmarkManagerEventRouter*
      extension_bookmark_manager_event_router() {
    return extension_bookmark_manager_event_router_.get();
  }

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

  scoped_ptr<ExtensionBookmarkManagerEventRouter>
      extension_bookmark_manager_event_router_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_DOM_UI_H_

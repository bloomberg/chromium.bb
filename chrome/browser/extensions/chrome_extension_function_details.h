// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_FUNCTION_DETAILS_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_FUNCTION_DETAILS_H_

#include "base/macros.h"
#include "ui/gfx/native_widget_types.h"

class Browser;
class Profile;
class UIThreadExtensionFunction;

namespace content {
class WebContents;
}

namespace extensions {
class WindowController;
}  // namespace extensions

// Provides Chrome-specific details to UIThreadExtensionFunction
// implementations.
class ChromeExtensionFunctionDetails {
 public:
  // Constructs a new ChromeExtensionFunctionDetails instance for |function|.
  // This instance does not own |function|. |function| must outlive this
  // instance.
  explicit ChromeExtensionFunctionDetails(UIThreadExtensionFunction* function);
  ~ChromeExtensionFunctionDetails();

  Profile* GetProfile() const;

  // Gets the "current" browser, if any.
  //
  // Many extension APIs operate relative to the current browser, which is the
  // browser the calling code is running inside of. For example, popups and tabs
  // all have a containing browser, but background pages and notification
  // bubbles do not.
  //
  // If there is no containing window, the current browser defaults to the
  // foremost one.
  //
  // Incognito browsers are not considered unless the calling extension has
  // incognito access enabled.
  //
  // This method can return NULL if there is no matching browser, which can
  // happen if only incognito windows are open, or early in startup or shutdown
  // shutdown when there are no active windows.
  //
  // TODO(stevenjb): Replace this with GetExtensionWindowController().
  Browser* GetCurrentBrowser() const;

  // Same as above but uses WindowControllerList instead of BrowserList.
  extensions::WindowController* GetExtensionWindowController() const;

  // Gets the "current" web contents if any. If there is no associated web
  // contents then defaults to the foremost one.
  content::WebContents* GetAssociatedWebContents();

  // Find a UI surface to display any UI (like a permission prompt) for the
  // extension calling this function. This will check, in order of preference,
  // - The current window for the function (as defined by
  //   WindowControllerList::CurrentWindowForFunction()),
  // - The sender web contents
  // - Open app windows
  // - A browser with the same profile
  gfx::NativeWindow GetNativeWindowForUI();

  // Returns a pointer to the associated UIThreadExtensionFunction
  UIThreadExtensionFunction* function() { return function_; }
  const UIThreadExtensionFunction* function() const { return function_; }

 private:
  // The function for which these details have been created. Must outlive the
  // ChromeExtensionFunctionDetails instance.
  UIThreadExtensionFunction* function_;

  DISALLOW_COPY_AND_ASSIGN(ChromeExtensionFunctionDetails);
};

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_FUNCTION_DETAILS_H_

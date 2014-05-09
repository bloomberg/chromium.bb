// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_FUNCTION_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_FUNCTION_H_

#include "extensions/browser/extension_function.h"

class Browser;
class Profile;

namespace content {
class WebContents;
}

namespace extensions {
class WindowController;
}

// A chrome specific analog to AsyncExtensionFunction. This has access to a
// chrome Profile.
class ChromeUIThreadExtensionFunction : public UIThreadExtensionFunction {
 public:
  ChromeUIThreadExtensionFunction();

  Profile* GetProfile() const;

  // Returns true if this function (and the profile and extension that it was
  // invoked from) can operate on the window wrapped by |window_controller|.
  bool CanOperateOnWindow(const extensions::WindowController* window_controller)
      const;

  // Gets the "current" browser, if any.
  //
  // Many extension APIs operate relative to the current browser, which is the
  // browser the calling code is running inside of. For example, popups, tabs,
  // and infobars all have a containing browser, but background pages and
  // notification bubbles do not.
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
  Browser* GetCurrentBrowser();

  // Same as above but uses WindowControllerList instead of BrowserList.
  extensions::WindowController* GetExtensionWindowController();

  // Gets the "current" web contents if any. If there is no associated web
  // contents then defaults to the foremost one.
  virtual content::WebContents* GetAssociatedWebContents() OVERRIDE;

 protected:
  virtual ~ChromeUIThreadExtensionFunction();
};

// A chrome specific analog to AsyncExtensionFunction. This has access to a
// chrome Profile.
class ChromeAsyncExtensionFunction : public ChromeUIThreadExtensionFunction {
 public:
  ChromeAsyncExtensionFunction();

 protected:
  virtual ~ChromeAsyncExtensionFunction();

  // Deprecated, see AsyncExtensionFunction::RunAsync.
  virtual bool RunAsync() = 0;

  // ValidationFailure override to match RunAsync().
  static bool ValidationFailure(ChromeAsyncExtensionFunction* function);

 private:
  virtual ResponseAction Run() OVERRIDE;
};

// A chrome specific analog to SyncExtensionFunction. This has access to a
// chrome Profile.
class ChromeSyncExtensionFunction : public ChromeUIThreadExtensionFunction {
 public:
  ChromeSyncExtensionFunction();

 protected:
  virtual ~ChromeSyncExtensionFunction();

  // Deprecated, see SyncExtensionFunction::RunSync.
  virtual bool RunSync() = 0;

  // ValidationFailure override to match RunSync().
  static bool ValidationFailure(ChromeSyncExtensionFunction* function);

 private:
  virtual ResponseAction Run() OVERRIDE;
};

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_FUNCTION_H_

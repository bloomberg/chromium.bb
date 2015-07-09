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
//
// DEPRECATED: Please consider inherting UIThreadExtensionFunction directly.
// Then if you need access to Chrome details, you can construct a
// ChromeExtensionFunctionDetails object within your function implementation.
class ChromeUIThreadExtensionFunction : public UIThreadExtensionFunction {
 public:
  ChromeUIThreadExtensionFunction();

  Profile* GetProfile() const;

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
  content::WebContents* GetAssociatedWebContents() override;

 protected:
  ~ChromeUIThreadExtensionFunction() override;
};

// A chrome specific analog to AsyncExtensionFunction. This has access to a
// chrome Profile.
//
// DEPRECATED: Please consider inherting UIThreadExtensionFunction or
// AsyncExtensionFunction directly. Then if you need access to Chrome details,
// you can construct a ChromeExtensionFunctionDetails object within your
// function implementation.
class ChromeAsyncExtensionFunction : public ChromeUIThreadExtensionFunction {
 public:
  ChromeAsyncExtensionFunction();

 protected:
  ~ChromeAsyncExtensionFunction() override;

  // Deprecated, see AsyncExtensionFunction::RunAsync.
  virtual bool RunAsync() = 0;

  // ValidationFailure override to match RunAsync().
  static bool ValidationFailure(ChromeAsyncExtensionFunction* function);

 private:
  // If you're hitting a compile error here due to "final" - great! You're doing
  // the right thing, you just need to extend ChromeUIThreadExtensionFunction
  // instead of ChromeAsyncExtensionFunction.
  ResponseAction Run() final;
};

// A chrome specific analog to SyncExtensionFunction. This has access to a
// chrome Profile.
//
// DEPRECATED: Please consider inherting UIThreadExtensionFunction or
// SyncExtensionFunction directly. Then if you need access to Chrome details,
// you can construct a ChromeExtensionFunctionDetails object within your
// function implementation.
class ChromeSyncExtensionFunction : public ChromeUIThreadExtensionFunction {
 public:
  ChromeSyncExtensionFunction();

 protected:
  ~ChromeSyncExtensionFunction() override;

  // Deprecated, see SyncExtensionFunction::RunSync.
  virtual bool RunSync() = 0;

  // ValidationFailure override to match RunSync().
  static bool ValidationFailure(ChromeSyncExtensionFunction* function);

 private:
  // If you're hitting a compile error here due to "final" - great! You're doing
  // the right thing, you just need to extend ChromeUIThreadExtensionFunction
  // instead of ChromeSyncExtensionFunction.
  ResponseAction Run() final;
};

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_FUNCTION_H_

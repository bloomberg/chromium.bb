// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_FUNCTION_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_FUNCTION_H_

#include "chrome/browser/extensions/chrome_extension_function_details.h"
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

  void SetError(const std::string& error);

  // ExtensionFunction:
  content::WebContents* GetAssociatedWebContents() override;
  const std::string& GetError() const override;

 protected:
  ~ChromeUIThreadExtensionFunction() override;

  // Responds with success/failure. |results_| or |error_| should be set
  // accordingly.
  void SendResponse(bool success);

  // Sets a single Value as the results of the function.
  void SetResult(std::unique_ptr<base::Value> result);

  // Sets multiple Values as the results of the function.
  void SetResultList(std::unique_ptr<base::ListValue> results);

  // Exposed versions of ExtensionFunction::results_ and
  // ExtensionFunction::error_ that are curried into the response.
  // These need to keep the same name to avoid breaking existing
  // implementations, but this should be temporary with crbug.com/648275
  // and crbug.com/634140.
  std::unique_ptr<base::ListValue> results_;
  std::string error_;

 private:
  ChromeExtensionFunctionDetails chrome_details_;
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

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_FUNCTION_H_

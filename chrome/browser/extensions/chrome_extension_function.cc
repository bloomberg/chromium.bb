// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extension_function.h"

#include <utility>

#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "extensions/browser/extension_function_dispatcher.h"

using content::RenderViewHost;
using content::WebContents;

ChromeUIThreadExtensionFunction::ChromeUIThreadExtensionFunction()
    : chrome_details_(this) {}

Profile* ChromeUIThreadExtensionFunction::GetProfile() const {
  return Profile::FromBrowserContext(context_);
}

// TODO(stevenjb): Replace this with GetExtensionWindowController().
Browser* ChromeUIThreadExtensionFunction::GetCurrentBrowser() {
  return chrome_details_.GetCurrentBrowser();
}

extensions::WindowController*
ChromeUIThreadExtensionFunction::GetExtensionWindowController() {
  return chrome_details_.GetExtensionWindowController();
}

content::WebContents*
ChromeUIThreadExtensionFunction::GetAssociatedWebContents() {
  return chrome_details_.GetAssociatedWebContents();
}

ChromeUIThreadExtensionFunction::~ChromeUIThreadExtensionFunction() {
}

ChromeAsyncExtensionFunction::ChromeAsyncExtensionFunction() {
}

ChromeAsyncExtensionFunction::~ChromeAsyncExtensionFunction() {}

ExtensionFunction::ResponseAction ChromeAsyncExtensionFunction::Run() {
  return RunAsync() ? RespondLater() : RespondNow(Error(error_));
}

// static
bool ChromeAsyncExtensionFunction::ValidationFailure(
    ChromeAsyncExtensionFunction* function) {
  return false;
}

ChromeSyncExtensionFunction::ChromeSyncExtensionFunction() {
}

ChromeSyncExtensionFunction::~ChromeSyncExtensionFunction() {}

ExtensionFunction::ResponseAction ChromeSyncExtensionFunction::Run() {
  return RespondNow(RunSync() ? ArgumentList(std::move(results_))
                              : Error(error_));
}

// static
bool ChromeSyncExtensionFunction::ValidationFailure(
    ChromeSyncExtensionFunction* function) {
  return false;
}

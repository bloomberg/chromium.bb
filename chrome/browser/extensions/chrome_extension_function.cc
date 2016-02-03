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

ChromeUIThreadExtensionFunction::ChromeUIThreadExtensionFunction() {
}

Profile* ChromeUIThreadExtensionFunction::GetProfile() const {
  return Profile::FromBrowserContext(context_);
}

// TODO(stevenjb): Replace this with GetExtensionWindowController().
Browser* ChromeUIThreadExtensionFunction::GetCurrentBrowser() {
  // If the delegate has an associated browser, return it.
  if (dispatcher()) {
    extensions::WindowController* window_controller =
        dispatcher()->GetExtensionWindowController();
    if (window_controller) {
      Browser* browser = window_controller->GetBrowser();
      if (browser)
        return browser;
    }
  }

  // Otherwise, try to default to a reasonable browser. If |include_incognito_|
  // is true, we will also search browsers in the incognito version of this
  // profile. Note that the profile may already be incognito, in which case
  // we will search the incognito version only, regardless of the value of
  // |include_incognito|. Look only for browsers on the active desktop as it is
  // preferable to pretend no browser is open then to return a browser on
  // another desktop.
  content::WebContents* web_contents = GetSenderWebContents();
  if (web_contents) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    Browser* browser = chrome::FindAnyBrowser(profile, include_incognito_);
    if (browser)
      return browser;
  }

  // NOTE(rafaelw): This can return NULL in some circumstances. In particular,
  // a background_page onload chrome.tabs api call can make it into here
  // before the browser is sufficiently initialized to return here, or
  // all of this profile's browser windows may have been closed.
  // A similar situation may arise during shutdown.
  // TODO(rafaelw): Delay creation of background_page until the browser
  // is available. http://code.google.com/p/chromium/issues/detail?id=13284
  return NULL;
}

extensions::WindowController*
ChromeUIThreadExtensionFunction::GetExtensionWindowController() {
  // If the delegate has an associated window controller, return it.
  if (dispatcher()) {
    extensions::WindowController* window_controller =
        dispatcher()->GetExtensionWindowController();
    if (window_controller)
      return window_controller;
  }

  return extensions::WindowControllerList::GetInstance()
      ->CurrentWindowForFunction(this);
}

content::WebContents*
ChromeUIThreadExtensionFunction::GetAssociatedWebContents() {
  content::WebContents* web_contents =
      UIThreadExtensionFunction::GetAssociatedWebContents();
  if (web_contents)
    return web_contents;

  Browser* browser = GetCurrentBrowser();
  if (!browser)
    return NULL;
  return browser->tab_strip_model()->GetActiveWebContents();
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

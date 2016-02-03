// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extension_function_details.h"

#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_dispatcher.h"

using content::WebContents;
using content::RenderViewHost;
using extensions::WindowController;

ChromeExtensionFunctionDetails::ChromeExtensionFunctionDetails(
    UIThreadExtensionFunction* function)
    : function_(function) {
}

ChromeExtensionFunctionDetails::~ChromeExtensionFunctionDetails() {
}

Profile* ChromeExtensionFunctionDetails::GetProfile() const {
  return Profile::FromBrowserContext(function_->browser_context());
}

// TODO(stevenjb): Replace this with GetExtensionWindowController().
Browser* ChromeExtensionFunctionDetails::GetCurrentBrowser() const {
  // If the delegate has an associated browser, return it.
  if (function_->dispatcher()) {
    extensions::WindowController* window_controller =
        function_->dispatcher()->GetExtensionWindowController();
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
  if (function_->render_frame_host()) {
    Profile* profile = Profile::FromBrowserContext(
        function_->render_frame_host()->GetProcess()->GetBrowserContext());
    Browser* browser =
        chrome::FindAnyBrowser(profile, function_->include_incognito());
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
ChromeExtensionFunctionDetails::GetExtensionWindowController() const {
  // If the delegate has an associated window controller, return it.
  if (function_->dispatcher()) {
    extensions::WindowController* window_controller =
        function_->dispatcher()->GetExtensionWindowController();
    if (window_controller)
      return window_controller;
  }

  return extensions::WindowControllerList::GetInstance()
      ->CurrentWindowForFunction(function_);
}

content::WebContents*
ChromeExtensionFunctionDetails::GetAssociatedWebContents() {
  content::WebContents* web_contents = function_->GetAssociatedWebContents();
  if (web_contents)
    return web_contents;

  Browser* browser = GetCurrentBrowser();
  if (!browser)
    return NULL;
  return browser->tab_strip_model()->GetActiveWebContents();
}

content::WebContents* ChromeExtensionFunctionDetails::GetOriginWebContents() {
  WebContents* contents = function_->GetSenderWebContents();
  if (!contents)
    return nullptr;

  // Hack: use the existence of a WebContentsModalDialogManager to decide
  // whether the sender web contents is visible.
  web_modal::WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(contents);
  if (web_contents_modal_dialog_manager)
    return contents;

  // If there is no WebContentsModalDialogManager, then this contents is
  // probably the background page for an extension or app.
  contents = nullptr;

  int source_tab_id = function_->source_tab_id();
  if (source_tab_id != TabStripModel::kNoTab) {
    // When the request originated from a background page, but there is no
    // app window open, check to see if it originated from a tab and display
    // the dialog in that tab.
    if (extensions::ExtensionTabUtil::GetTabById(
            source_tab_id, GetProfile(), true /* include_incognito */, nullptr,
            nullptr, &contents, nullptr))
      return contents;
  }

  // Try to find an app window and get its web contents.
  const extensions::Extension* extension = function_->extension();
  if (extension) {
    extensions::AppWindow* window =
        extensions::AppWindowRegistry::Get(GetProfile())
            ->GetCurrentAppWindowForApp(extension->id());
    if (window)
      return window->web_contents();
  }
  return contents;
}

gfx::NativeWindow ChromeExtensionFunctionDetails::GetNativeWindowForUI() {
  // Try to use WindowControllerList first because WebContents's
  // GetTopLevelNativeWindow() can't return the top level window when the tab
  // is not focused.
  WindowController* controller =
      extensions::WindowControllerList::GetInstance()->CurrentWindowForFunction(
          function_);
  if (controller)
    return controller->window()->GetNativeWindow();

  // CurrentWindowForFunction() can't find app's window.
  WebContents* contents = GetOriginWebContents();
  if (contents)
    return contents->GetTopLevelNativeWindow();

  Browser* browser = chrome::FindBrowserWithProfile(GetProfile());
  return browser->window()->GetNativeWindow();
}

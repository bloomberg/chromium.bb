// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/browser_extension_window_controller.h"

#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/extensions/extension.h"

BrowserExtensionWindowController::BrowserExtensionWindowController(
    Browser* browser)
    : extensions::WindowController(browser->window(), browser->profile()),
      browser_(browser) {
  extensions::WindowControllerList::GetInstance()->AddExtensionWindow(this);
}

BrowserExtensionWindowController::~BrowserExtensionWindowController() {
  extensions::WindowControllerList::GetInstance()->RemoveExtensionWindow(this);
}

int BrowserExtensionWindowController::GetWindowId() const {
  return static_cast<int>(browser_->session_id().id());
}

namespace keys = extensions::tabs_constants;

std::string BrowserExtensionWindowController::GetWindowTypeText() const {
  if (browser_->is_type_popup())
    return keys::kWindowTypeValuePopup;
  if (browser_->is_type_panel())
    return keys::kWindowTypeValuePanel;
  if (browser_->is_app())
    return keys::kWindowTypeValueApp;
  return keys::kWindowTypeValueNormal;
}

base::DictionaryValue*
BrowserExtensionWindowController::CreateWindowValue() const {
  DictionaryValue* result = extensions::WindowController::CreateWindowValue();
  return result;
}

base::DictionaryValue*
BrowserExtensionWindowController::CreateWindowValueWithTabs(
    const extensions::Extension* extension) const {
  DictionaryValue* result = CreateWindowValue();

  result->Set(keys::kTabsKey, ExtensionTabUtil::CreateTabList(browser_,
                                                              extension));

  return result;
}

bool BrowserExtensionWindowController::CanClose(Reason* reason) const {
  // Don't let an extension remove the window if the user is dragging tabs
  // in that window.
  if (!chrome::IsTabStripEditable(browser_)) {
    *reason = extensions::WindowController::REASON_NOT_EDITABLE;
    return false;
  }
  return true;
}

void BrowserExtensionWindowController::SetFullscreenMode(
    bool is_fullscreen,
    const GURL& extension_url) const {
  if (browser_->window()->IsFullscreen() != is_fullscreen)
    browser_->ToggleFullscreenModeWithExtension(extension_url);
}

Browser* BrowserExtensionWindowController::GetBrowser() const {
  return browser_;
}

bool BrowserExtensionWindowController::IsVisibleToExtension(
    const extensions::Extension* extension) const {
  // Platform apps can only see their own windows.
  // TODO(mihaip): what about non-Aura panels?
  return !extension->is_platform_app();
}

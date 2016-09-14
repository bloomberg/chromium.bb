// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/browser_extension_window_controller.h"

#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/sessions/core/session_id.h"
#include "extensions/common/extension.h"

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
  if (browser_->is_devtools())
    return keys::kWindowTypeValueDevTools;
  if (browser_->is_type_popup())
    return keys::kWindowTypeValuePopup;
  if (browser_->is_app())
    return keys::kWindowTypeValueApp;
  return keys::kWindowTypeValueNormal;
}

std::unique_ptr<base::DictionaryValue>
BrowserExtensionWindowController::CreateWindowValueWithTabs(
    const extensions::Extension* extension) const {
  std::unique_ptr<base::DictionaryValue> result = CreateWindowValue();

  result->Set(keys::kTabsKey,
              extensions::ExtensionTabUtil::CreateTabList(browser_, extension));

  return result;
}

std::unique_ptr<extensions::api::tabs::Tab>
BrowserExtensionWindowController::CreateTabObject(
    const extensions::Extension* extension,
    int tab_index) const {
  TabStripModel* tab_strip = browser_->tab_strip_model();
  return extensions::ExtensionTabUtil::CreateTabObject(
      tab_strip->GetWebContentsAt(tab_index), tab_strip, tab_index);
}

bool BrowserExtensionWindowController::CanClose(Reason* reason) const {
  // Don't let an extension remove the window if the user is dragging tabs
  // in that window.
  if (!browser_->window()->IsTabStripEditable()) {
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
  DCHECK(extension);
  // Platform apps can only see their own windows.
  return !browser_->is_devtools() && !extension->is_platform_app();
}

// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/browser_action_test_util.h"

#include "base/gfx/rect.h"
#include "base/gfx/size.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/views/browser_actions_container.h"
#include "chrome/browser/views/extensions/extension_popup.h"
#include "chrome/browser/views/toolbar_view.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"

namespace {

BrowserActionsContainer* GetContainer(Browser* browser) {
  BrowserActionsContainer* container =
      browser->window()->GetBrowserWindowTesting()->GetToolbarView()->
          browser_actions();
  return container;
}

}  // namespace

int BrowserActionTestUtil::NumberOfBrowserActions() {
  return GetContainer(browser_)->num_browser_actions();
}

bool BrowserActionTestUtil::HasIcon(int index) {
  return !GetContainer(browser_)->GetBrowserActionViewAt(index)->
      button()->icon().empty();
}

void BrowserActionTestUtil::Press(int index) {
  GetContainer(browser_)->TestExecuteBrowserAction(index);
}

std::string BrowserActionTestUtil::GetTooltip(int index) {
  std::wstring text;
  GetContainer(browser_)->GetBrowserActionViewAt(0)->button()->
      GetTooltipText(0, 0, &text);
  return WideToUTF8(text);
}

bool BrowserActionTestUtil::HasPopup() {
  return GetContainer(browser_)->TestGetPopup() != NULL;
}

gfx::Rect BrowserActionTestUtil::GetPopupBounds() {
  return GetContainer(browser_)->TestGetPopup()->view()->bounds();
}

bool BrowserActionTestUtil::HidePopup() {
  BrowserActionsContainer* container = GetContainer(browser_);
  container->HidePopup();
  return !HasPopup();
}

gfx::Size BrowserActionTestUtil::GetMinPopupSize() {
  return gfx::Size(ExtensionPopup::kMinWidth, ExtensionPopup::kMinHeight);
}

gfx::Size BrowserActionTestUtil::GetMaxPopupSize() {
  return gfx::Size(ExtensionPopup::kMaxWidth, ExtensionPopup::kMaxHeight);
}

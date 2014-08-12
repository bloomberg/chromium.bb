// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/browser_action_test_util.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window_testing_views.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/browser/ui/views/toolbar/browser_action_view.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace {

BrowserActionsContainer* GetContainer(Browser* browser) {
  return browser->window()->GetBrowserWindowTesting()->GetToolbarView()->
      browser_actions();
}

}  // namespace

int BrowserActionTestUtil::NumberOfBrowserActions() {
  return GetContainer(browser_)->num_browser_actions();
}

int BrowserActionTestUtil::VisibleBrowserActions() {
  return GetContainer(browser_)->VisibleBrowserActions();
}

ExtensionAction* BrowserActionTestUtil::GetExtensionAction(int index) {
  return extensions::ExtensionActionManager::Get(browser_->profile())->
      GetBrowserAction(*GetContainer(browser_)->GetBrowserActionViewAt(index)->
                       extension());
}

void BrowserActionTestUtil::InspectPopup(int index) {
  GetContainer(browser_)->GetBrowserActionViewAt(index)->
      view_controller()->InspectPopup();
}

bool BrowserActionTestUtil::HasIcon(int index) {
  return !GetContainer(browser_)->GetBrowserActionViewAt(index)->
      GetImage(views::Button::STATE_NORMAL).isNull();
}

gfx::Image BrowserActionTestUtil::GetIcon(int index) {
  gfx::ImageSkia icon = GetContainer(browser_)->GetBrowserActionViewAt(index)->
      GetIconForTest();
  return gfx::Image(icon);
}

void BrowserActionTestUtil::Press(int index) {
  GetContainer(browser_)->GetBrowserActionViewAt(index)->
      view_controller()->ExecuteActionByUser();
}

std::string BrowserActionTestUtil::GetExtensionId(int index) {
  return GetContainer(browser_)->GetBrowserActionViewAt(index)->
      extension()->id();
}

std::string BrowserActionTestUtil::GetTooltip(int index) {
  base::string16 text;
  GetContainer(browser_)->GetBrowserActionViewAt(index)->
      GetTooltipText(gfx::Point(), &text);
  return base::UTF16ToUTF8(text);
}

gfx::NativeView BrowserActionTestUtil::GetPopupNativeView() {
  return GetContainer(browser_)->TestGetPopup()->GetWidget()->GetNativeView();
}

bool BrowserActionTestUtil::HasPopup() {
  return GetContainer(browser_)->TestGetPopup() != NULL;
}

gfx::Rect BrowserActionTestUtil::GetPopupBounds() {
  return GetContainer(browser_)->TestGetPopup()->bounds();
}

bool BrowserActionTestUtil::HidePopup() {
  GetContainer(browser_)->HideActivePopup();
  return !HasPopup();
}

void BrowserActionTestUtil::SetIconVisibilityCount(size_t icons) {
  GetContainer(browser_)->TestSetIconVisibilityCount(icons);
}

gfx::Size BrowserActionTestUtil::GetMinPopupSize() {
  return gfx::Size(ExtensionPopup::kMinWidth, ExtensionPopup::kMinHeight);
}

gfx::Size BrowserActionTestUtil::GetMaxPopupSize() {
  return gfx::Size(ExtensionPopup::kMaxWidth, ExtensionPopup::kMaxHeight);
}

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/browser_action_test_util.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window_testing_views.h"
#include "chrome/browser/ui/views/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "ui/aura/window.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/views/widget/widget.h"

namespace {

BrowserActionsContainer* GetContainer(Browser* browser) {
  return browser->window()->GetBrowserWindowTesting()->GetToolbarView()->
      browser_actions();
}

}  // namespace

int BrowserActionTestUtil::NumberOfBrowserActions() {
  return GetContainer(browser_)->num_toolbar_actions();
}

int BrowserActionTestUtil::VisibleBrowserActions() {
  return GetContainer(browser_)->VisibleBrowserActions();
}

void BrowserActionTestUtil::InspectPopup(int index) {
  ToolbarActionView* view =
      GetContainer(browser_)->GetToolbarActionViewAt(index);
  static_cast<ExtensionActionViewController*>(view->view_controller())->
      InspectPopup();
}

bool BrowserActionTestUtil::HasIcon(int index) {
  return !GetContainer(browser_)->GetToolbarActionViewAt(index)->
      GetImage(views::Button::STATE_NORMAL).isNull();
}

gfx::Image BrowserActionTestUtil::GetIcon(int index) {
  gfx::ImageSkia icon = GetContainer(browser_)->GetToolbarActionViewAt(index)->
      GetIconForTest();
  return gfx::Image(icon);
}

void BrowserActionTestUtil::Press(int index) {
  GetContainer(browser_)->GetToolbarActionViewAt(index)->
      view_controller()->ExecuteAction(true);
}

std::string BrowserActionTestUtil::GetExtensionId(int index) {
  return GetContainer(browser_)->GetToolbarActionViewAt(index)->
      view_controller()->GetId();
}

std::string BrowserActionTestUtil::GetTooltip(int index) {
  base::string16 text;
  GetContainer(browser_)->GetToolbarActionViewAt(index)->
      GetTooltipText(gfx::Point(), &text);
  return base::UTF16ToUTF8(text);
}

gfx::NativeView BrowserActionTestUtil::GetPopupNativeView() {
  return GetContainer(browser_)->TestGetPopup();
}

bool BrowserActionTestUtil::HasPopup() {
  return GetContainer(browser_)->TestGetPopup() != NULL;
}

gfx::Size BrowserActionTestUtil::GetPopupSize() {
  gfx::NativeView popup = GetContainer(browser_)->TestGetPopup();
  views::Widget* widget = views::Widget::GetWidgetForNativeView(popup);
  return widget->GetWindowBoundsInScreen().size();
}

bool BrowserActionTestUtil::HidePopup() {
  GetContainer(browser_)->HideActivePopup();
  return !HasPopup();
}

void BrowserActionTestUtil::SetIconVisibilityCount(size_t icons) {
  extensions::ExtensionToolbarModel::Get(browser_->profile())->
      SetVisibleIconCount(icons);
}

// static
void BrowserActionTestUtil::DisableAnimations() {
  BrowserActionsContainer::disable_animations_during_testing_ = true;
}

// static
void BrowserActionTestUtil::EnableAnimations() {
  BrowserActionsContainer::disable_animations_during_testing_ = false;
}

// static
gfx::Size BrowserActionTestUtil::GetMinPopupSize() {
  return gfx::Size(ExtensionPopup::kMinWidth, ExtensionPopup::kMinHeight);
}

// static
gfx::Size BrowserActionTestUtil::GetMaxPopupSize() {
  return gfx::Size(ExtensionPopup::kMaxWidth, ExtensionPopup::kMaxHeight);
}

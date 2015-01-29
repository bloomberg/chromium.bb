// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/browser_action_test_util.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/wrench_toolbar_button.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/views/widget/widget.h"

namespace {

BrowserActionsContainer* GetContainer(Browser* browser,
                                      ToolbarActionsBarDelegate* bar_delegate) {
  if (bar_delegate)
    return static_cast<BrowserActionsContainer*>(bar_delegate);
  return BrowserView::GetBrowserViewForBrowser(browser)->toolbar()->
      browser_actions();
}

}  // namespace

int BrowserActionTestUtil::NumberOfBrowserActions() {
  return GetContainer(browser_, bar_delegate_)->num_toolbar_actions();
}

int BrowserActionTestUtil::VisibleBrowserActions() {
  return GetContainer(browser_, bar_delegate_)->VisibleBrowserActions();
}

bool BrowserActionTestUtil::IsChevronShowing() {
  BrowserActionsContainer* container = GetContainer(browser_, bar_delegate_);
  gfx::Size visible_size = container->GetVisibleBounds().size();
  return container->chevron() &&
      container->chevron()->visible() &&
      visible_size.width() >=
          container->chevron()->GetPreferredSize().width() &&
      !visible_size.IsEmpty();
}

void BrowserActionTestUtil::InspectPopup(int index) {
  ToolbarActionView* view =
      GetContainer(browser_, bar_delegate_)->GetToolbarActionViewAt(index);
  static_cast<ExtensionActionViewController*>(view->view_controller())->
      InspectPopup();
}

bool BrowserActionTestUtil::HasIcon(int index) {
  return !GetContainer(browser_, bar_delegate_)->GetToolbarActionViewAt(index)->
      GetImage(views::Button::STATE_NORMAL).isNull();
}

gfx::Image BrowserActionTestUtil::GetIcon(int index) {
  gfx::ImageSkia icon =
      GetContainer(browser_, bar_delegate_)->GetToolbarActionViewAt(index)->
          GetIconForTest();
  return gfx::Image(icon);
}

void BrowserActionTestUtil::Press(int index) {
  GetContainer(browser_, bar_delegate_)->GetToolbarActionViewAt(index)->
      view_controller()->ExecuteAction(true);
}

std::string BrowserActionTestUtil::GetExtensionId(int index) {
  return GetContainer(browser_, bar_delegate_)->GetToolbarActionViewAt(index)->
      view_controller()->GetId();
}

std::string BrowserActionTestUtil::GetTooltip(int index) {
  base::string16 text;
  GetContainer(browser_, bar_delegate_)->GetToolbarActionViewAt(index)->
      GetTooltipText(gfx::Point(), &text);
  return base::UTF16ToUTF8(text);
}

gfx::NativeView BrowserActionTestUtil::GetPopupNativeView() {
  return GetContainer(browser_, bar_delegate_)->TestGetPopup();
}

bool BrowserActionTestUtil::HasPopup() {
  return GetContainer(browser_, bar_delegate_)->TestGetPopup() != NULL;
}

gfx::Size BrowserActionTestUtil::GetPopupSize() {
  gfx::NativeView popup = GetContainer(browser_, bar_delegate_)->TestGetPopup();
  views::Widget* widget = views::Widget::GetWidgetForNativeView(popup);
  return widget->GetWindowBoundsInScreen().size();
}

bool BrowserActionTestUtil::HidePopup() {
  GetContainer(browser_, bar_delegate_)->HideActivePopup();
  return !HasPopup();
}

bool BrowserActionTestUtil::ActionButtonWantsToRun(size_t index) {
  return GetContainer(browser_, bar_delegate_)->GetToolbarActionViewAt(index)->
      wants_to_run_for_testing();
}

bool BrowserActionTestUtil::OverflowedActionButtonWantsToRun() {
  return BrowserView::GetBrowserViewForBrowser(browser_)->toolbar()->
      app_menu()->overflowed_toolbar_action_wants_to_run_for_testing();
}

ToolbarActionsBar* BrowserActionTestUtil::GetToolbarActionsBar() {
  return GetContainer(browser_, bar_delegate_)->toolbar_actions_bar();
}

// static
gfx::Size BrowserActionTestUtil::GetMinPopupSize() {
  return gfx::Size(ExtensionPopup::kMinWidth, ExtensionPopup::kMinHeight);
}

// static
gfx::Size BrowserActionTestUtil::GetMaxPopupSize() {
  return gfx::Size(ExtensionPopup::kMaxWidth, ExtensionPopup::kMaxHeight);
}

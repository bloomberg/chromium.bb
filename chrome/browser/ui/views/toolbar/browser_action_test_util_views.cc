// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/browser_action_test_util.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/views/widget/widget.h"

namespace {

// The BrowserActionsContainer expects to have a parent (and be added to the
// view hierarchy), so wrap it in a shell view that will set the container's
// bounds to be its preferred bounds.
class ContainerParent : public views::View {
 public:
  explicit ContainerParent(BrowserActionsContainer* container)
      : container_(container) {
    AddChildView(container_);
  }
  ~ContainerParent() override {}

  void Layout() override {
    container_->SizeToPreferredSize();
  }

 private:
  BrowserActionsContainer* container_;

  DISALLOW_COPY_AND_ASSIGN(ContainerParent);
};

// The views-specific implementation of the TestToolbarActionsBarHelper, which
// creates and owns a BrowserActionsContainer.
class TestToolbarActionsBarHelperViews : public TestToolbarActionsBarHelper {
 public:
  TestToolbarActionsBarHelperViews(Browser* browser,
                                   BrowserActionsContainer* main_bar);
  ~TestToolbarActionsBarHelperViews() override;

  BrowserActionsContainer* browser_actions_container() {
    return browser_actions_container_;
  }

 private:
  // The created BrowserActionsContainer. Owned by |container_parent_|.
  BrowserActionsContainer* browser_actions_container_;

  // The parent of the BrowserActionsContainer, which directly owns the
  // container as part of the views hierarchy.
  ContainerParent container_parent_;

  DISALLOW_COPY_AND_ASSIGN(TestToolbarActionsBarHelperViews);
};

TestToolbarActionsBarHelperViews::TestToolbarActionsBarHelperViews(
    Browser* browser,
    BrowserActionsContainer* main_bar)
    : browser_actions_container_(
          new BrowserActionsContainer(browser, main_bar)),
      container_parent_(browser_actions_container_) {
  container_parent_.set_owned_by_client();
  container_parent_.SetSize(gfx::Size(1000, 1000));
  container_parent_.Layout();
}

TestToolbarActionsBarHelperViews::~TestToolbarActionsBarHelperViews() {
}

BrowserActionsContainer* GetContainer(Browser* browser,
                                      TestToolbarActionsBarHelper* helper) {
  if (helper) {
    return static_cast<TestToolbarActionsBarHelperViews*>(helper)
        ->browser_actions_container();
  }
  return BrowserView::GetBrowserViewForBrowser(browser)->toolbar()->
      browser_actions();
}

}  // namespace

BrowserActionTestUtil::BrowserActionTestUtil(Browser* browser)
    : BrowserActionTestUtil(browser, true) {
}

BrowserActionTestUtil::BrowserActionTestUtil(Browser* browser,
                                             bool is_real_window)
    : browser_(browser) {
  if (!is_real_window)
    test_helper_.reset(new TestToolbarActionsBarHelperViews(browser, nullptr));
}

BrowserActionTestUtil::~BrowserActionTestUtil() {
}

int BrowserActionTestUtil::NumberOfBrowserActions() {
  return GetContainer(browser_, test_helper_.get())->num_toolbar_actions();
}

int BrowserActionTestUtil::VisibleBrowserActions() {
  return GetContainer(browser_, test_helper_.get())->VisibleBrowserActions();
}

bool BrowserActionTestUtil::IsChevronShowing() {
  BrowserActionsContainer* container =
      GetContainer(browser_, test_helper_.get());
  gfx::Size visible_size = container->GetVisibleBounds().size();
  return container->chevron() &&
      container->chevron()->visible() &&
      visible_size.width() >=
          container->chevron()->GetPreferredSize().width() &&
      !visible_size.IsEmpty();
}

void BrowserActionTestUtil::InspectPopup(int index) {
  ToolbarActionView* view =
      GetContainer(browser_, test_helper_.get())->GetToolbarActionViewAt(index);
  static_cast<ExtensionActionViewController*>(view->view_controller())->
      InspectPopup();
}

bool BrowserActionTestUtil::HasIcon(int index) {
  return !GetContainer(browser_, test_helper_.get())
              ->GetToolbarActionViewAt(index)
              ->GetImage(views::Button::STATE_NORMAL)
              .isNull();
}

gfx::Image BrowserActionTestUtil::GetIcon(int index) {
  gfx::ImageSkia icon = GetContainer(browser_, test_helper_.get())
                            ->GetToolbarActionViewAt(index)
                            ->GetIconForTest();
  return gfx::Image(icon);
}

void BrowserActionTestUtil::Press(int index) {
  GetContainer(browser_, test_helper_.get())
      ->GetToolbarActionViewAt(index)
      ->view_controller()
      ->ExecuteAction(true);
}

std::string BrowserActionTestUtil::GetExtensionId(int index) {
  return GetContainer(browser_, test_helper_.get())
      ->GetToolbarActionViewAt(index)
      ->view_controller()
      ->GetId();
}

std::string BrowserActionTestUtil::GetTooltip(int index) {
  base::string16 text;
  GetContainer(browser_, test_helper_.get())
      ->GetToolbarActionViewAt(index)
      ->GetTooltipText(gfx::Point(), &text);
  return base::UTF16ToUTF8(text);
}

gfx::NativeView BrowserActionTestUtil::GetPopupNativeView() {
  ToolbarActionViewController* popup_owner =
      GetToolbarActionsBar()->popup_owner();
  return popup_owner ? popup_owner->GetPopupNativeView() : nullptr;
}

bool BrowserActionTestUtil::HasPopup() {
  return GetPopupNativeView() != nullptr;
}

gfx::Size BrowserActionTestUtil::GetPopupSize() {
  gfx::NativeView popup = GetPopupNativeView();
  views::Widget* widget = views::Widget::GetWidgetForNativeView(popup);
  return widget->GetWindowBoundsInScreen().size();
}

bool BrowserActionTestUtil::HidePopup() {
  GetToolbarActionsBar()->HideActivePopup();
  return !HasPopup();
}

bool BrowserActionTestUtil::ActionButtonWantsToRun(size_t index) {
  return GetContainer(browser_, test_helper_.get())
      ->GetToolbarActionViewAt(index)
      ->wants_to_run_for_testing();
}

void BrowserActionTestUtil::SetWidth(int width) {
  BrowserActionsContainer* container =
      GetContainer(browser_, test_helper_.get());
  container->SetSize(gfx::Size(width, container->height()));
}

bool BrowserActionTestUtil::IsHighlightingForSurfacingBubble() {
  return GetContainer(browser_, test_helper_.get())
      ->toolbar_actions_bar()
      ->is_highlighting();
}

ToolbarActionsBar* BrowserActionTestUtil::GetToolbarActionsBar() {
  return GetContainer(browser_, test_helper_.get())->toolbar_actions_bar();
}

scoped_ptr<BrowserActionTestUtil> BrowserActionTestUtil::CreateOverflowBar() {
  CHECK(!GetToolbarActionsBar()->in_overflow_mode())
      << "Only a main bar can create an overflow bar!";
  return make_scoped_ptr(new BrowserActionTestUtil(browser_, this));
}

// static
gfx::Size BrowserActionTestUtil::GetMinPopupSize() {
  return gfx::Size(ExtensionPopup::kMinWidth, ExtensionPopup::kMinHeight);
}

// static
gfx::Size BrowserActionTestUtil::GetMaxPopupSize() {
  return gfx::Size(ExtensionPopup::kMaxWidth, ExtensionPopup::kMaxHeight);
}

BrowserActionTestUtil::BrowserActionTestUtil(Browser* browser,
                                             BrowserActionTestUtil* main_bar)
    : browser_(browser),
      test_helper_(new TestToolbarActionsBarHelperViews(
          browser_,
          GetContainer(browser_, main_bar->test_helper_.get()))) {
}

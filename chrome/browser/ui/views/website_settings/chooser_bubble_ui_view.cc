// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/website_settings/chooser_bubble_ui_view.h"

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "chrome/browser/chooser_controller/chooser_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/device_chooser_content_view.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/website_settings/chooser_bubble_delegate.h"
#include "components/bubble/bubble_controller.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/window/dialog_client_view.h"

std::unique_ptr<BubbleUi> ChooserBubbleDelegate::BuildBubbleUi() {
  return base::MakeUnique<ChooserBubbleUiView>(browser_,
                                               std::move(chooser_controller_));
}

///////////////////////////////////////////////////////////////////////////////
// View implementation for the chooser bubble.
class ChooserBubbleUiViewDelegate : public views::BubbleDialogDelegateView,
                                    public views::TableViewObserver {
 public:
  ChooserBubbleUiViewDelegate(
      views::View* anchor_view,
      views::BubbleBorder::Arrow anchor_arrow,
      std::unique_ptr<ChooserController> chooser_controller);
  ~ChooserBubbleUiViewDelegate() override;

  // views::WidgetDelegate:
  base::string16 GetWindowTitle() const override;

  // views::DialogDelegate:
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  views::View* CreateFootnoteView() override;
  bool Accept() override;
  bool Cancel() override;
  bool Close() override;

  // views::DialogDelegateView:
  views::View* GetContentsView() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

  // views::TableViewObserver:
  void OnSelectionChanged() override;

  // Updates the anchor's arrow and view. Also repositions the bubble so it's
  // displayed in the correct location.
  void UpdateAnchor(views::View* anchor_view,
                    views::BubbleBorder::Arrow anchor_arrow);

  void set_bubble_reference(BubbleReference bubble_reference);
  void UpdateTableView() const;

 private:
  DeviceChooserContentView* device_chooser_content_view_;
  BubbleReference bubble_reference_;

  DISALLOW_COPY_AND_ASSIGN(ChooserBubbleUiViewDelegate);
};

ChooserBubbleUiViewDelegate::ChooserBubbleUiViewDelegate(
    views::View* anchor_view,
    views::BubbleBorder::Arrow anchor_arrow,
    std::unique_ptr<ChooserController> chooser_controller)
    : views::BubbleDialogDelegateView(anchor_view, anchor_arrow),
      device_chooser_content_view_(nullptr) {
  // ------------------------------------
  // | Chooser bubble title             |
  // | -------------------------------- |
  // | | option 0                     | |
  // | | option 1                     | |
  // | | option 2                     | |
  // | |                              | |
  // | |                              | |
  // | |                              | |
  // | -------------------------------- |
  // |           [ Connect ] [ Cancel ] |
  // |----------------------------------|
  // | Get help                         |
  // ------------------------------------

  device_chooser_content_view_ =
      new DeviceChooserContentView(this, std::move(chooser_controller));
}

ChooserBubbleUiViewDelegate::~ChooserBubbleUiViewDelegate() {}

base::string16 ChooserBubbleUiViewDelegate::GetWindowTitle() const {
  return device_chooser_content_view_->GetWindowTitle();
}

base::string16 ChooserBubbleUiViewDelegate::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return device_chooser_content_view_->GetDialogButtonLabel(button);
}

bool ChooserBubbleUiViewDelegate::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  return device_chooser_content_view_->IsDialogButtonEnabled(button);
}

views::View* ChooserBubbleUiViewDelegate::CreateFootnoteView() {
  return device_chooser_content_view_->footnote_link();
}

bool ChooserBubbleUiViewDelegate::Accept() {
  device_chooser_content_view_->Accept();
  if (bubble_reference_)
    bubble_reference_->CloseBubble(BUBBLE_CLOSE_ACCEPTED);
  return true;
}

bool ChooserBubbleUiViewDelegate::Cancel() {
  device_chooser_content_view_->Cancel();
  if (bubble_reference_)
    bubble_reference_->CloseBubble(BUBBLE_CLOSE_CANCELED);
  return true;
}

bool ChooserBubbleUiViewDelegate::Close() {
  device_chooser_content_view_->Close();
  return true;
}

views::View* ChooserBubbleUiViewDelegate::GetContentsView() {
  return device_chooser_content_view_;
}

views::Widget* ChooserBubbleUiViewDelegate::GetWidget() {
  return device_chooser_content_view_->GetWidget();
}

const views::Widget* ChooserBubbleUiViewDelegate::GetWidget() const {
  return device_chooser_content_view_->GetWidget();
}

void ChooserBubbleUiViewDelegate::OnSelectionChanged() {
  GetDialogClientView()->UpdateDialogButtons();
}

void ChooserBubbleUiViewDelegate::UpdateAnchor(
    views::View* anchor_view,
    views::BubbleBorder::Arrow anchor_arrow) {
  if (GetAnchorView() == anchor_view && arrow() == anchor_arrow)
    return;

  set_arrow(anchor_arrow);

  // Reposition the bubble based on the updated arrow and view.
  SetAnchorView(anchor_view);
}

void ChooserBubbleUiViewDelegate::set_bubble_reference(
    BubbleReference bubble_reference) {
  bubble_reference_ = bubble_reference;
  DCHECK(bubble_reference_);
}

void ChooserBubbleUiViewDelegate::UpdateTableView() const {
  device_chooser_content_view_->UpdateTableView();
}

//////////////////////////////////////////////////////////////////////////////
// ChooserBubbleUiView
ChooserBubbleUiView::ChooserBubbleUiView(
    Browser* browser,
    std::unique_ptr<ChooserController> chooser_controller)
    : browser_(browser), chooser_bubble_ui_view_delegate_(nullptr) {
  DCHECK(browser_);
  DCHECK(chooser_controller);
  chooser_bubble_ui_view_delegate_ = new ChooserBubbleUiViewDelegate(
      GetAnchorView(), GetAnchorArrow(), std::move(chooser_controller));
}

ChooserBubbleUiView::~ChooserBubbleUiView() {}

void ChooserBubbleUiView::Show(BubbleReference bubble_reference) {
  chooser_bubble_ui_view_delegate_->set_bubble_reference(bubble_reference);

  // Set |parent_window| because some valid anchors can become hidden.
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
      browser_->window()->GetNativeWindow());
  chooser_bubble_ui_view_delegate_->set_parent_window(widget->GetNativeView());

  views::BubbleDialogDelegateView::CreateBubble(
      chooser_bubble_ui_view_delegate_)
      ->Show();

  chooser_bubble_ui_view_delegate_->UpdateTableView();
}

void ChooserBubbleUiView::Close() {}

void ChooserBubbleUiView::UpdateAnchorPosition() {
  chooser_bubble_ui_view_delegate_->UpdateAnchor(GetAnchorView(),
                                                 GetAnchorArrow());
}

views::View* ChooserBubbleUiView::GetAnchorView() {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);

  if (browser_->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR))
    return browser_view->GetLocationBarView()
        ->location_icon_view()
        ->GetImageView();

  if (browser_view->IsFullscreenBubbleVisible())
    return browser_view->exclusive_access_bubble()->GetView();

  return browser_view->top_container();
}

views::BubbleBorder::Arrow ChooserBubbleUiView::GetAnchorArrow() {
  if (browser_->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR))
    return views::BubbleBorder::TOP_LEFT;
  return views::BubbleBorder::NONE;
}

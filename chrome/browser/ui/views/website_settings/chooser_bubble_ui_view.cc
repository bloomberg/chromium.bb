// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/website_settings/chooser_bubble_ui_view.h"

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/chooser_content_view.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/website_settings/chooser_bubble_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/bubble/bubble_controller.h"
#include "components/chooser_controller/chooser_controller.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/window/dialog_client_view.h"
#include "url/origin.h"

std::unique_ptr<BubbleUi> ChooserBubbleDelegate::BuildBubbleUi() {
  return base::WrapUnique(
      new ChooserBubbleUiView(browser_, std::move(chooser_controller_)));
}

///////////////////////////////////////////////////////////////////////////////
// View implementation for the chooser bubble.
class ChooserBubbleUiViewDelegate : public views::BubbleDialogDelegateView,
                                    public views::StyledLabelListener,
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

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  // views::TableViewObserver:
  void OnSelectionChanged() override;

  // Updates the anchor's arrow and view. Also repositions the bubble so it's
  // displayed in the correct location.
  void UpdateAnchor(views::View* anchor_view,
                    views::BubbleBorder::Arrow anchor_arrow);

  void set_bubble_reference(BubbleReference bubble_reference);
  void UpdateTableModel() const;

 private:
  url::Origin origin_;
  ChooserContentView* chooser_content_view_;
  BubbleReference bubble_reference_;

  DISALLOW_COPY_AND_ASSIGN(ChooserBubbleUiViewDelegate);
};

ChooserBubbleUiViewDelegate::ChooserBubbleUiViewDelegate(
    views::View* anchor_view,
    views::BubbleBorder::Arrow anchor_arrow,
    std::unique_ptr<ChooserController> chooser_controller)
    : views::BubbleDialogDelegateView(anchor_view, anchor_arrow),
      chooser_content_view_(nullptr) {
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
  // | Not seeing your device? Get help |
  // ------------------------------------

  origin_ = chooser_controller->GetOrigin();
  chooser_content_view_ =
      new ChooserContentView(this, std::move(chooser_controller));
}

ChooserBubbleUiViewDelegate::~ChooserBubbleUiViewDelegate() {}

base::string16 ChooserBubbleUiViewDelegate::GetWindowTitle() const {
  return l10n_util::GetStringFUTF16(
      IDS_DEVICE_CHOOSER_PROMPT,
      url_formatter::FormatOriginForSecurityDisplay(
          origin_, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
}

base::string16 ChooserBubbleUiViewDelegate::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return chooser_content_view_->GetDialogButtonLabel(button);
}

bool ChooserBubbleUiViewDelegate::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  return chooser_content_view_->IsDialogButtonEnabled(button);
}

views::View* ChooserBubbleUiViewDelegate::CreateFootnoteView() {
  return chooser_content_view_->CreateFootnoteView(this);
}

bool ChooserBubbleUiViewDelegate::Accept() {
  chooser_content_view_->Accept();
  bubble_reference_->CloseBubble(BUBBLE_CLOSE_ACCEPTED);
  return true;
}

bool ChooserBubbleUiViewDelegate::Cancel() {
  chooser_content_view_->Cancel();
  bubble_reference_->CloseBubble(BUBBLE_CLOSE_CANCELED);
  return true;
}

bool ChooserBubbleUiViewDelegate::Close() {
  chooser_content_view_->Close();
  return true;
}

views::View* ChooserBubbleUiViewDelegate::GetContentsView() {
  return chooser_content_view_;
}

views::Widget* ChooserBubbleUiViewDelegate::GetWidget() {
  return chooser_content_view_->GetWidget();
}

const views::Widget* ChooserBubbleUiViewDelegate::GetWidget() const {
  return chooser_content_view_->GetWidget();
}

void ChooserBubbleUiViewDelegate::StyledLabelLinkClicked(
    views::StyledLabel* label,
    const gfx::Range& range,
    int event_flags) {
  chooser_content_view_->StyledLabelLinkClicked();
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

void ChooserBubbleUiViewDelegate::UpdateTableModel() const {
  chooser_content_view_->UpdateTableModel();
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

  chooser_bubble_ui_view_delegate_->UpdateTableModel();
}

void ChooserBubbleUiView::Close() {}

void ChooserBubbleUiView::UpdateAnchorPosition() {
  chooser_bubble_ui_view_delegate_->UpdateAnchor(GetAnchorView(),
                                                 GetAnchorArrow());
}

views::View* ChooserBubbleUiView::GetAnchorView() {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);

  if (browser_->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR))
    return browser_view->GetLocationBarView()->location_icon_view();

  if (browser_view->IsFullscreenBubbleVisible())
    return browser_view->exclusive_access_bubble()->GetView();

  return browser_view->top_container();
}

views::BubbleBorder::Arrow ChooserBubbleUiView::GetAnchorArrow() {
  if (browser_->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR))
    return views::BubbleBorder::TOP_LEFT;
  return views::BubbleBorder::NONE;
}

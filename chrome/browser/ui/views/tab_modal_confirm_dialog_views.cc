// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_modal_confirm_dialog_views.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

// static
TabModalConfirmDialog* TabModalConfirmDialog::Create(
    TabModalConfirmDialogDelegate* delegate,
    content::WebContents* web_contents) {
  return new TabModalConfirmDialogViews(delegate, web_contents);
}

//////////////////////////////////////////////////////////////////////////////
// TabModalConfirmDialogViews, constructor & destructor:

TabModalConfirmDialogViews::TabModalConfirmDialogViews(
    TabModalConfirmDialogDelegate* delegate,
    content::WebContents* web_contents)
    : delegate_(delegate) {
  views::MessageBoxView::InitParams init_params(delegate->GetDialogMessage());
  init_params.inter_row_vertical_spacing =
      views::kUnrelatedControlVerticalSpacing;
  message_box_view_ = new views::MessageBoxView(init_params);

  base::string16 link_text(delegate->GetLinkText());
  if (!link_text.empty())
    message_box_view_->SetLink(link_text, this);

  ShowWebModalDialogViews(this, web_contents);
  delegate_->set_close_delegate(this);
}

TabModalConfirmDialogViews::~TabModalConfirmDialogViews() {
}

void TabModalConfirmDialogViews::AcceptTabModalDialog() {
  GetDialogClientView()->AcceptWindow();
}

void TabModalConfirmDialogViews::CancelTabModalDialog() {
  GetDialogClientView()->CancelWindow();
}

void TabModalConfirmDialogViews::CloseDialog() {
  GetWidget()->Close();
}

//////////////////////////////////////////////////////////////////////////////
// TabModalConfirmDialogViews, views::LinkListener implementation:

void TabModalConfirmDialogViews::LinkClicked(views::Link* source,
                                             int event_flags) {
  delegate_->LinkClicked(ui::DispositionFromEventFlags(event_flags));
}

//////////////////////////////////////////////////////////////////////////////
// TabModalConfirmDialogViews, views::DialogDelegate implementation:

base::string16 TabModalConfirmDialogViews::GetWindowTitle() const {
  return delegate_->GetTitle();
}

base::string16 TabModalConfirmDialogViews::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK)
    return delegate_->GetAcceptButtonTitle();
  if (button == ui::DIALOG_BUTTON_CANCEL)
    return delegate_->GetCancelButtonTitle();
  return base::string16();
}

bool TabModalConfirmDialogViews::Cancel() {
  delegate_->Cancel();
  return true;
}

bool TabModalConfirmDialogViews::Accept() {
  delegate_->Accept();
  return true;
}

bool TabModalConfirmDialogViews::Close() {
  delegate_->Close();
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// TabModalConfirmDialogViews, views::WidgetDelegate implementation:

views::View* TabModalConfirmDialogViews::GetContentsView() {
  return message_box_view_;
}

views::Widget* TabModalConfirmDialogViews::GetWidget() {
  return message_box_view_->GetWidget();
}

const views::Widget* TabModalConfirmDialogViews::GetWidget() const {
  return message_box_view_->GetWidget();
}

void TabModalConfirmDialogViews::DeleteDelegate() {
  delete this;
}

ui::ModalType TabModalConfirmDialogViews::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

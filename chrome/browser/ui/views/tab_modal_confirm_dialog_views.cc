// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_modal_confirm_dialog_views.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "chrome/common/chrome_switches.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

using web_modal::WebContentsModalDialogManager;
using web_modal::WebContentsModalDialogManagerDelegate;

// static
TabModalConfirmDialog* TabModalConfirmDialog::Create(
    TabModalConfirmDialogDelegate* delegate,
    content::WebContents* web_contents) {
  return new TabModalConfirmDialogViews(
      delegate, web_contents);
}

//////////////////////////////////////////////////////////////////////////////
// TabModalConfirmDialogViews, constructor & destructor:

TabModalConfirmDialogViews::TabModalConfirmDialogViews(
    TabModalConfirmDialogDelegate* delegate,
    content::WebContents* web_contents)
    : delegate_(delegate),
      dialog_(NULL),
      browser_context_(web_contents->GetBrowserContext()) {
  views::MessageBoxView::InitParams init_params(delegate->GetMessage());
  init_params.inter_row_vertical_spacing =
      views::kUnrelatedControlVerticalSpacing;
  message_box_view_ = new views::MessageBoxView(init_params);

  string16 link_text(delegate->GetLinkText());
  if (!link_text.empty())
    message_box_view_->SetLink(link_text, this);

  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(web_contents);
  WebContentsModalDialogManagerDelegate* modal_delegate =
      web_contents_modal_dialog_manager->delegate();
  DCHECK(modal_delegate);
  dialog_ = views::Widget::CreateWindowAsFramelessChild(
      this,
      web_contents->GetView()->GetNativeView(),
      modal_delegate->GetWebContentsModalDialogHost()->GetHostView());
  web_contents_modal_dialog_manager->ShowDialog(dialog_->GetNativeView());
  web_contents_modal_dialog_manager->SetCloseOnInterstitialWebUI(
      dialog_->GetNativeView(), true);
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
  dialog_->Close();
}

//////////////////////////////////////////////////////////////////////////////
// TabModalConfirmDialogViews, views::LinkListener implementation:

void TabModalConfirmDialogViews::LinkClicked(views::Link* source,
                                             int event_flags) {
  delegate_->LinkClicked(ui::DispositionFromEventFlags(event_flags));
}

//////////////////////////////////////////////////////////////////////////////
// TabModalConfirmDialogViews, views::DialogDelegate implementation:

string16 TabModalConfirmDialogViews::GetWindowTitle() const {
  return delegate_->GetTitle();
}

string16 TabModalConfirmDialogViews::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK)
    return delegate_->GetAcceptButtonTitle();
  if (button == ui::DIALOG_BUTTON_CANCEL)
    return delegate_->GetCancelButtonTitle();
  return string16();
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

// TODO(wittman): Remove this override once we move to the new style frame view
// on all dialogs.
views::NonClientFrameView* TabModalConfirmDialogViews::CreateNonClientFrameView(
    views::Widget* widget) {
  return CreateConstrainedStyleNonClientFrameView(widget, browser_context_);
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
#if defined(USE_ASH)
  return ui::MODAL_TYPE_CHILD;
#else
  return views::WidgetDelegate::GetModalType();
#endif
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_modal_confirm_dialog_views.h"

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "chrome/common/chrome_switches.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/window/dialog_client_view.h"

// static
TabModalConfirmDialog* TabModalConfirmDialog::Create(
    TabModalConfirmDialogDelegate* delegate,
    TabContents* tab_contents) {
  // TODO(wittman): We're using this dialog during development; disable
  // Chrome style here at flag-flip time.
  bool enable_chrome_style =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableFramelessConstrainedDialogs);
  return new TabModalConfirmDialogViews(delegate,
                                        tab_contents,
                                        enable_chrome_style);
}

namespace {
const int kChromeStyleUniformInset = 0;
const int kChromeStyleInterRowVerticalSpacing = 20;

const int kChromeStyleButtonVEdgeMargin = 0;
const int kChromeStyleButtonHEdgeMargin = 0;
const int kChromeStyleDialogButtonLabelSpacing = 24;

views::MessageBoxView::InitParams CreateMessageBoxViewInitParams(
    const string16& message,
    bool enable_chrome_style)
{
  views::MessageBoxView::InitParams params(message);

  if (enable_chrome_style) {
    params.top_inset = kChromeStyleUniformInset;
    params.bottom_inset = kChromeStyleUniformInset;
    params.left_inset = kChromeStyleUniformInset;
    params.right_inset = kChromeStyleUniformInset;

    params.inter_row_vertical_spacing = kChromeStyleInterRowVerticalSpacing;
  }

  return params;
}
}  // namespace

//////////////////////////////////////////////////////////////////////////////
// TabModalConfirmDialogViews, constructor & destructor:

TabModalConfirmDialogViews::TabModalConfirmDialogViews(
    TabModalConfirmDialogDelegate* delegate,
    TabContents* tab_contents,
    bool enable_chrome_style)
    : delegate_(delegate),
      message_box_view_(new views::MessageBoxView(
          CreateMessageBoxViewInitParams(delegate->GetMessage(),
                                         enable_chrome_style))) {
  delegate_->set_window(new ConstrainedWindowViews(tab_contents->web_contents(),
                                                   this,
                                                   enable_chrome_style));
}

TabModalConfirmDialogViews::~TabModalConfirmDialogViews() {
}

void TabModalConfirmDialogViews::AcceptTabModalDialog() {
  GetDialogClientView()->AcceptWindow();
}

void TabModalConfirmDialogViews::CancelTabModalDialog() {
  GetDialogClientView()->CancelWindow();
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

views::ClientView* TabModalConfirmDialogViews::CreateClientView(
    views::Widget* widget) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableFramelessConstrainedDialogs)) {
    views::DialogClientView::StyleParams params;
    params.button_vedge_margin = kChromeStyleButtonVEdgeMargin;
    params.button_hedge_margin = kChromeStyleButtonHEdgeMargin;
    params.button_label_spacing = kChromeStyleDialogButtonLabelSpacing;
    params.text_button_factory =
        &views::DialogClientView::CreateChromeStyleDialogButton;

    return new views::DialogClientView(widget, GetContentsView(), params);
  }

  return DialogDelegate::CreateClientView(widget);
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

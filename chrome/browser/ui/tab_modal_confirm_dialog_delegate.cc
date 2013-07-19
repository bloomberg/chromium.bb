// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"

#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::WebContents;

TabModalConfirmDialogDelegate::TabModalConfirmDialogDelegate()
    : operations_delegate_(NULL),
      closing_(false) {
}

TabModalConfirmDialogDelegate::~TabModalConfirmDialogDelegate() {
  // If we end up here, the window has been closed, so make sure we don't close
  // it again.
  operations_delegate_ = NULL;
  // Make sure everything is cleaned up.
  Cancel();
}

void TabModalConfirmDialogDelegate::Cancel() {
  if (closing_)
    return;
  // Make sure we won't do anything when |Cancel()| or |Accept()| is called
  // again.
  closing_ = true;
  OnCanceled();
  CloseDialog();
}

void TabModalConfirmDialogDelegate::Accept() {
  if (closing_)
    return;
  // Make sure we won't do anything when |Cancel()| or |Accept()| is called
  // again.
  closing_ = true;
  OnAccepted();
  CloseDialog();
}

void TabModalConfirmDialogDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  if (closing_)
    return;
  // Make sure we won't do anything when another action occurs.
  closing_ = true;
  OnLinkClicked(disposition);
  CloseDialog();
}

gfx::Image* TabModalConfirmDialogDelegate::GetIcon() {
  return NULL;
}

string16 TabModalConfirmDialogDelegate::GetAcceptButtonTitle() {
  return l10n_util::GetStringUTF16(IDS_OK);
}

string16 TabModalConfirmDialogDelegate::GetCancelButtonTitle() {
  return l10n_util::GetStringUTF16(IDS_CANCEL);
}

string16 TabModalConfirmDialogDelegate::GetLinkText() const {
  return string16();
}

const char* TabModalConfirmDialogDelegate::GetAcceptButtonIcon() {
  return NULL;
}

const char* TabModalConfirmDialogDelegate::GetCancelButtonIcon() {
  return NULL;
}

void TabModalConfirmDialogDelegate::OnAccepted() {
}

void TabModalConfirmDialogDelegate::OnCanceled() {
}

void TabModalConfirmDialogDelegate::OnLinkClicked(
    WindowOpenDisposition disposition) {
}

void TabModalConfirmDialogDelegate::CloseDialog() {
  if (operations_delegate_)
    operations_delegate_->CloseDialog();
}

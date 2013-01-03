// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"

#include "chrome/browser/ui/web_contents_modal_dialog.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::NavigationController;
using content::WebContents;

TabModalConfirmDialogDelegate::TabModalConfirmDialogDelegate(
    WebContents* web_contents)
    : window_(NULL),
      closing_(false) {
  NavigationController* controller = &web_contents->GetController();
  registrar_.Add(this, content::NOTIFICATION_LOAD_START,
                 content::Source<NavigationController>(controller));
  registrar_.Add(this, chrome::NOTIFICATION_TAB_CLOSING,
                 content::Source<NavigationController>(controller));
}

TabModalConfirmDialogDelegate::~TabModalConfirmDialogDelegate() {
  // If we end up here, the constrained window has been closed, so make sure we
  // don't close it again.
  window_ = NULL;
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


void TabModalConfirmDialogDelegate::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  // Close the dialog if we load a page (because the action might not apply to
  // the same page anymore) or if the tab is closed.
  if (type == content::NOTIFICATION_LOAD_START ||
      type == chrome::NOTIFICATION_TAB_CLOSING) {
    Cancel();
  } else {
    NOTREACHED();
  }
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

void TabModalConfirmDialogDelegate::CloseDialog() {
  if (window_)
    window_->CloseWebContentsModalDialog();
}

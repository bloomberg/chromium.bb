// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_promo/sync_promo_dialog.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/webui/html_dialog_tab_contents_delegate.h"
#include "chrome/browser/ui/webui/sync_promo/sync_promo_ui.h"
#include "content/public/browser/page_navigator.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/size.h"

SyncPromoDialog::SyncPromoDialog(Profile* profile, GURL url)
    : profile_(profile),
      spawned_browser_(NULL),
      sync_promo_was_closed_(false),
      url_(url),
      window_(NULL) {
}

SyncPromoDialog::~SyncPromoDialog() {
}

void SyncPromoDialog::ShowDialog() {
  window_ = browser::ShowHtmlDialog(NULL, profile_, NULL, this, STYLE_GENERIC);

  // Wait for the dialog to close.
  MessageLoop::current()->Run();
}

ui::ModalType SyncPromoDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

string16 SyncPromoDialog::GetDialogTitle() const {
  return l10n_util::GetStringFUTF16(
      IDS_SYNC_PROMO_TITLE,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
}

GURL SyncPromoDialog::GetDialogContentURL() const {
  return url_;
}

void SyncPromoDialog::GetWebUIMessageHandlers(
    std::vector<content::WebUIMessageHandler*>* handlers) const {
}

void SyncPromoDialog::GetDialogSize(gfx::Size* size) const {
  DCHECK(size);
  const int kDialogWidth = 530;
  const int kDialogHeight = 540;
  *size = gfx::Size(kDialogWidth, kDialogHeight);
}

std::string SyncPromoDialog::GetDialogArgs() const {
  return std::string();
}

void SyncPromoDialog::OnDialogClosed(const std::string& json_retval) {
  MessageLoop::current()->Quit();
}

void SyncPromoDialog::OnCloseContents(content::WebContents* source,
                                      bool* out_close_dialog) {
}

bool SyncPromoDialog::ShouldShowDialogTitle() const {
  return true;
}

bool SyncPromoDialog::HandleContextMenu(
    const content::ContextMenuParams& params) {
  return true;
}

bool SyncPromoDialog::HandleOpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    content::WebContents** out_new_contents) {
  spawned_browser_ = HtmlDialogTabContentsDelegate::StaticOpenURLFromTab(
      profile_, source, params, out_new_contents);
  // If the open URL request is for the current tab then that means the sync
  // promo page will be closed.
  sync_promo_was_closed_ = params.disposition == CURRENT_TAB;
  CloseDialog();
  return true;
}

bool SyncPromoDialog::HandleAddNewContents(
    content::WebContents* source,
    content::WebContents* new_contents,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_pos,
    bool user_gesture) {
  spawned_browser_ = HtmlDialogTabContentsDelegate::StaticAddNewContents(
      profile_, source, new_contents, disposition, initial_pos, user_gesture);
  CloseDialog();
  return true;
}

void SyncPromoDialog::CloseDialog() {
  // Only close the window once.
  if (!window_)
    return;

  // Close the dialog asynchronously since closing it will cause this and other
  // WebUI handlers to be deleted.
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(browser::CloseHtmlDialog, window_));
  window_ = NULL;
}

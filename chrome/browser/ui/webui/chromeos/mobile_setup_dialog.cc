// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/mobile_setup_dialog.h"

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

// static
MobileSetupDialog* MobileSetupDialog::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return Singleton<MobileSetupDialog>::get();
}

MobileSetupDialog::MobileSetupDialog() {
}

MobileSetupDialog::~MobileSetupDialog() {
}

// static
void MobileSetupDialog::Show() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  MobileSetupDialog* dialog = MobileSetupDialog::GetInstance();
  dialog->ShowDialog();
}

void MobileSetupDialog::ShowDialog() {
  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
    return;
  browser->BrowserShowHtmlDialog(this, NULL);
}

bool MobileSetupDialog::IsDialogModal() const {
  return true;
}

string16 MobileSetupDialog::GetDialogTitle() const {
  return l10n_util::GetStringUTF16(IDS_MOBILE_SETUP_TITLE);
}

GURL MobileSetupDialog::GetDialogContentURL() const {
  return GURL(chrome::kChromeUIMobileSetupURL);
}

void MobileSetupDialog::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const{
}

void MobileSetupDialog::GetDialogSize(gfx::Size* size) const {
#if defined(POST_PORTAL)
  size->SetSize(850, 650);
#else
  size->SetSize(1100, 700);
#endif
}

std::string MobileSetupDialog::GetDialogArgs() const {
  return std::string();
}

void MobileSetupDialog::OnDialogClosed(const std::string& json_retval) {
}

void MobileSetupDialog::OnCloseContents(TabContents* source,
                                        bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool MobileSetupDialog::ShouldShowDialogTitle() const {
  return true;
}

bool MobileSetupDialog::HandleContextMenu(const ContextMenuParams& params) {
  return true;
}

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
#include "chrome/browser/ui/dialog_style.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

class MobileSetupDialogDelegate : public HtmlDialogUIDelegate {
 public:
  static MobileSetupDialogDelegate* GetInstance();
  void ShowDialog();

 protected:
  friend struct DefaultSingletonTraits<MobileSetupDialogDelegate>;

  MobileSetupDialogDelegate();
  virtual ~MobileSetupDialogDelegate();

  void OnCloseDialog();

  // HtmlDialogUIDelegate overrides.
  virtual bool IsDialogModal() const OVERRIDE;
  virtual string16 GetDialogTitle() const OVERRIDE;
  virtual GURL GetDialogContentURL() const OVERRIDE;
  virtual void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const OVERRIDE;
  virtual void GetDialogSize(gfx::Size* size) const OVERRIDE;
  virtual std::string GetDialogArgs() const OVERRIDE;
  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE;
  virtual void OnCloseContents(TabContents* source,
                               bool* out_close_dialog) OVERRIDE;
  virtual bool ShouldShowDialogTitle() const OVERRIDE;
  virtual bool HandleContextMenu(const ContextMenuParams& params) OVERRIDE;

 private:

  DISALLOW_COPY_AND_ASSIGN(MobileSetupDialogDelegate);
};

// static
void MobileSetupDialog::Show() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  MobileSetupDialogDelegate::GetInstance()->ShowDialog();
}

// static
MobileSetupDialogDelegate* MobileSetupDialogDelegate::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return Singleton<MobileSetupDialogDelegate>::get();
}

MobileSetupDialogDelegate::MobileSetupDialogDelegate() {
}

MobileSetupDialogDelegate::~MobileSetupDialogDelegate() {
}

void MobileSetupDialogDelegate::ShowDialog() {
  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
    return;
  browser->BrowserShowHtmlDialog(this, NULL, STYLE_GENERIC);
}

bool MobileSetupDialogDelegate::IsDialogModal() const {
  return true;
}

string16 MobileSetupDialogDelegate::GetDialogTitle() const {
  return l10n_util::GetStringUTF16(IDS_MOBILE_SETUP_TITLE);
}

GURL MobileSetupDialogDelegate::GetDialogContentURL() const {
  return GURL(chrome::kChromeUIMobileSetupURL);
}

void MobileSetupDialogDelegate::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const{
}

void MobileSetupDialogDelegate::GetDialogSize(gfx::Size* size) const {
#if defined(POST_PORTAL)
  size->SetSize(850, 650);
#else
  size->SetSize(1100, 700);
#endif
}

std::string MobileSetupDialogDelegate::GetDialogArgs() const {
  return std::string();
}

void MobileSetupDialogDelegate::OnDialogClosed(const std::string& json_retval) {
}

void MobileSetupDialogDelegate::OnCloseContents(TabContents* source,
                                        bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool MobileSetupDialogDelegate::ShouldShowDialogTitle() const {
  return true;
}

bool MobileSetupDialogDelegate::HandleContextMenu(
    const ContextMenuParams& params) {
  return true;
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/idle_logout_dialog.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_helper.h"
#include "chrome/browser/chromeos/dbus/power_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/dialog_style.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/result_codes.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace {

// Global singleton instance of our dialog class.
IdleLogoutDialog* g_instance = NULL;
// Height/Width of the logout dialog.
const int kIdleLogoutDialogWidth = 400;
const int kIdleLogoutDialogHeight = 120;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// IdleLogoutDialogHandler

class IdleLogoutDialogHandler : public content::WebUIMessageHandler {
 public:
  IdleLogoutDialogHandler() : contents_(NULL) {}
  virtual void RegisterMessages() OVERRIDE;
  void CloseDialog();

 private:
  void RequestCountdown(const base::ListValue*);
  void RequestLogout(const base::ListValue*);

  content::WebContents* contents_;

  DISALLOW_COPY_AND_ASSIGN(IdleLogoutDialogHandler);
};

////////////////////////////////////////////////////////////////////////////////
// IdleLogoutDialog public static methods

void IdleLogoutDialog::ShowIdleLogoutDialog() {
  if (!g_instance) {
    g_instance = new IdleLogoutDialog();
  }
  g_instance->ShowDialog();
}

void IdleLogoutDialog::CloseIdleLogoutDialog() {
  if (g_instance)
    g_instance->CloseDialog();
}

////////////////////////////////////////////////////////////////////////////////
// IdleLogoutDialog private methods

IdleLogoutDialog::IdleLogoutDialog()
    : contents_(NULL), handler_(NULL) {
}

IdleLogoutDialog::~IdleLogoutDialog() {
}

void IdleLogoutDialog::ShowDialog() {
  Browser* browser = BrowserList::GetLastActive();
  DCHECK(browser);
  handler_ = new IdleLogoutDialogHandler();
  browser->BrowserShowHtmlDialog(this, NULL, STYLE_GENERIC);
}

void IdleLogoutDialog::CloseDialog() {
  contents_ = NULL;
  handler_->CloseDialog();
}

////////////////////////////////////////////////////////////////////////////////
// Overridden from HtmlDialogUIDelegate
ui::ModalType IdleLogoutDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

string16 IdleLogoutDialog::GetDialogTitle() const {
  return string16();
}

GURL IdleLogoutDialog::GetDialogContentURL() const {
  return GURL(chrome::kChromeUIIdleLogoutDialogURL);
}

void IdleLogoutDialog::GetWebUIMessageHandlers(
  std::vector<WebUIMessageHandler*>* handlers) const {
  handlers->push_back(handler_);
}

void IdleLogoutDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kIdleLogoutDialogWidth, kIdleLogoutDialogHeight);
}

std::string IdleLogoutDialog::GetDialogArgs() const {
  return std::string();
}

void IdleLogoutDialog::OnDialogClosed(const std::string& json_retval) {
  g_instance = NULL;
  delete this;
}

void IdleLogoutDialog::OnCloseContents(WebContents* source,
                                       bool* out_close_dialog) {
  NOTIMPLEMENTED();
}

bool IdleLogoutDialog::ShouldShowDialogTitle() const {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// IdleLogoutDialogUI methods

IdleLogoutDialogUI::IdleLogoutDialogUI(content::WebUI* web_ui)
    : HtmlDialogUI(web_ui) {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIIdleLogoutDialogHost);

  source->AddLocalizedString("title", IDS_IDLE_LOGOUT_TITLE);
  source->AddLocalizedString("warning", IDS_IDLE_LOGOUT_WARNING);

  source->set_json_path("strings.js");

  source->add_resource_path("idle_logout_dialog.js",
                            IDR_IDLE_LOGOUT_DIALOG_JS);
  source->add_resource_path("idle_logout_dialog.css",
                            IDR_IDLE_LOGOUT_DIALOG_CSS);

  source->set_default_resource(IDR_IDLE_LOGOUT_DIALOG_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  profile->GetChromeURLDataManager()->AddDataSource(source);
}

////////////////////////////////////////////////////////////////////////////////
// IdleLogoutDialogHandler methods

void IdleLogoutDialogHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("requestCountdown",
      base::Bind(&IdleLogoutDialogHandler::RequestCountdown,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("requestLogout",
      base::Bind(&IdleLogoutDialogHandler::RequestLogout,
                 base::Unretained(this)));
}

void IdleLogoutDialogHandler::CloseDialog() {
  static_cast<HtmlDialogUI*>(web_ui()->GetController())->CloseDialog(NULL);
}

void IdleLogoutDialogHandler::RequestCountdown(const base::ListValue*) {
  if (!chromeos::KioskModeHelper::Get()->is_initialized()) {
    chromeos::KioskModeHelper::Get()->Initialize(
            base::Bind(&IdleLogoutDialogHandler::RequestCountdown,
                       base::Unretained(this), (base::ListValue*) NULL));
    return;
  }
  base::FundamentalValue warning_timeout((int)
      chromeos::KioskModeHelper::Get()->GetIdleLogoutWarningTimeout());
  web_ui()->CallJavascriptFunction("startCountdown", warning_timeout);
}

void IdleLogoutDialogHandler::RequestLogout(const base::ListValue*) {
  BrowserList::AttemptUserExit();
}

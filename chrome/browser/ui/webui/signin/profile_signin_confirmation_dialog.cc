// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/profile_signin_confirmation_dialog.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_contents_modal_dialog.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace content {
class WebContents;
}

namespace {

class ProfileSigninConfirmationHandler : public content::WebUIMessageHandler {
 public:
  ProfileSigninConfirmationHandler(
      const ProfileSigninConfirmationDialog* dialog,
      const base::Closure& cancel_signin,
      const base::Closure& signin_with_new_profile,
      const base::Closure& continue_signin);
  virtual ~ProfileSigninConfirmationHandler();
  virtual void RegisterMessages() OVERRIDE;

 private:
  // content::WebUIMessageHandler implementation.
  void OnCancelButtonClicked(const base::ListValue* args);
  void OnCreateProfileClicked(const base::ListValue* args);
  void OnContinueButtonClicked(const base::ListValue* args);

  // Weak ptr to parent dialog.
  const ProfileSigninConfirmationDialog* dialog_;

  // Dialog button callbacks.
  base::Closure cancel_signin_;
  base::Closure signin_with_new_profile_;
  base::Closure continue_signin_;
};

}  // namespace

ProfileSigninConfirmationHandler::ProfileSigninConfirmationHandler(
      const ProfileSigninConfirmationDialog* dialog,
      const base::Closure& cancel_signin,
      const base::Closure& signin_with_new_profile,
      const base::Closure& continue_signin)
  : dialog_(dialog),
    cancel_signin_(cancel_signin),
    signin_with_new_profile_(signin_with_new_profile),
    continue_signin_(continue_signin) {
}

ProfileSigninConfirmationHandler::~ProfileSigninConfirmationHandler() {
}

void ProfileSigninConfirmationHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("cancel",
      base::Bind(&ProfileSigninConfirmationHandler::OnCancelButtonClicked,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("createNewProfile",
      base::Bind(&ProfileSigninConfirmationHandler::OnCreateProfileClicked,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("continue",
      base::Bind(&ProfileSigninConfirmationHandler::OnContinueButtonClicked,
                 base::Unretained(this)));
}

void ProfileSigninConfirmationHandler::OnCancelButtonClicked(
    const base::ListValue* args) {
  // TODO(dconnelly): redirect back to NTP?
  cancel_signin_.Run();
  dialog_->Close();
}

void ProfileSigninConfirmationHandler::OnCreateProfileClicked(
    const base::ListValue* args) {
  signin_with_new_profile_.Run();
  dialog_->Close();
}

void ProfileSigninConfirmationHandler::OnContinueButtonClicked(
    const base::ListValue* args) {
  continue_signin_.Run();
  dialog_->Close();
}

void ProfileSigninConfirmationDialog::ShowDialog(
    Profile* profile,
    const std::string& username,
    const base::Closure& cancel_signin,
    const base::Closure& signin_with_new_profile,
    const base::Closure& continue_signin) {
  new ProfileSigninConfirmationDialog(profile,
                                      username,
                                      cancel_signin,
                                      signin_with_new_profile,
                                      continue_signin);
}

ProfileSigninConfirmationDialog::ProfileSigninConfirmationDialog(
    Profile* profile,
    const std::string& username,
    const base::Closure& cancel_signin,
    const base::Closure& signin_with_new_profile,
    const base::Closure& continue_signin)
  : username_(username),
    cancel_signin_(cancel_signin),
    signin_with_new_profile_(signin_with_new_profile),
    continue_signin_(continue_signin) {
  Browser* browser = FindBrowserWithProfile(profile,
                                            chrome::GetActiveDesktop());
  if (!browser) {
    DLOG(WARNING) << "No browser found to display the confirmation dialog";
    cancel_signin_.Run();
    return;
  }

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    DLOG(WARNING) << "No web contents found to display the confirmation dialog";
    cancel_signin_.Run();
    return;
  }

  delegate_ = CreateConstrainedWebDialog(profile, this, NULL, web_contents);
}

ProfileSigninConfirmationDialog::~ProfileSigninConfirmationDialog() {
}

void ProfileSigninConfirmationDialog::Close() const {
  closed_by_handler_ = true;
  delegate_->OnDialogCloseFromWebUI();
}

ui::ModalType ProfileSigninConfirmationDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

string16 ProfileSigninConfirmationDialog::GetDialogTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_ENTERPRISE_SIGNIN_PROFILE_LINK_DIALOG_TITLE);
}

GURL ProfileSigninConfirmationDialog::GetDialogContentURL() const {
  return GURL(chrome::kChromeUIProfileSigninConfirmationURL);
}

void ProfileSigninConfirmationDialog::GetWebUIMessageHandlers(
    std::vector<content::WebUIMessageHandler*>* handlers) const {
  handlers->push_back(
      new ProfileSigninConfirmationHandler(this,
                                           cancel_signin_,
                                           signin_with_new_profile_,
                                           continue_signin_));
}

void ProfileSigninConfirmationDialog::GetDialogSize(gfx::Size* size) const {
  const int kMinimumDialogWidth = 480;
  const int kMinimumDialogHeight = 300;
  size->SetSize(kMinimumDialogWidth, kMinimumDialogHeight);
}

std::string ProfileSigninConfirmationDialog::GetDialogArgs() const {
  std::string data;
  DictionaryValue dict;
  dict.SetString("username", username_);
  base::JSONWriter::Write(&dict, &data);
  return data;
}

void ProfileSigninConfirmationDialog::OnDialogClosed(
    const std::string& json_retval) {
  if (!closed_by_handler_)
    cancel_signin_.Run();
}

void ProfileSigninConfirmationDialog::OnCloseContents(
    content::WebContents* source,
    bool* out_close_dialog) {
  if (out_close_dialog)
    *out_close_dialog = true;
}

bool ProfileSigninConfirmationDialog::ShouldShowDialogTitle() const {
  return true;
}

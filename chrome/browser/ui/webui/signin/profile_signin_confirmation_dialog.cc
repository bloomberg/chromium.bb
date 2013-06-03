// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/profile_signin_confirmation_dialog.h"

#include "base/basictypes.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

// ProfileSigninConfirmationHandler --------------------------------------------

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
  web_ui()->RegisterMessageCallback(
      "cancel",
      base::Bind(&ProfileSigninConfirmationHandler::OnCancelButtonClicked,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "createNewProfile",
      base::Bind(&ProfileSigninConfirmationHandler::OnCreateProfileClicked,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "continue",
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

}  // namespace

#if !defined(TOOLKIT_VIEWS)
namespace chrome {
// static
// Declared in browser_dialogs.h
void ShowProfileSigninConfirmationDialog(
    Browser* browser,
    content::WebContents* web_contents,
    Profile* profile,
    const std::string& username,
    const base::Closure& cancel_signin,
    const base::Closure& signin_with_new_profile,
    const base::Closure& continue_signin) {
  ProfileSigninConfirmationDialog::ShowDialog(web_contents,
                                              profile,
                                              username,
                                              cancel_signin,
                                              signin_with_new_profile,
                                              continue_signin);
}
}  // namespace chrome
#endif

// ProfileSigninConfirmationDialog ---------------------------------------------

ProfileSigninConfirmationDialog::ProfileSigninConfirmationDialog(
    content::WebContents* web_contents,
    Profile* profile,
    const std::string& username,
    const base::Closure& cancel_signin,
    const base::Closure& signin_with_new_profile,
    const base::Closure& continue_signin)
  : web_contents_(web_contents),
    profile_(profile),
    username_(username),
    cancel_signin_(cancel_signin),
    signin_with_new_profile_(signin_with_new_profile),
    continue_signin_(continue_signin),
    delegate_(NULL),
    prompt_for_new_profile_(true) {
}

ProfileSigninConfirmationDialog::~ProfileSigninConfirmationDialog() {
}

// static
void ProfileSigninConfirmationDialog::ShowDialog(
  content::WebContents* web_contents,
  Profile* profile,
  const std::string& username,
  const base::Closure& cancel_signin,
  const base::Closure& signin_with_new_profile,
  const base::Closure& continue_signin) {
  ProfileSigninConfirmationDialog* dialog =
      new ProfileSigninConfirmationDialog(web_contents,
                                          profile,
                                          username,
                                          cancel_signin,
                                          signin_with_new_profile,
                                          continue_signin);
  ui::CheckShouldPromptForNewProfile(
      profile,
      // This callback is guaranteed to be invoked, and once it is, the dialog
      // owns itself.
      base::Bind(&ProfileSigninConfirmationDialog::Show,
                 base::Unretained(dialog)));
}

void ProfileSigninConfirmationDialog::Close() const {
  closed_by_handler_ = true;
  delegate_->OnDialogCloseFromWebUI();
}

void ProfileSigninConfirmationDialog::Show(bool prompt) {
  prompt_for_new_profile_ = prompt;
  delegate_ = CreateConstrainedWebDialog(profile_, this, NULL, web_contents_);
}

ui::ModalType ProfileSigninConfirmationDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

string16 ProfileSigninConfirmationDialog::GetDialogTitle() const {
  return l10n_util::GetStringUTF16(IDS_ENTERPRISE_SIGNIN_TITLE);
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
#if defined(OS_WIN)
  const int kMinimumDialogHeight = 180;
#else
  const int kMinimumDialogHeight = 210;
#endif
  const int kProfileCreationMessageHeight = prompt_for_new_profile_ ? 50 : 0;
  size->SetSize(kMinimumDialogWidth,
                kMinimumDialogHeight + kProfileCreationMessageHeight);
}

std::string ProfileSigninConfirmationDialog::GetDialogArgs() const {
  std::string data;
  base::DictionaryValue dict;
  dict.SetString("username", username_);
  dict.SetBoolean("promptForNewProfile", prompt_for_new_profile_);
#if defined(OS_WIN)
  dict.SetBoolean("hideTitle", true);
#endif
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

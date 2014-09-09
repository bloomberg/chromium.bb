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
#include "chrome/grit/chromium_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/base/l10n/l10n_util.h"

// ProfileSigninConfirmationHandler --------------------------------------------

namespace {

class ProfileSigninConfirmationHandler : public content::WebUIMessageHandler {
 public:
  ProfileSigninConfirmationHandler(
      const ProfileSigninConfirmationDialog* dialog,
      ui::ProfileSigninConfirmationDelegate* delegate_);
  virtual ~ProfileSigninConfirmationHandler();
  virtual void RegisterMessages() OVERRIDE;

 private:
  // content::WebUIMessageHandler implementation.
  void OnCancelButtonClicked(const base::ListValue* args);
  void OnCreateProfileClicked(const base::ListValue* args);
  void OnContinueButtonClicked(const base::ListValue* args);

  // Weak ptr to parent dialog.
  const ProfileSigninConfirmationDialog* dialog_;

  // Dialog button handling.
  ui::ProfileSigninConfirmationDelegate* delegate_;
};

ProfileSigninConfirmationHandler::ProfileSigninConfirmationHandler(
    const ProfileSigninConfirmationDialog* dialog,
    ui::ProfileSigninConfirmationDelegate* delegate)
  : dialog_(dialog), delegate_(delegate) {
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
  delegate_->OnCancelSignin();
  dialog_->Close();
}

void ProfileSigninConfirmationHandler::OnCreateProfileClicked(
    const base::ListValue* args) {
  delegate_->OnSigninWithNewProfile();
  dialog_->Close();
}

void ProfileSigninConfirmationHandler::OnContinueButtonClicked(
    const base::ListValue* args) {
  delegate_->OnContinueSignin();
  dialog_->Close();
}

}  // namespace

#if !defined(TOOLKIT_VIEWS) && !defined(OS_MACOSX)
namespace chrome {
// static
// Declared in browser_dialogs.h
void ShowProfileSigninConfirmationDialog(
    Browser* browser,
    content::WebContents* web_contents,
    Profile* profile,
    const std::string& username,
    ui::ProfileSigninConfirmationDelegate* delegate) {
  ProfileSigninConfirmationDialog::ShowDialog(web_contents,
                                              profile,
                                              username,
                                              delegate);
}
}  // namespace chrome
#endif

// ProfileSigninConfirmationDialog ---------------------------------------------

ProfileSigninConfirmationDialog::ProfileSigninConfirmationDialog(
    content::WebContents* web_contents,
    Profile* profile,
    const std::string& username,
    ui::ProfileSigninConfirmationDelegate* delegate)
  : web_contents_(web_contents),
    profile_(profile),
    username_(username),
    signin_delegate_(delegate),
    dialog_delegate_(NULL),
    prompt_for_new_profile_(true) {
}

ProfileSigninConfirmationDialog::~ProfileSigninConfirmationDialog() {
}

// static
void ProfileSigninConfirmationDialog::ShowDialog(
  content::WebContents* web_contents,
  Profile* profile,
  const std::string& username,
  ui::ProfileSigninConfirmationDelegate* delegate) {
  ProfileSigninConfirmationDialog* dialog =
      new ProfileSigninConfirmationDialog(web_contents,
                                          profile,
                                          username,
                                          delegate);
  ui::CheckShouldPromptForNewProfile(
      profile,
      // This callback is guaranteed to be invoked, and once it is, the dialog
      // owns itself.
      base::Bind(&ProfileSigninConfirmationDialog::Show,
                 base::Unretained(dialog)));
}

void ProfileSigninConfirmationDialog::Close() const {
  closed_by_handler_ = true;
  dialog_delegate_->OnDialogCloseFromWebUI();
}

void ProfileSigninConfirmationDialog::Show(bool prompt) {
  prompt_for_new_profile_ = prompt;
  dialog_delegate_ = CreateConstrainedWebDialog(profile_, this, web_contents_);
}

ui::ModalType ProfileSigninConfirmationDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

base::string16 ProfileSigninConfirmationDialog::GetDialogTitle() const {
  return l10n_util::GetStringUTF16(IDS_ENTERPRISE_SIGNIN_TITLE);
}

GURL ProfileSigninConfirmationDialog::GetDialogContentURL() const {
  return GURL(chrome::kChromeUIProfileSigninConfirmationURL);
}

void ProfileSigninConfirmationDialog::GetWebUIMessageHandlers(
    std::vector<content::WebUIMessageHandler*>* handlers) const {
  handlers->push_back(
      new ProfileSigninConfirmationHandler(this, signin_delegate_));
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
    signin_delegate_->OnCancelSignin();
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

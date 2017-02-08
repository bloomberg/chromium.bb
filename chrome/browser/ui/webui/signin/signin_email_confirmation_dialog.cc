// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_email_confirmation_dialog.h"

#include <vector>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/signin/signin_email_confirmation_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Dialog size.
const int kDialogWidth = 512;
const int kDialogMinHeight = 200;
const int kDialogMaxHeight = 700;

// Dialog action key;
const char kActionKey[] = "action";

// Dialog action values.
const char kActionCancel[] = "cancel";
const char kActionCreateNewUser[] = "createNewUser";
const char kActionStartSync[] = "startSync";

}  // namespace

class SigninEmailConfirmationDialog::DialogWebContentsObserver
    : public content::WebContentsObserver {
 public:
  DialogWebContentsObserver(content::WebContents* web_contents,
                            SigninEmailConfirmationDialog* dialog)
      : content::WebContentsObserver(web_contents),
        signin_email_confirmation_dialog_(dialog) {}
  ~DialogWebContentsObserver() override {}

 private:
  void WebContentsDestroyed() override {
    // The dialog is already closed. No need to call CloseDialog() again.
    // NOTE: |this| is deleted after |ResetDialogObserver| returns.
    signin_email_confirmation_dialog_->ResetDialogObserver();
  }

  void RenderProcessGone(base::TerminationStatus status) override {
    signin_email_confirmation_dialog_->CloseDialog();
  }

  SigninEmailConfirmationDialog* signin_email_confirmation_dialog_;
};

SigninEmailConfirmationDialog::SigninEmailConfirmationDialog(
    content::WebContents* contents,
    Profile* profile,
    const std::string& last_email,
    const std::string& new_email,
    const Callback& callback)
    : web_contents_(contents),
      profile_(profile),
      last_email_(last_email),
      new_email_(new_email),
      callback_(callback) {}

SigninEmailConfirmationDialog::~SigninEmailConfirmationDialog() {}

// static
void SigninEmailConfirmationDialog::AskForConfirmation(
    content::WebContents* contents,
    Profile* profile,
    const std::string& last_email,
    const std::string& email,
    const Callback& callback) {
  content::RecordAction(
      base::UserMetricsAction("Signin_Show_ImportDataPrompt"));
  SigninEmailConfirmationDialog* dialog = new SigninEmailConfirmationDialog(
      contents, profile, last_email, email, callback);
  dialog->ShowDialog();
}

void SigninEmailConfirmationDialog::ShowDialog() {
  gfx::Size minSize(kDialogWidth, kDialogMinHeight);
  gfx::Size maxSize(kDialogWidth, kDialogMaxHeight);
  ConstrainedWebDialogDelegate* dialog_delegate =
      ShowConstrainedWebDialogWithAutoResize(profile_, this, web_contents_,
                                             minSize, maxSize);

  content::WebContents* dialog_web_contents = dialog_delegate->GetWebContents();
  dialog_observer_.reset(
      new DialogWebContentsObserver(dialog_web_contents, this));
}

void SigninEmailConfirmationDialog::CloseDialog() {
  content::WebContents* dialog_web_contents = GetDialogWebContents();
  if (dialog_web_contents == nullptr)
    return;
  content::WebUI* web_ui = dialog_web_contents->GetWebUI();
  if (web_ui) {
    SigninEmailConfirmationUI* signin_email_confirmation_ui =
        static_cast<SigninEmailConfirmationUI*>(web_ui->GetController());
    if (signin_email_confirmation_ui)
      signin_email_confirmation_ui->Close();
  }
}

void SigninEmailConfirmationDialog::ResetDialogObserver() {
  dialog_observer_.reset();
}

content::WebContents* SigninEmailConfirmationDialog::GetDialogWebContents()
    const {
  return dialog_observer_.get() ? dialog_observer_->web_contents() : nullptr;
}

// ui::WebDialogDelegate implementation

ui::ModalType SigninEmailConfirmationDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

base::string16 SigninEmailConfirmationDialog::GetDialogTitle() const {
  return base::string16();
}

GURL SigninEmailConfirmationDialog::GetDialogContentURL() const {
  return GURL(chrome::kChromeUISigninEmailConfirmationURL);
}

void SigninEmailConfirmationDialog::GetWebUIMessageHandlers(
    std::vector<content::WebUIMessageHandler*>* handlers) const {}

void SigninEmailConfirmationDialog::GetDialogSize(gfx::Size* size) const {
  DCHECK(size);

  // Set the dialog width if it's not set, so that the dialog is center-aligned
  // horizontally when it appears. Avoid setting a dialog height in here as
  // this dialog auto-resizes.
  if (size->IsEmpty())
    size->set_width(kDialogWidth);
}

std::string SigninEmailConfirmationDialog::GetDialogArgs() const {
  std::string data;
  base::DictionaryValue dialog_args;
  dialog_args.SetString("lastEmail", last_email_);
  dialog_args.SetString("newEmail", new_email_);
  base::JSONWriter::Write(dialog_args, &data);
  return data;
}

void SigninEmailConfirmationDialog::OnDialogClosed(
    const std::string& json_retval) {
  Action action = CLOSE;
  std::unique_ptr<base::DictionaryValue> ret_value(
      base::DictionaryValue::From(base::JSONReader::Read(json_retval)));
  if (ret_value) {
    std::string action_string;
    if (ret_value->GetString(kActionKey, &action_string)) {
      if (action_string == kActionCancel) {
        action = CLOSE;
      } else if (action_string == kActionCreateNewUser) {
        action = CREATE_NEW_USER;
      } else if (action_string == kActionStartSync) {
        action = START_SYNC;
      } else {
        NOTREACHED() << "Unexpected action value [" << action_string << "]";
      }
    } else {
      NOTREACHED() << "No action in the dialog close return arguments";
    }
  } else {
    // If the dialog is dismissed without any return value, then simply close
    // the dialog. (see http://crbug.com/667690)
    action = CLOSE;
  }

  if (!callback_.is_null()) {
    callback_.Run(action);
    callback_.Reset();
  }
}

void SigninEmailConfirmationDialog::OnCloseContents(
    content::WebContents* source,
    bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool SigninEmailConfirmationDialog::ShouldShowDialogTitle() const {
  return false;
}

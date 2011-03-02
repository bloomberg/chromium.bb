// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager_delegate_impl.h"

#include "base/metrics/histogram.h"
#include "base/singleton.h"
#include "chrome/browser/password_manager/password_form_manager.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/common/autofill_messages.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/glue/password_form.h"

// After a successful *new* login attempt, we take the PasswordFormManager in
// provisional_save_manager_ and move it to a SavePasswordInfoBarDelegate while
// the user makes up their mind with the "save password" infobar. Note if the
// login is one we already know about, the end of the line is
// provisional_save_manager_ because we just update it on success and so such
// forms never end up in an infobar.
class SavePasswordInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  SavePasswordInfoBarDelegate(TabContents* tab_contents,
                              PasswordFormManager* form_to_save);

 private:
  enum ResponseType {
    NO_RESPONSE = 0,
    REMEMBER_PASSWORD,
    DONT_REMEMBER_PASSWORD,
    NUM_RESPONSE_TYPES,
  };

  virtual ~SavePasswordInfoBarDelegate();

  // ConfirmInfoBarDelegate
  virtual void InfoBarClosed();
  virtual SkBitmap* GetIcon() const;
  virtual Type GetInfoBarType() const;
  virtual string16 GetMessageText() const;
  virtual string16 GetButtonLabel(InfoBarButton button) const;
  virtual bool Accept();
  virtual bool Cancel();

  // The PasswordFormManager managing the form we're asking the user about,
  // and should update as per her decision.
  scoped_ptr<PasswordFormManager> form_to_save_;

  // Used to track the results we get from the info bar.
  ResponseType infobar_response_;

  DISALLOW_COPY_AND_ASSIGN(SavePasswordInfoBarDelegate);
};

SavePasswordInfoBarDelegate::SavePasswordInfoBarDelegate(
    TabContents* tab_contents,
    PasswordFormManager* form_to_save)
    : ConfirmInfoBarDelegate(tab_contents),
      form_to_save_(form_to_save),
      infobar_response_(NO_RESPONSE) {
}

SavePasswordInfoBarDelegate::~SavePasswordInfoBarDelegate() {
}

void SavePasswordInfoBarDelegate::InfoBarClosed() {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.InfoBarResponse",
                            infobar_response_, NUM_RESPONSE_TYPES);
  delete this;
}

SkBitmap* SavePasswordInfoBarDelegate::GetIcon() const {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_INFOBAR_SAVE_PASSWORD);
}

InfoBarDelegate::Type SavePasswordInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

string16 SavePasswordInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SAVE_PASSWORD_PROMPT);
}

string16 SavePasswordInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_PASSWORD_MANAGER_SAVE_BUTTON : IDS_PASSWORD_MANAGER_BLACKLIST_BUTTON);
}

bool SavePasswordInfoBarDelegate::Accept() {
  DCHECK(form_to_save_.get());
  form_to_save_->Save();
  infobar_response_ = REMEMBER_PASSWORD;
  return true;
}

bool SavePasswordInfoBarDelegate::Cancel() {
  DCHECK(form_to_save_.get());
  form_to_save_->PermanentlyBlacklist();
  infobar_response_ = DONT_REMEMBER_PASSWORD;
  return true;
}


// PasswordManagerDelegateImpl ------------------------------------------------

void PasswordManagerDelegateImpl::FillPasswordForm(
    const webkit_glue::PasswordFormFillData& form_data) {
  tab_contents_->render_view_host()->Send(new AutoFillMsg_FillPasswordForm(
      tab_contents_->render_view_host()->routing_id(), form_data));
}

void PasswordManagerDelegateImpl::AddSavePasswordInfoBar(
    PasswordFormManager* form_to_save) {
  tab_contents_->AddInfoBar(
      new SavePasswordInfoBarDelegate(tab_contents_, form_to_save));
}

Profile* PasswordManagerDelegateImpl::GetProfileForPasswordManager() {
  return tab_contents_->profile();
}

bool PasswordManagerDelegateImpl::DidLastPageLoadEncounterSSLErrors() {
  return tab_contents_->controller().ssl_manager()->
      ProcessedSSLErrorFromRequest();
}

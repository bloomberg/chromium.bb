// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager_delegate_impl.h"

#include "app/l10n_util.h"
#include "base/metrics/histogram.h"
#include "base/singleton.h"
#include "chrome/browser/password_manager/password_form_manager.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
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
                              PasswordFormManager* form_to_save)
      : ConfirmInfoBarDelegate(tab_contents),
        form_to_save_(form_to_save),
        infobar_response_(NO_RESPONSE) {}

  virtual ~SavePasswordInfoBarDelegate() {}

  // Begin ConfirmInfoBarDelegate implementation.
  virtual void InfoBarClosed() {
    UMA_HISTOGRAM_ENUMERATION("PasswordManager.InfoBarResponse",
                              infobar_response_, NUM_RESPONSE_TYPES);
    delete this;
  }

  virtual Type GetInfoBarType() { return PAGE_ACTION_TYPE; }

  virtual string16 GetMessageText() const {
    return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SAVE_PASSWORD_PROMPT);
  }

  virtual SkBitmap* GetIcon() const {
    return ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_INFOBAR_SAVE_PASSWORD);
  }

  virtual int GetButtons() const {
    return BUTTON_OK | BUTTON_CANCEL;
  }

  virtual string16 GetButtonLabel(InfoBarButton button) const {
    if (button == BUTTON_OK)
      return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SAVE_BUTTON);
    if (button == BUTTON_CANCEL)
      return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_BLACKLIST_BUTTON);
    NOTREACHED();
    return string16();
  }

  virtual bool Accept() {
    DCHECK(form_to_save_.get());
    form_to_save_->Save();
    infobar_response_ = REMEMBER_PASSWORD;
    return true;
  }

  virtual bool Cancel() {
    DCHECK(form_to_save_.get());
    form_to_save_->PermanentlyBlacklist();
    infobar_response_ = DONT_REMEMBER_PASSWORD;
    return true;
  }
  // End ConfirmInfoBarDelegate implementation.

 private:
  // The PasswordFormManager managing the form we're asking the user about,
  // and should update as per her decision.
  scoped_ptr<PasswordFormManager> form_to_save_;

  // Used to track the results we get from the info bar.
  enum ResponseType {
    NO_RESPONSE = 0,
    REMEMBER_PASSWORD,
    DONT_REMEMBER_PASSWORD,
    NUM_RESPONSE_TYPES,
  };
  ResponseType infobar_response_;

  DISALLOW_COPY_AND_ASSIGN(SavePasswordInfoBarDelegate);
};

//----------------------------------------------------------------------------

void PasswordManagerDelegateImpl::FillPasswordForm(
    const webkit_glue::PasswordFormFillData& form_data) {
  tab_contents_->render_view_host()->FillPasswordForm(form_data);
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

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager_delegate_impl.h"

#include "base/memory/singleton.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/password_manager/password_form_manager.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/ui/sync/one_click_signin_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/autofill_messages.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/ssl_status.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources_standard.h"
#include "net/base/cert_status_flags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/forms/password_form.h"

// After a successful *new* login attempt, we take the PasswordFormManager in
// provisional_save_manager_ and move it to a SavePasswordInfoBarDelegate while
// the user makes up their mind with the "save password" infobar. Note if the
// login is one we already know about, the end of the line is
// provisional_save_manager_ because we just update it on success and so such
// forms never end up in an infobar.
class SavePasswordInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  SavePasswordInfoBarDelegate(InfoBarTabHelper* infobar_helper,
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
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

  virtual InfoBarAutomationType GetInfoBarAutomationType() const OVERRIDE;

  // The PasswordFormManager managing the form we're asking the user about,
  // and should update as per her decision.
  scoped_ptr<PasswordFormManager> form_to_save_;

  // Used to track the results we get from the info bar.
  ResponseType infobar_response_;

  DISALLOW_COPY_AND_ASSIGN(SavePasswordInfoBarDelegate);
};

SavePasswordInfoBarDelegate::SavePasswordInfoBarDelegate(
    InfoBarTabHelper* infobar_helper,
    PasswordFormManager* form_to_save)
    : ConfirmInfoBarDelegate(infobar_helper),
      form_to_save_(form_to_save),
      infobar_response_(NO_RESPONSE) {
}

SavePasswordInfoBarDelegate::~SavePasswordInfoBarDelegate() {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.InfoBarResponse",
                            infobar_response_, NUM_RESPONSE_TYPES);
}

gfx::Image* SavePasswordInfoBarDelegate::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
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

InfoBarDelegate::InfoBarAutomationType
    SavePasswordInfoBarDelegate::GetInfoBarAutomationType() const {
  return PASSWORD_INFOBAR;
}

// PasswordManagerDelegateImpl ------------------------------------------------

void PasswordManagerDelegateImpl::FillPasswordForm(
    const webkit::forms::PasswordFormFillData& form_data) {
  bool disable_popup = tab_contents_->autofill_manager()->HasExternalDelegate();

  tab_contents_->web_contents()->GetRenderViewHost()->Send(
      new AutofillMsg_FillPasswordForm(
          tab_contents_->web_contents()->GetRenderViewHost()->GetRoutingID(),
          form_data,
          disable_popup));
}

void PasswordManagerDelegateImpl::AddSavePasswordInfoBarIfPermitted(
    PasswordFormManager* form_to_save) {
  // Don't show the password manager infobar if this form is for a google
  // account and we are going to show the one-click singin infobar.
  // For now, one-click signin is fully implemented only on windows.
#if defined(ENABLE_ONE_CLICK_SIGNIN)
  GURL realm(form_to_save->realm());
  if (realm == GURL(GaiaUrls::GetInstance()->gaia_login_form_realm()) &&
      OneClickSigninHelper::CanOffer(tab_contents_->web_contents(), true)) {
    return;
  }
#endif

  tab_contents_->infobar_tab_helper()->AddInfoBar(
      new SavePasswordInfoBarDelegate(
          tab_contents_->infobar_tab_helper(), form_to_save));
}

Profile* PasswordManagerDelegateImpl::GetProfile() {
  return tab_contents_->profile();
}

bool PasswordManagerDelegateImpl::DidLastPageLoadEncounterSSLErrors() {
  content::NavigationEntry* entry =
      tab_contents_->web_contents()->GetController().GetActiveEntry();
  if (!entry) {
    NOTREACHED();
    return false;
  }

  return net::IsCertStatusError(entry->GetSSL().cert_status);
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/offers_private_api.h"

#include <string>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/system/statistics_provider.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_window_controller.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// For a given coupon type, returns the coupon code value from the underlying
// system.
std::string GetValueForCouponType(std::string& type) {
  // Possible ECHO code type and corresponding key name in StatisticsProvider.
  const std::string kCouponType = "COUPON_CODE";
  const std::string kCouponCodeKey = "ubind_attribute";
  const std::string kGroupType = "GROUP_CODE";
  const std::string kGroupCodeKey = "gbind_attribute";

  chromeos::system::StatisticsProvider* provider =
      chromeos::system::StatisticsProvider::GetInstance();
  std::string result;
  if (type == kCouponType)
    provider->GetMachineStatistic(kCouponCodeKey, &result);
  else if (type == kGroupType)
    provider->GetMachineStatistic(kGroupCodeKey, &result);
  return result;
}

}  // namespace


GetCouponCodeFunction::GetCouponCodeFunction() {
}

GetCouponCodeFunction::~GetCouponCodeFunction() {
}

bool GetCouponCodeFunction::RunImpl() {
  std::string type;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &type));
  result_.reset(Value::CreateStringValue(GetValueForCouponType(type)));
  return true;
}

// Confirmation InfoBar displayed to ask user for consent for performing
// the ECHO protocol transaction. Used by GetUserConsentFunction.
class OffersConsentInfoBarDelegate
    : public ConfirmInfoBarDelegate,
      public content::NotificationObserver {
 public:
  OffersConsentInfoBarDelegate(Browser* browser,
                               InfoBarTabHelper* infobar_helper,
                               const GURL& url,
                               GetUserConsentFunction* consent_receiver);
 private:
  virtual ~OffersConsentInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual void InfoBarDismissed() OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  content::NotificationRegistrar registrar_;
  Browser* browser_;

  // The extension host we are showing the InfoBar for. The delegate needs to
  // own this since the InfoBar gets deleted and recreated when you switch tabs
  // and come back (and we don't want the user's interaction with the InfoBar to
  // get lost at that point).
  scoped_ptr<ExtensionHost> extension_host_;

  scoped_refptr<GetUserConsentFunction> consent_receiver_;
};

OffersConsentInfoBarDelegate::OffersConsentInfoBarDelegate(
    Browser* browser,
    InfoBarTabHelper* infobar_helper,
    const GURL& url,
    GetUserConsentFunction* consent_receiver)
    : ConfirmInfoBarDelegate(infobar_helper),
      browser_(browser),
      consent_receiver_(consent_receiver) {
  ExtensionProcessManager* manager =
      browser->profile()->GetExtensionProcessManager();
  extension_host_.reset(manager->CreateInfobarHost(url, browser));
  extension_host_->SetAssociatedWebContents(infobar_helper->web_contents());

  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
                 content::Source<Profile>(browser->profile()));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(browser->profile()));
}

OffersConsentInfoBarDelegate::~OffersConsentInfoBarDelegate() {
}

string16 OffersConsentInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_OFFERS_CONSENT_INFOBAR_LABEL);
}

int OffersConsentInfoBarDelegate::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

string16 OffersConsentInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK(button == BUTTON_OK || button == BUTTON_CANCEL);
  if (button == BUTTON_OK)
    return l10n_util::GetStringUTF16(IDS_OFFERS_CONSENT_INFOBAR_ENABLE_BUTTON);
  return l10n_util::GetStringUTF16(IDS_OFFERS_CONSENT_INFOBAR_DISABLE_BUTTON);
}

bool OffersConsentInfoBarDelegate::Accept() {
  consent_receiver_->SetConsent(true);
  return true;
}

bool OffersConsentInfoBarDelegate::Cancel() {
  consent_receiver_->SetConsent(false);
  return true;
}

void OffersConsentInfoBarDelegate::InfoBarDismissed() {
  // Assume no consent if the user closes the Info Bar without
  // pressing any of the buttons.
  consent_receiver_->SetConsent(false);
}

void OffersConsentInfoBarDelegate::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE ||
      type == chrome::NOTIFICATION_EXTENSION_UNLOADED) {
    if (extension_host_.get() == content::Details<ExtensionHost>(details).ptr())
      RemoveSelf();
  }
}

GetUserConsentFunction::GetUserConsentFunction() {
}

void GetUserConsentFunction::SetConsent(bool is_consent) {
  result_.reset(Value::CreateBooleanValue(is_consent));
  SendResponse(true);
  Release();
}

GetUserConsentFunction::~GetUserConsentFunction() {
}

bool GetUserConsentFunction::RunImpl() {
  AddRef();  // Balanced in SetConsent().
  ShowConsentInfoBar();
  return true;
}

void GetUserConsentFunction::ShowConsentInfoBar() {
  const Extension* extension = GetExtension();
  GURL url = extension->GetResourceURL("");

  Browser* browser = GetCurrentBrowser();

  TabContentsWrapper* tab_contents = NULL;
  tab_contents = browser->GetSelectedTabContentsWrapper();
  tab_contents->infobar_tab_helper()->AddInfoBar(
      new OffersConsentInfoBarDelegate(browser,
                                       tab_contents->infobar_tab_helper(),
                                       url,
                                       this));
  DCHECK(browser->extension_window_controller());
}

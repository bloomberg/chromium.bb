// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/chrome_signin_delegate.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/webui/sync_promo/sync_promo_ui.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/common/page_transition_types.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/app_list/signin_delegate_observer.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

SigninManager* GetSigninManager(Profile* profile) {
  return SigninManagerFactory::GetForProfile(profile);
}

}  // namespace

ChromeSigninDelegate::ChromeSigninDelegate(Profile* profile)
    : profile_(profile) {}

bool ChromeSigninDelegate::NeedSignin()  {
#if defined(OS_CHROMEOS)
  return false;
#else
  if (!profile_)
    return false;

  if (!GetSigninManager(profile_))
    return false;

  return GetSigninManager(profile_)->GetAuthenticatedUsername().empty();
#endif
}

void ChromeSigninDelegate::ShowSignin() {
  DCHECK(profile_);

  signin_tracker_.reset(new SigninTracker(profile_, this));

  Browser* browser = FindOrCreateTabbedBrowser(profile_,
                                               chrome::GetActiveDesktop());
  chrome::ShowBrowserSignin(browser, SyncPromoUI::SOURCE_APP_LAUNCHER);
}

void ChromeSigninDelegate::OpenLearnMore() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  GURL gurl(rb.GetLocalizedString(IDS_APP_LIST_SIGNIN_LEARN_MORE_LINK));
  chrome::NavigateParams params(profile_, gurl, content::PAGE_TRANSITION_LINK);
  chrome::Navigate(&params);
}

void ChromeSigninDelegate::OpenSettings() {
  ExtensionService* service = profile_->GetExtensionService();
  DCHECK(service);
  const extensions::Extension* extension = service->GetInstalledExtension(
      extension_misc::kSettingsAppId);
  if (!extension)
    return;

  chrome::OpenApplication(chrome::AppLaunchParams(
      profile_, extension, NEW_FOREGROUND_TAB));
}

string16 ChromeSigninDelegate::GetSigninHeading() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetLocalizedString(IDS_APP_LIST_SIGNIN_HEADING);
}

string16 ChromeSigninDelegate::GetSigninText() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetLocalizedString(IDS_APP_LIST_SIGNIN_TEXT);
}

string16 ChromeSigninDelegate::GetSigninButtonText() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetLocalizedString(IDS_APP_LIST_SIGNIN_BUTTON);
}

string16 ChromeSigninDelegate::GetLearnMoreLinkText() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetLocalizedString(IDS_APP_LIST_SIGNIN_LEARN_MORE_TEXT);
}

string16 ChromeSigninDelegate::GetSettingsLinkText() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetLocalizedString(IDS_APP_LIST_SIGNIN_SETTINGS_TEXT);
}

void ChromeSigninDelegate::GaiaCredentialsValid() {}

ChromeSigninDelegate::~ChromeSigninDelegate() {}

void ChromeSigninDelegate::SigninFailed(const GoogleServiceAuthError& error) {}

void ChromeSigninDelegate::SigninSuccess() {
  NotifySigninSuccess();
}

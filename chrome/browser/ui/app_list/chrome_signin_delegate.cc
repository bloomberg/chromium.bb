// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/chrome_signin_delegate.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "ui/app_list/signin_delegate_observer.h"

#if defined(ENABLE_ONE_CLICK_SIGNIN)
#include "chrome/browser/ui/sync/one_click_signin_helper.h"
#endif

using content::WebContents;

namespace {

ProfileSyncService* GetSyncService(Profile* profile) {
  return ProfileSyncServiceFactory::GetForProfile(profile);
}

LoginUIService* GetLoginUIService(Profile* profile) {
  return LoginUIServiceFactory::GetForProfile(profile);
}

SigninManager* GetSigninManager(Profile* profile) {
  return SigninManagerFactory::GetForProfile(profile);
}

}  // namespace

ChromeSigninDelegate::ChromeSigninDelegate(Profile* profile)
    : profile_(profile) {}

bool ChromeSigninDelegate::NeedSignin()  {
#if defined(USE_ASH)
  return false;
#else
  if (!profile_)
    return false;

  if (!GetSigninManager(profile_))
    return false;

  return GetSigninManager(profile_)->GetAuthenticatedUsername().empty();
#endif
}

WebContents* ChromeSigninDelegate::PrepareForSignin() {
  DCHECK(!IsActiveSignin());

  if (!GetLoginUIService(profile_))
    return NULL;

  signin_tracker_.reset(new SigninTracker(profile_, this));

  if (GetLoginUIService(profile_)->current_login_ui())
    GetLoginUIService(profile_)->current_login_ui()->CloseUI();

  GetLoginUIService(profile_)->SetLoginUI(this);

  if (!GetSyncService(profile_))
    return NULL;

  GetSyncService(profile_)->UnsuppressAndStart();

#if defined(ENABLE_ONE_CLICK_SIGNIN)
  GURL url("https://accounts.google.com/ServiceLogin?service=chromiumsync&"
           "sarp=1&rm=hide&continue=https://www.google.com/intl/en-US/"
           "chrome/blank.html?source=2");

  WebContents::CreateParams params =  WebContents::CreateParams(
      profile_, content::SiteInstance::CreateForURL(profile_, url));
  WebContents* web_contents = WebContents::Create(params);
  if (OneClickSigninHelper::CanOffer(
      web_contents, OneClickSigninHelper::CAN_OFFER_FOR_ALL, "", NULL)) {
    OneClickSigninHelper::CreateForWebContents(web_contents);
  }

  web_contents->GetController().LoadURL(
      url, content::Referrer(), content::PAGE_TRANSITION_LINK,
      std::string());
  return web_contents;
#else
  NOTREACHED();
  return NULL;
#endif
}

ChromeSigninDelegate::~ChromeSigninDelegate() {
  FinishSignin();
}

bool ChromeSigninDelegate::IsActiveSignin() {
  if (!GetLoginUIService(profile_))
    return false;

  return GetLoginUIService(profile_)->current_login_ui() == this;
}

void ChromeSigninDelegate::FinishSignin() {
  if (!IsActiveSignin())
    return;

  GetLoginUIService(profile_)->LoginUIClosed(this);
}

void ChromeSigninDelegate::FocusUI() {
  // The launcher gets hidden if it is not focused, so if it is visible it
  // is focused. Hence, nothing to do here.
}

void ChromeSigninDelegate::CloseUI() {
  // TODO: remove tab contents helper
  // This can't happen, as the launcher keeps focus, but do something sensible
  // nonetheless.
  FinishSignin();
}

void ChromeSigninDelegate::GaiaCredentialsValid() {
}

void ChromeSigninDelegate::SigninFailed(const GoogleServiceAuthError& error) {
}

void ChromeSigninDelegate::SigninSuccess() {
  if (!IsActiveSignin())
    return;

  FinishSignin();
  NotifySigninSuccess();
}

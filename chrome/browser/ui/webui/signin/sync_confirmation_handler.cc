// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/sync_confirmation_handler.h"

#include <vector>

#include "base/bind.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/signin_view_controller_delegate.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "url/gurl.h"

const int kProfileImageSize = 128;

SyncConfirmationHandler::SyncConfirmationHandler(Browser* browser)
    : profile_(browser->profile()),
      browser_(browser),
      did_user_explicitly_interact(false) {
  DCHECK(profile_);
  DCHECK(browser_);
  BrowserList::AddObserver(this);
}

SyncConfirmationHandler::~SyncConfirmationHandler() {
  BrowserList::RemoveObserver(this);
  AccountTrackerServiceFactory::GetForProfile(profile_)->RemoveObserver(this);

  // Abort signin and prevent sync from starting if none of the actions on the
  // sync confirmation dialog are taken by the user.
  if (!did_user_explicitly_interact) {
    HandleUndo(nullptr);
    base::RecordAction(base::UserMetricsAction("Signin_Abort_Signin"));
  }
}

void SyncConfirmationHandler::OnBrowserRemoved(Browser* browser) {
  if (browser_ == browser)
    browser_ = nullptr;
}

void SyncConfirmationHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("confirm",
      base::Bind(&SyncConfirmationHandler::HandleConfirm,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("undo",
      base::Bind(&SyncConfirmationHandler::HandleUndo, base::Unretained(this)));
  web_ui()->RegisterMessageCallback("initializedWithSize",
      base::Bind(&SyncConfirmationHandler::HandleInitializedWithSize,
                 base::Unretained(this)));
}

void SyncConfirmationHandler::HandleConfirm(const base::ListValue* args) {
  did_user_explicitly_interact = true;
  bool configure_sync_first = false;
  CHECK(args->GetBoolean(0, &configure_sync_first));
  CloseModalSigninWindow(configure_sync_first
                             ? LoginUIService::CONFIGURE_SYNC_FIRST
                             : LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
}

void SyncConfirmationHandler::HandleUndo(const base::ListValue* args) {
  did_user_explicitly_interact = true;
  base::RecordAction(base::UserMetricsAction("Signin_Undo_Signin"));
  LoginUIServiceFactory::GetForProfile(profile_)->SyncConfirmationUIClosed(
      LoginUIService::ABORT_SIGNIN);
  SigninManagerFactory::GetForProfile(profile_)->SignOut(
      signin_metrics::ABORT_SIGNIN,
      signin_metrics::SignoutDelete::IGNORE_METRIC);

  if (browser_)
    browser_->signin_view_controller()->CloseModalSignin();
}

void SyncConfirmationHandler::SetUserImageURL(const std::string& picture_url) {
  std::string picture_url_to_load;
  GURL picture_gurl(picture_url);
  if (picture_gurl.is_valid()) {
    picture_url_to_load =
        profiles::GetImageURLWithOptions(picture_gurl, kProfileImageSize,
                                         false /* no_silhouette */)
            .spec();
  } else {
    // Use the placeholder avatar icon until the account picture URL is fetched.
    picture_url_to_load = profiles::GetPlaceholderAvatarIconUrl();
  }
  base::Value picture_url_value(picture_url_to_load);
  web_ui()->CallJavascriptFunctionUnsafe("sync.confirmation.setUserImageURL",
                                         picture_url_value);
}

void SyncConfirmationHandler::OnAccountUpdated(const AccountInfo& info) {
  if (!info.IsValid())
    return;

  SigninManager* signin_manager = SigninManagerFactory::GetForProfile(profile_);
  if (info.account_id != signin_manager->GetAuthenticatedAccountId())
    return;

  AccountTrackerServiceFactory::GetForProfile(profile_)->RemoveObserver(this);
  SetUserImageURL(info.picture_url);
}

void SyncConfirmationHandler::CloseModalSigninWindow(
    LoginUIService::SyncConfirmationUIClosedResult result) {
  LoginUIServiceFactory::GetForProfile(profile_)->SyncConfirmationUIClosed(
      result);
  if (browser_)
    browser_->signin_view_controller()->CloseModalSignin();
}

void SyncConfirmationHandler::HandleInitializedWithSize(
    const base::ListValue* args) {
  if (!browser_)
    return;

  std::string account_id = SigninManagerFactory::GetForProfile(profile_)
                               ->GetAuthenticatedAccountId();
  if (account_id.empty()) {
    // No account is signed in, so there is nothing to be displayed in the sync
    // confirmation dialog.
    return;
  }
  AccountTrackerService* account_tracker =
      AccountTrackerServiceFactory::GetForProfile(profile_);
  AccountInfo account_info = account_tracker->GetAccountInfo(account_id);

  if (!account_info.IsValid()) {
    SetUserImageURL(AccountTrackerService::kNoPictureURLFound);
    account_tracker->AddObserver(this);
  } else {
    SetUserImageURL(account_info.picture_url);
  }

  signin::SetInitializedModalHeight(browser_, web_ui(), args);

  // After the dialog is shown, some platforms might have an element focused.
  // To be consistent, clear the focused element on all platforms.
  // TODO(anthonyvd): Figure out why this is needed on Mac and not other
  // platforms and if there's a way to start unfocused while avoiding this
  // workaround.
  web_ui()->CallJavascriptFunctionUnsafe("sync.confirmation.clearFocus");
}

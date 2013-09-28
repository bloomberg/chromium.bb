// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_global_error.h"

#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

SigninGlobalError::SigninGlobalError(Profile* profile)
    : auth_error_(GoogleServiceAuthError::AuthErrorNone()), profile_(profile) {
}

SigninGlobalError::~SigninGlobalError() {
  DCHECK(provider_set_.empty())
      << "All AuthStatusProviders should be unregistered before"
      << " SigninManager::Shutdown() is called";
}

void SigninGlobalError::AddProvider(const AuthStatusProvider* provider) {
  DCHECK(provider_set_.find(provider) == provider_set_.end())
      << "Adding same AuthStatusProvider multiple times";
  provider_set_.insert(provider);
  AuthStatusChanged();
}

void SigninGlobalError::RemoveProvider(const AuthStatusProvider* provider) {
  std::set<const AuthStatusProvider*>::iterator iter =
      provider_set_.find(provider);
  DCHECK(iter != provider_set_.end())
      << "Removing provider that was never added";
  provider_set_.erase(iter);
  AuthStatusChanged();
}

SigninGlobalError::AuthStatusProvider::AuthStatusProvider() {
}

SigninGlobalError::AuthStatusProvider::~AuthStatusProvider() {
}

void SigninGlobalError::AuthStatusChanged() {
  // Walk all of the status providers and collect any error.
  GoogleServiceAuthError current_error(GoogleServiceAuthError::AuthErrorNone());
  std::string current_account_id;
  for (std::set<const AuthStatusProvider*>::const_iterator it =
           provider_set_.begin(); it != provider_set_.end(); ++it) {
    current_account_id = (*it)->GetAccountId();
    current_error = (*it)->GetAuthStatus();

    // Break out if any provider reports an error (ignoring ordinary network
    // errors, which are not surfaced to the user). This logic may eventually
    // need to be extended to prioritize different auth errors, but for now
    // all auth errors are treated the same.
    if (current_error.state() != GoogleServiceAuthError::NONE &&
        current_error.state() != GoogleServiceAuthError::CONNECTION_FAILED) {
      break;
    }
  }
  if (current_error.state() != auth_error_.state() ||
      account_id_ != current_account_id) {
    auth_error_ = current_error;
    if (auth_error_.state() == GoogleServiceAuthError::NONE) {
      account_id_.clear();
    } else {
      account_id_ = current_account_id;
    }

    GlobalErrorServiceFactory::GetForProfile(profile_)->NotifyErrorsChanged(
        this);
  }
}

bool SigninGlobalError::HasMenuItem() {
  return !MenuItemLabel().empty();
}

int SigninGlobalError::MenuItemCommandID() {
  return IDC_SHOW_SIGNIN_ERROR;
}

string16 SigninGlobalError::MenuItemLabel() {
  if (account_id_.empty() ||
      auth_error_.state() == GoogleServiceAuthError::NONE ||
      auth_error_.state() == GoogleServiceAuthError::CONNECTION_FAILED) {
    // If the user isn't signed in, or there's no auth error worth elevating to
    // the user, don't display any menu item.
    return string16();
  } else {
    // There's an auth error the user should know about - notify the user.
    return l10n_util::GetStringUTF16(IDS_SYNC_SIGN_IN_ERROR_WRENCH_MENU_ITEM);
  }
}

void SigninGlobalError::ExecuteMenuItem(Browser* browser) {
#if defined(OS_CHROMEOS)
  if (auth_error_.state() != GoogleServiceAuthError::NONE) {
    DLOG(INFO) << "Signing out the user to fix a sync error.";
    // TODO(beng): seems like this could just call chrome::AttemptUserExit().
    chrome::ExecuteCommand(browser, IDC_EXIT);
    return;
  }
#endif

  // TODO(rogerta): what we do depends on which account is reporting an error.
  // This will be needed once the account reconcilor is implemented.  The
  // LoginUIService will support multi-login as well.

  // Global errors don't show up in the wrench menu on android.
#if !defined(OS_ANDROID)
  LoginUIService* login_ui = LoginUIServiceFactory::GetForProfile(profile_);
  if (login_ui->current_login_ui()) {
    login_ui->current_login_ui()->FocusUI();
    return;
  }
  // Need to navigate to the settings page and display the UI.
  chrome::ShowSettingsSubPage(browser, chrome::kSyncSetupSubPage);
#endif
}

bool SigninGlobalError::HasBubbleView() {
  return !GetBubbleViewMessages().empty();
}

string16 SigninGlobalError::GetBubbleViewTitle() {
  return l10n_util::GetStringUTF16(IDS_SIGNIN_ERROR_BUBBLE_VIEW_TITLE);
}

std::vector<string16> SigninGlobalError::GetBubbleViewMessages() {
  std::vector<string16> messages;

  // If the user isn't signed in, no need to display an error bubble.
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfileIfExists(profile_);
  if (signin_manager) {
    std::string username = signin_manager->GetAuthenticatedUsername();
    if (username.empty())
      return messages;
  }

  switch (auth_error_.state()) {
    // In the case of no error, or a simple network error, don't bother
    // displaying a popup bubble.
    case GoogleServiceAuthError::CONNECTION_FAILED:
    case GoogleServiceAuthError::NONE:
      return messages;

    // TODO(rogerta): use account id in error messages.

    // User credentials are invalid (bad acct, etc).
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
    case GoogleServiceAuthError::ACCOUNT_DELETED:
    case GoogleServiceAuthError::ACCOUNT_DISABLED:
      messages.push_back(l10n_util::GetStringFUTF16(
          IDS_SYNC_SIGN_IN_ERROR_BUBBLE_VIEW_MESSAGE,
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
      break;

    // Sync service is not available for this account's domain.
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
      messages.push_back(l10n_util::GetStringFUTF16(
          IDS_SYNC_UNAVAILABLE_ERROR_BUBBLE_VIEW_MESSAGE,
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
      break;

    // Generic message for "other" errors.
    default:
      messages.push_back(l10n_util::GetStringFUTF16(
          IDS_SYNC_OTHER_SIGN_IN_ERROR_BUBBLE_VIEW_MESSAGE,
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
  }
  return messages;
}

string16 SigninGlobalError::GetBubbleViewAcceptButtonLabel() {
  // If the service is unavailable, don't give the user the option to try
  // signing in again.
  if (auth_error_.state() == GoogleServiceAuthError::SERVICE_UNAVAILABLE) {
    return l10n_util::GetStringUTF16(
        IDS_SYNC_UNAVAILABLE_ERROR_BUBBLE_VIEW_ACCEPT);
  } else {
    return l10n_util::GetStringUTF16(IDS_SYNC_SIGN_IN_ERROR_BUBBLE_VIEW_ACCEPT);
  }
}

string16 SigninGlobalError::GetBubbleViewCancelButtonLabel() {
  return string16();
}

void SigninGlobalError::OnBubbleViewDidClose(Browser* browser) {
}

void SigninGlobalError::BubbleViewAcceptButtonPressed(Browser* browser) {
  ExecuteMenuItem(browser);
}

void SigninGlobalError::BubbleViewCancelButtonPressed(Browser* browser) {
  NOTREACHED();
}

// static
SigninGlobalError* SigninGlobalError::GetForProfile(Profile* profile) {
  return ProfileOAuth2TokenServiceFactory::GetForProfile(profile)->
      signin_global_error();
}

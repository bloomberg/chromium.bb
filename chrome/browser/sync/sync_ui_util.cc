// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_ui_util.h"

#include "base/i18n/number_formatting.h"
#include "base/i18n/time_formatting.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/sessions/sync_session_snapshot.h"
#include "sync/protocol/proto_enum_conversions.h"
#include "sync/protocol/sync_protocol_error.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

typedef GoogleServiceAuthError AuthError;

namespace sync_ui_util {

namespace {

// Given an authentication state this helper function returns various labels
// that can be used to display information about the state.
void GetStatusLabelsForAuthError(const AuthError& auth_error,
                                 const ProfileSyncService& service,
                                 string16* status_label,
                                 string16* link_label,
                                 string16* global_error_menu_label,
                                 string16* global_error_bubble_message,
                                 string16* global_error_bubble_accept_label) {
  string16 username = UTF8ToUTF16(service.profile()->GetPrefs()->GetString(
      prefs::kGoogleServicesUsername));
  string16 product_name = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
  if (link_label)
    link_label->assign(l10n_util::GetStringUTF16(IDS_SYNC_RELOGIN_LINK_LABEL));

  switch (auth_error.state()) {
    case AuthError::INVALID_GAIA_CREDENTIALS:
    case AuthError::ACCOUNT_DELETED:
    case AuthError::ACCOUNT_DISABLED:
      // If the user name is empty then the first login failed, otherwise the
      // credentials are out-of-date.
      if (username.empty()) {
        if (status_label) {
          status_label->assign(
              l10n_util::GetStringUTF16(IDS_SYNC_INVALID_USER_CREDENTIALS));
        }
      } else {
        if (status_label) {
          status_label->assign(
              l10n_util::GetStringUTF16(IDS_SYNC_LOGIN_INFO_OUT_OF_DATE));
        }
        if (global_error_menu_label) {
          global_error_menu_label->assign(l10n_util::GetStringUTF16(
              IDS_SYNC_SIGN_IN_ERROR_WRENCH_MENU_ITEM));
        }
        if (global_error_bubble_message) {
          global_error_bubble_message->assign(l10n_util::GetStringFUTF16(
              IDS_SYNC_SIGN_IN_ERROR_BUBBLE_VIEW_MESSAGE, product_name));
        }
        if (global_error_bubble_accept_label) {
          global_error_bubble_accept_label->assign(l10n_util::GetStringUTF16(
              IDS_SYNC_SIGN_IN_ERROR_BUBBLE_VIEW_ACCEPT));
        }
      }
      break;
    case AuthError::SERVICE_UNAVAILABLE:
      if (status_label) {
        status_label->assign(
            l10n_util::GetStringUTF16(IDS_SYNC_SERVICE_UNAVAILABLE));
      }
      if (link_label)
        link_label->clear();
      if (global_error_menu_label) {
        global_error_menu_label->assign(l10n_util::GetStringUTF16(
            IDS_SYNC_SIGN_IN_ERROR_WRENCH_MENU_ITEM));
      }
      if (global_error_bubble_message) {
        global_error_bubble_message->assign(l10n_util::GetStringFUTF16(
            IDS_SYNC_UNAVAILABLE_ERROR_BUBBLE_VIEW_MESSAGE, product_name));
      }
      if (global_error_bubble_accept_label) {
        global_error_bubble_accept_label->assign(l10n_util::GetStringUTF16(
            IDS_SYNC_UNAVAILABLE_ERROR_BUBBLE_VIEW_ACCEPT));
      }
      break;
    case AuthError::CONNECTION_FAILED:
      // Note that there is little the user can do if the server is not
      // reachable. Since attempting to re-connect is done automatically by
      // the Syncer, we do not show the (re)login link.
      if (status_label) {
        status_label->assign(
            l10n_util::GetStringFUTF16(IDS_SYNC_SERVER_IS_UNREACHABLE,
                                       product_name));
      }
      break;
    default:
      if (status_label) {
        status_label->assign(l10n_util::GetStringUTF16(
            IDS_SYNC_ERROR_SIGNING_IN));
      }
      if (global_error_menu_label) {
        global_error_menu_label->assign(l10n_util::GetStringUTF16(
            IDS_SYNC_SIGN_IN_ERROR_WRENCH_MENU_ITEM));
      }
      if (global_error_bubble_message) {
        global_error_bubble_message->assign(l10n_util::GetStringFUTF16(
            IDS_SYNC_OTHER_SIGN_IN_ERROR_BUBBLE_VIEW_MESSAGE, product_name));
      }
      if (global_error_bubble_accept_label) {
        global_error_bubble_accept_label->assign(l10n_util::GetStringUTF16(
            IDS_SYNC_SIGN_IN_ERROR_BUBBLE_VIEW_ACCEPT));
      }
      break;
  }
}

// Returns the message that should be displayed when the user is authenticated
// and can connect to the sync server. If the user hasn't yet authenticated, an
// empty string is returned.
string16 GetSyncedStateStatusLabel(ProfileSyncService* service,
                                   StatusLabelStyle style) {
  if (!service->sync_initialized())
    return string16();

  string16 user_name = UTF8ToUTF16(service->profile()->GetPrefs()->GetString(
      prefs::kGoogleServicesUsername));
  DCHECK(!user_name.empty());

  // Message may also carry additional advice with an HTML link, if acceptable.
  switch (style) {
    case PLAIN_TEXT:
      return l10n_util::GetStringFUTF16(
          IDS_SYNC_ACCOUNT_SYNCING_TO_USER,
          user_name);
    case WITH_HTML:
      return l10n_util::GetStringFUTF16(
          IDS_SYNC_ACCOUNT_SYNCING_TO_USER_WITH_MANAGE_LINK,
          user_name,
          ASCIIToUTF16(chrome::kSyncGoogleDashboardURL));
    default:
      NOTREACHED();
      return NULL;
  }
}

void GetStatusForActionableError(
    const syncer::SyncProtocolError& error,
    string16* status_label) {
  DCHECK(status_label);
  switch (error.action) {
    case syncer::STOP_AND_RESTART_SYNC:
       status_label->assign(
           l10n_util::GetStringUTF16(IDS_SYNC_STOP_AND_RESTART_SYNC));
      break;
    case syncer::UPGRADE_CLIENT:
       status_label->assign(
           l10n_util::GetStringFUTF16(IDS_SYNC_UPGRADE_CLIENT,
               l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
      break;
    case syncer::ENABLE_SYNC_ON_ACCOUNT:
       status_label->assign(
           l10n_util::GetStringUTF16(IDS_SYNC_ENABLE_SYNC_ON_ACCOUNT));
    break;
    case syncer::CLEAR_USER_DATA_AND_RESYNC:
       status_label->assign(
           l10n_util::GetStringUTF16(IDS_SYNC_CLEAR_USER_DATA));
      break;
    default:
      NOTREACHED();
  }
}

// TODO(akalin): Write unit tests for these three functions below.

// status_label and link_label must either be both NULL or both non-NULL.
MessageType GetStatusInfo(ProfileSyncService* service,
                          const SigninManager& signin,
                          StatusLabelStyle style,
                          string16* status_label,
                          string16* link_label) {
  DCHECK_EQ(status_label == NULL, link_label == NULL);

  MessageType result_type(SYNCED);

  if (!service) {
    return PRE_SYNCED;
  }

  if (service->HasSyncSetupCompleted()) {
    ProfileSyncService::Status status;
    service->QueryDetailedSyncStatus(&status);
    const AuthError& auth_error = service->GetAuthError();

    // The order or priority is going to be: 1. Unrecoverable errors.
    // 2. Auth errors. 3. Protocol errors. 4. Passphrase errors.

    if (service->HasUnrecoverableError()) {
      if (status_label) {
        status_label->assign(l10n_util::GetStringFUTF16(
            IDS_SYNC_STATUS_UNRECOVERABLE_ERROR,
            l10n_util::GetStringUTF16(IDS_SYNC_UNRECOVERABLE_ERROR_HELP_URL)));
      }
      return SYNC_ERROR;
    }

    // For auth errors first check if an auth is in progress.
    if (signin.AuthInProgress()) {
      if (status_label) {
        status_label->assign(
          l10n_util::GetStringUTF16(IDS_SYNC_AUTHENTICATING_LABEL));
      }
      return PRE_SYNCED;
    }

    // No auth in progress check for an auth error.
    if (auth_error.state() != AuthError::NONE) {
      if (status_label && link_label) {
        GetStatusLabelsForAuthError(auth_error, *service,
                                    status_label, link_label, NULL, NULL, NULL);
      }
      return SYNC_ERROR;
    }

    // We dont have an auth error. Check for protocol error.
    if (ShouldShowActionOnUI(status.sync_protocol_error)) {
      if (status_label) {
        GetStatusForActionableError(status.sync_protocol_error,
            status_label);
      }
      return SYNC_ERROR;
    }

    // Now finally passphrase error.
    if (service->IsPassphraseRequired()) {
      if (service->IsPassphraseRequiredForDecryption()) {
        // TODO(lipalani) : Ask tim if this is still needed.
        // NOT first machine.
        // Show a link ("needs attention"), but still indicate the
        // current synced status.  Return SYNC_PROMO so that
        // the configure link will still be shown.
        if (status_label && link_label) {
          status_label->assign(GetSyncedStateStatusLabel(service, style));
          link_label->assign(
              l10n_util::GetStringUTF16(IDS_SYNC_PASSWORD_SYNC_ATTENTION));
        }
        return SYNC_PROMO;
      }
    }

    // There is no error. Display "Last synced..." message.
    if (status_label)
      status_label->assign(GetSyncedStateStatusLabel(service, style));
    return SYNCED;
  } else {
    // Either show auth error information with a link to re-login, auth in prog,
    // or provide a link to continue with setup.
    result_type = PRE_SYNCED;
    if (service->FirstSetupInProgress()) {
      ProfileSyncService::Status status;
      service->QueryDetailedSyncStatus(&status);
      const AuthError& auth_error = service->GetAuthError();
      if (status_label) {
        status_label->assign(
            l10n_util::GetStringUTF16(IDS_SYNC_NTP_SETUP_IN_PROGRESS));
      }
      if (signin.AuthInProgress()) {
        if (status_label) {
          status_label->assign(
              l10n_util::GetStringUTF16(IDS_SYNC_AUTHENTICATING_LABEL));
        }
      } else if (auth_error.state() != AuthError::NONE &&
                 auth_error.state() != AuthError::TWO_FACTOR) {
        if (status_label) {
          status_label->clear();
          GetStatusLabelsForAuthError(auth_error, *service, status_label, NULL,
                                      NULL, NULL, NULL);
        }
        result_type = SYNC_ERROR;
      }
    } else if (service->HasUnrecoverableError()) {
      result_type = SYNC_ERROR;
      ProfileSyncService::Status status;
      service->QueryDetailedSyncStatus(&status);
      if (ShouldShowActionOnUI(status.sync_protocol_error)) {
        if (status_label) {
          GetStatusForActionableError(status.sync_protocol_error,
              status_label);
        }
      } else if (status_label) {
        status_label->assign(l10n_util::GetStringUTF16(IDS_SYNC_SETUP_ERROR));
      }
    }
  }
  return result_type;
}

// Returns the status info for use on the new tab page, where we want slightly
// different information than in the settings panel.
MessageType GetStatusInfoForNewTabPage(ProfileSyncService* service,
                                       const SigninManager& signin,
                                       string16* status_label,
                                       string16* link_label) {
  DCHECK(status_label);
  DCHECK(link_label);

  if (service->HasSyncSetupCompleted() &&
      service->IsPassphraseRequired()) {
    if (service->passphrase_required_reason() == syncer::REASON_ENCRYPTION) {
      // First machine migrating to passwords.  Show as a promotion.
      if (status_label && link_label) {
        status_label->assign(
            l10n_util::GetStringFUTF16(
                IDS_SYNC_NTP_PASSWORD_PROMO,
                l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
        link_label->assign(
            l10n_util::GetStringUTF16(IDS_SYNC_NTP_PASSWORD_ENABLE));
      }
      return SYNC_PROMO;
    } else {
      // NOT first machine.
      // Show a link and present as an error ("needs attention").
      if (status_label && link_label) {
        status_label->assign(string16());
        link_label->assign(
            l10n_util::GetStringUTF16(IDS_SYNC_CONFIGURE_ENCRYPTION));
      }
      return SYNC_ERROR;
    }
  }

  // Fallback to default.
  return GetStatusInfo(service, signin, WITH_HTML, status_label, link_label);
}

}  // namespace

MessageType GetStatusLabels(ProfileSyncService* service,
                            const SigninManager& signin,
                            StatusLabelStyle style,
                            string16* status_label,
                            string16* link_label) {
  DCHECK(status_label);
  DCHECK(link_label);
  return sync_ui_util::GetStatusInfo(
      service, signin, style, status_label, link_label);
}

MessageType GetStatusLabelsForNewTabPage(ProfileSyncService* service,
                                         const SigninManager& signin,
                                         string16* status_label,
                                         string16* link_label) {
  DCHECK(status_label);
  DCHECK(link_label);
  return sync_ui_util::GetStatusInfoForNewTabPage(
      service, signin, status_label, link_label);
}

void GetStatusLabelsForSyncGlobalError(ProfileSyncService* service,
                                       const SigninManager& signin,
                                       string16* menu_label,
                                       string16* bubble_message,
                                       string16* bubble_accept_label) {
  DCHECK(menu_label);
  DCHECK(bubble_message);
  DCHECK(bubble_accept_label);
  *menu_label = string16();
  *bubble_message = string16();
  *bubble_accept_label = string16();

  if (!service->HasSyncSetupCompleted())
    return;

  MessageType status = GetStatus(service, signin);
  if (status == SYNC_ERROR) {
    const AuthError& auth_error = service->GetAuthError();
    if (auth_error.state() != AuthError::NONE) {
      GetStatusLabelsForAuthError(auth_error, *service, NULL, NULL,
          menu_label, bubble_message, bubble_accept_label);
      // If we have an actionable auth error, display it.
      if (!menu_label->empty())
        return;
    }
  }

  // No actionable auth error - display the passphrase error.
  if (service->IsPassphraseRequired() &&
      service->IsPassphraseRequiredForDecryption()) {
    // This is not the first machine so ask user to enter passphrase.
    *menu_label = l10n_util::GetStringUTF16(
        IDS_SYNC_PASSPHRASE_ERROR_WRENCH_MENU_ITEM);
    string16 product_name = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
    *bubble_message = l10n_util::GetStringFUTF16(
        IDS_SYNC_PASSPHRASE_ERROR_BUBBLE_VIEW_MESSAGE, product_name);
    *bubble_accept_label = l10n_util::GetStringUTF16(
        IDS_SYNC_PASSPHRASE_ERROR_BUBBLE_VIEW_ACCEPT);
    return;
  }
}

MessageType GetStatus(
    ProfileSyncService* service, const SigninManager& signin) {
  return sync_ui_util::GetStatusInfo(service, signin, WITH_HTML, NULL, NULL);
}

string16 GetSyncMenuLabel(
    ProfileSyncService* service, const SigninManager& signin) {
  MessageType type = GetStatus(service, signin);

  if (type == sync_ui_util::SYNCED)
    return l10n_util::GetStringUTF16(IDS_SYNC_MENU_SYNCED_LABEL);
  else if (type == sync_ui_util::SYNC_ERROR)
    return l10n_util::GetStringUTF16(IDS_SYNC_MENU_SYNC_ERROR_LABEL);
  else
    return l10n_util::GetStringUTF16(IDS_SYNC_START_SYNC_BUTTON_LABEL);
}

string16 ConstructTime(int64 time_in_int) {
  base::Time time = base::Time::FromInternalValue(time_in_int);

  // If time is null the format function returns a time in 1969.
  if (time.is_null())
    return string16();
  return base::TimeFormatFriendlyDateAndTime(time);
}

std::string MakeSyncAuthErrorText(
    const GoogleServiceAuthError::State& state) {
  switch (state) {
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
    case GoogleServiceAuthError::ACCOUNT_DELETED:
    case GoogleServiceAuthError::ACCOUNT_DISABLED:
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
      return "INVALID_GAIA_CREDENTIALS";
    case GoogleServiceAuthError::USER_NOT_SIGNED_UP:
      return "USER_NOT_SIGNED_UP";
    case GoogleServiceAuthError::CONNECTION_FAILED:
      return "CONNECTION_FAILED";
    default:
      return std::string();
  }
}

}  // namespace sync_ui_util

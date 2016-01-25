// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_METRICS_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_METRICS_H_

#include <limits.h>

#include "base/time/time.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace signin_metrics {

// Enum for the ways in which primary account detection is done.
enum DifferentPrimaryAccounts {
  // token and cookie had same primary accounts.
  ACCOUNTS_SAME = 0,
  // Deprecated. Indicates different primary accounts.
  UNUSED_ACCOUNTS_DIFFERENT,
  // No GAIA cookie present, so the primaries are considered different.
  NO_COOKIE_PRESENT,
  // There was at least one cookie and one token, and the primaries differed.
  COOKIE_AND_TOKEN_PRIMARIES_DIFFERENT,
  NUM_DIFFERENT_PRIMARY_ACCOUNT_METRICS,
};

// Track all the ways a profile can become signed out as a histogram.
enum ProfileSignout {
  // The value used within unit tests
  SIGNOUT_TEST = 0,
  // The preference or policy controlling if signin is valid has changed.
  SIGNOUT_PREF_CHANGED = 0,
  // The valid pattern for signing in to the Google service changed.
  GOOGLE_SERVICE_NAME_PATTERN_CHANGED,
  // The preference or policy controlling if signin is valid changed during
  // the signin process.
  SIGNIN_PREF_CHANGED_DURING_SIGNIN,
  // User clicked to signout from the settings page.
  USER_CLICKED_SIGNOUT_SETTINGS,
  // The signin process was aborted, but signin had succeeded, so signout. This
  // may be due to a server response, policy definition or user action.
  ABORT_SIGNIN,
  // The sync server caused the profile to be signed out.
  SERVER_FORCED_DISABLE,
  // The credentials are being transfered to a new profile, so the old one is
  // signed out.
  TRANSFER_CREDENTIALS,

  // Keep this as the last enum.
  NUM_PROFILE_SIGNOUT_METRICS,
};

// Enum values used for use with "AutoLogin.Reverse" histograms.
enum {
  // The infobar was shown to the user.
  HISTOGRAM_SHOWN,
  // The user pressed the accept button to perform the suggested action.
  HISTOGRAM_ACCEPTED,
  // The user pressed the reject to turn off the feature.
  HISTOGRAM_REJECTED,
  // The user pressed the X button to dismiss the infobar this time.
  HISTOGRAM_DISMISSED,
  // The user completely ignored the infoar.  Either they navigated away, or
  // they used the page as is.
  HISTOGRAM_IGNORED,
  // The user clicked on the learn more link in the infobar.
  HISTOGRAM_LEARN_MORE,
  // The sync was started with default settings.
  HISTOGRAM_WITH_DEFAULTS,
  // The sync was started with advanced settings.
  HISTOGRAM_WITH_ADVANCED,
  // The sync was started through auto-accept with default settings.
  HISTOGRAM_AUTO_WITH_DEFAULTS,
  // The sync was started through auto-accept with advanced settings.
  HISTOGRAM_AUTO_WITH_ADVANCED,
  // The sync was aborted with an undo button.
  HISTOGRAM_UNDO,
  HISTOGRAM_MAX
};

// Enum values used with the "Signin.OneClickConfirmation" histogram, which
// tracks the actions used in the OneClickConfirmation bubble.
enum {
  HISTOGRAM_CONFIRM_SHOWN,
  HISTOGRAM_CONFIRM_OK,
  HISTOGRAM_CONFIRM_RETURN,
  HISTOGRAM_CONFIRM_ADVANCED,
  HISTOGRAM_CONFIRM_CLOSE,
  HISTOGRAM_CONFIRM_ESCAPE,
  HISTOGRAM_CONFIRM_UNDO,
  HISTOGRAM_CONFIRM_LEARN_MORE,
  HISTOGRAM_CONFIRM_LEARN_MORE_OK,
  HISTOGRAM_CONFIRM_LEARN_MORE_RETURN,
  HISTOGRAM_CONFIRM_LEARN_MORE_ADVANCED,
  HISTOGRAM_CONFIRM_LEARN_MORE_CLOSE,
  HISTOGRAM_CONFIRM_LEARN_MORE_ESCAPE,
  HISTOGRAM_CONFIRM_LEARN_MORE_UNDO,
  HISTOGRAM_CONFIRM_MAX
};

// TODO(gogerald): right now, gaia server needs to distinguish the source from
// signin_metrics::SOURCE_START_PAGE, signin_metrics::SOURCE_SETTINGS and the
// others to show advanced sync setting, remove them after switching to Minute
// Maid sign in flow.
// This was previously used in Signin.SigninSource UMA histogram, but no longer
// used after having below AccessPoint and Reason related histograms.
enum Source {
  SOURCE_START_PAGE = 0,  // This must be first.
  SOURCE_SETTINGS = 3,
  SOURCE_OTHERS = 13,
};

// Enum values which enumerates all access points where sign in could be
// initiated. Not all of them exist on all platforms. They are used with
// "Signin.SigninStartedAccessPoint" and "Signin.SigninCompletedAccessPoint"
// histograms.
enum class AccessPoint : int {
  ACCESS_POINT_START_PAGE = 0,
  ACCESS_POINT_NTP_LINK,
  ACCESS_POINT_MENU,
  ACCESS_POINT_SETTINGS,
  ACCESS_POINT_SUPERVISED_USER,
  ACCESS_POINT_EXTENSION_INSTALL_BUBBLE,
  ACCESS_POINT_EXTENSIONS,
  ACCESS_POINT_APPS_PAGE_LINK,
  ACCESS_POINT_BOOKMARK_BUBBLE,
  ACCESS_POINT_BOOKMARK_MANAGER,
  ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN,
  ACCESS_POINT_USER_MANAGER,
  ACCESS_POINT_DEVICES_PAGE,
  ACCESS_POINT_CLOUD_PRINT,
  ACCESS_POINT_CONTENT_AREA,
  ACCESS_POINT_SIGNIN_PROMO,
  ACCESS_POINT_RECENT_TABS,
  ACCESS_POINT_UNKNOWN,  // This should never have been used to get signin URL.
  ACCESS_POINT_MAX,      // This must be last.
};

// Enum values which enumerates all reasons to start sign in process.
enum class Reason : int {
  REASON_SIGNIN_PRIMARY_ACCOUNT = 0,
  REASON_ADD_SECONDARY_ACCOUNT,
  REASON_REAUTHENTICATION,
  REASON_UNLOCK,
  REASON_UNKNOWN_REASON,  // This should never have been used to get signin URL.
  REASON_MAX,             // This must be last.
};

// Enum values used for use with the "Signin.Reauth" histogram.
enum AccountReauth {
  // The user gave the wrong email when doing a reauthentication.
  HISTOGRAM_ACCOUNT_MISSMATCH,
  // The user was shown a reauthentication login screen.
  HISTOGRAM_REAUTH_SHOWN,

  HISTOGRAM_REAUTH_MAX
};

// Enum values used for "Signin.XDevicePromo.Eligible" histogram, which tracks
// the reasons for which a profile is or is not eligible for the promo.
enum CrossDevicePromoEligibility {
  // The user is eligible for the promo.
  ELIGIBLE,
  // The profile has previously opted out of the promo.
  OPTED_OUT,
  // The profile is already signed in.
  SIGNED_IN,
  // The profile does not have a single, peristent GAIA cookie.
  NOT_SINGLE_GAIA_ACCOUNT,
  // Yet to determine how many devices the user has.
  UNKNOWN_COUNT_DEVICES,
  // An error was returned trying to determine the account's devices.
  ERROR_FETCHING_DEVICE_ACTIVITY,
  // The call to get device activity was throttled, and never executed.
  THROTTLED_FETCHING_DEVICE_ACTIVITY,
  // The user has no devices.
  ZERO_DEVICES,
  // The user has no device that was recently active.
  NO_ACTIVE_DEVICES,
  // Always last enumerated type.
  NUM_CROSS_DEVICE_PROMO_ELIGIBILITY_METRICS
};

// Enum reasons the CrossDevicePromo couldn't initialize, or that it succeeded.
enum CrossDevicePromoInitialized {
  // The promo was initialized successfully.
  INITIALIZED,
  // The profile is opted out, so the promo didn't initialize.
  UNINITIALIZED_OPTED_OUT,
  // Unable to read the variations configuration.
  NO_VARIATIONS_CONFIG,
  // Always the last enumerated type.
  NUM_CROSS_DEVICE_PROMO_INITIALIZED_METRICS
};

// Enum values used for "Signin.AccountReconcilorState.OnGaiaResponse"
// histogram, which records the state of the AccountReconcilor when GAIA returns
// a specific response.
enum AccountReconcilorState {
  // The AccountReconcilor has finished running ans is up-to-date.
  ACCOUNT_RECONCILOR_OK,
  // The AccountReconcilor is running and gathering information.
  ACCOUNT_RECONCILOR_RUNNING,
  // The AccountReconcilor encountered an error and stopped.
  ACCOUNT_RECONCILOR_ERROR,
  // Always the last enumerated type.
  ACCOUNT_RECONCILOR_HISTOGRAM_COUNT,
};

// Values of histogram comparing account id and email.
enum class AccountEquality : int {
  // Expected case when the user is not switching accounts.
  BOTH_EQUAL = 0,
  // Expected case when the user is switching accounts.
  BOTH_DIFFERENT,
  // The user has changed at least two email account names. This is actually
  // a different account, even though the email matches.
  ONLY_SAME_EMAIL,
  // The user has changed the email of their account, but the account is
  // actually the same.
  ONLY_SAME_ID,
  // The last account id was not present, email equality was used. This should
  // happen once to all old clients. Does not differentiate between same and
  // different accounts.
  EMAIL_FALLBACK,
  // Always the last enumerated type.
  HISTOGRAM_COUNT,
};

// When the user is give a choice of deleting their profile or not when signing
// out, the |DELETED| or |KEEPING| metric should be used. If the user is not
// given any option, then use the |IGNORE_METRIC| value should be used.
enum class SignoutDelete : int {
  DELETED = 0,
  KEEPING,
  IGNORE_METRIC,
};

// Tracks the access point of sign in.
void LogSigninAccessPointStarted(AccessPoint access_point);
void LogSigninAccessPointCompleted(AccessPoint access_point);

// Tracks the reason of sign in.
void LogSigninReason(Reason reason);

// Log to UMA histograms and UserCounts stats about a single execution of the
// AccountReconciler.
// |total_number_accounts| - How many accounts are in the browser for this
//                           profile.
// |count_added_to_cookie_jar| - How many accounts were in the browser but not
//                               in the cookie jar.
// |count_removed_from_cookie_jar| - How many accounts were in the cookie jar
//                                   but not in the browser.
// |primary_accounts_same| - False if the primary account for the cookie jar
//                           and the token service were different; else true.
// |is_first_reconcile| - True if these stats are from the first execution of
//                        the AccountReconcilor.
// |pre_count_gaia_cookies| - How many GAIA cookies were present before
//                            the AccountReconcilor began modifying the state.
void LogSigninAccountReconciliation(int total_number_accounts,
                                    int count_added_to_cookie_jar,
                                    int count_removed_from_cookie_jar,
                                    bool primary_accounts_same,
                                    bool is_first_reconcile,
                                    int pre_count_gaia_cookies);

// Logs duration of a single execution of AccountReconciler to UMA histograms.
// |duration| - How long execution of AccountReconciler took.
// |successful| - True if AccountReconciler was successful.
void LogSigninAccountReconciliationDuration(base::TimeDelta duration,
                                            bool successful);

// Track a successful signin.
void LogSigninAddAccount();

// Track a successful signin of a profile.
void LogSigninProfile(bool is_first_run, base::Time install_date);

// Track a profile signout.
void LogSignout(ProfileSignout source_metric, SignoutDelete delete_metric);

// Tracks whether the external connection results were all fetched before
// the gaia cookie manager service tried to use them with merge session.
// |time_to_check_connections| is the time it took to complete.
void LogExternalCcResultFetches(
    bool fetches_completed,
    const base::TimeDelta& time_to_check_connections);

// Track when the current authentication error changed.
void LogAuthError(GoogleServiceAuthError::State auth_error);

void LogSigninConfirmHistogramValue(int action);

void LogXDevicePromoEligible(CrossDevicePromoEligibility metric);

void LogXDevicePromoInitialized(CrossDevicePromoInitialized metric);

void LogBrowsingSessionDuration(const base::Time& previous_activity_time);

// Records the AccountReconcilor |state| when GAIA returns a specific response.
// If |state| is different than ACCOUNT_RECONCILOR_OK it means the user will
// be shown a different set of accounts in the content-area and the settings UI.
void LogAccountReconcilorStateOnGaiaResponse(AccountReconcilorState state);

// Records the AccountEquality metric when an investigator compares the current
// and previous id/emails during a signin.
void LogAccountEquality(AccountEquality equality);

}  // namespace signin_metrics

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_METRICS_H_

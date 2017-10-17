// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_METRICS_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_METRICS_UTIL_H_

#include <stddef.h>

#include <string>

#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/common/credential_manager_types.h"

namespace password_manager {

namespace metrics_util {

// Metrics: "PasswordManager.InfoBarResponse"
enum ResponseType {
  NO_RESPONSE = 0,
  REMEMBER_PASSWORD,
  NEVER_REMEMBER_PASSWORD,
  INFOBAR_DISMISSED,
  NUM_RESPONSE_TYPES,
};

// Metrics: "PasswordBubble.DisplayDisposition"
enum UIDisplayDisposition {
  AUTOMATIC_WITH_PASSWORD_PENDING = 0,
  MANUAL_WITH_PASSWORD_PENDING,
  MANUAL_MANAGE_PASSWORDS,
  MANUAL_BLACKLISTED_OBSOLETE,  // obsolete.
  AUTOMATIC_GENERATED_PASSWORD_CONFIRMATION,
  AUTOMATIC_CREDENTIAL_REQUEST_OBSOLETE,  // obsolete
  AUTOMATIC_SIGNIN_TOAST,
  MANUAL_WITH_PASSWORD_PENDING_UPDATE,
  AUTOMATIC_WITH_PASSWORD_PENDING_UPDATE,
  NUM_DISPLAY_DISPOSITIONS
};

// Metrics: "PasswordManager.UIDismissalReason"
enum UIDismissalReason {
  // We use this to mean both "Bubble lost focus" and "No interaction with the
  // infobar", depending on which experiment is active.
  NO_DIRECT_INTERACTION = 0,
  CLICKED_SAVE,
  CLICKED_CANCEL,
  CLICKED_NEVER,
  CLICKED_MANAGE,
  CLICKED_DONE,
  CLICKED_UNBLACKLIST_OBSOLETE,  // obsolete.
  CLICKED_OK,
  CLICKED_CREDENTIAL_OBSOLETE,  // obsolete.
  AUTO_SIGNIN_TOAST_TIMEOUT,
  AUTO_SIGNIN_TOAST_CLICKED_OBSOLETE,  // obsolete.
  CLICKED_BRAND_NAME,
  CLICKED_PASSWORDS_DASHBOARD,
  NUM_UI_RESPONSES,
};

enum FormDeserializationStatus {
  LOGIN_DATABASE_SUCCESS,
  LOGIN_DATABASE_FAILURE,
  LIBSECRET_SUCCESS,
  LIBSECRET_FAILURE,
  GNOME_SUCCESS,
  GNOME_FAILURE,
  NUM_DESERIALIZATION_STATUSES
};

// Metrics: "PasswordManager.PasswordSyncState"
enum PasswordSyncState {
  SYNCING_OK,
  NOT_SYNCING_FAILED_READ,
  NOT_SYNCING_DUPLICATE_TAGS,
  NOT_SYNCING_SERVER_ERROR,
  NUM_SYNC_STATES
};

// Metrics: "PasswordGeneration.SubmissionEvent"
enum PasswordSubmissionEvent {
  PASSWORD_SUBMITTED,
  PASSWORD_SUBMISSION_FAILED,
  PASSWORD_NOT_SUBMITTED,
  PASSWORD_OVERRIDDEN,
  PASSWORD_USED,
  GENERATED_PASSWORD_FORCE_SAVED,
  SUBMISSION_EVENT_ENUM_COUNT
};

enum UpdatePasswordSubmissionEvent {
  NO_ACCOUNTS_CLICKED_UPDATE,
  NO_ACCOUNTS_CLICKED_NOPE,
  NO_ACCOUNTS_NO_INTERACTION,
  ONE_ACCOUNT_CLICKED_UPDATE,
  ONE_ACCOUNT_CLICKED_NOPE,
  ONE_ACCOUNT_NO_INTERACTION,
  MULTIPLE_ACCOUNTS_CLICKED_UPDATE,
  MULTIPLE_ACCOUNTS_CLICKED_NOPE,
  MULTIPLE_ACCOUNTS_NO_INTERACTION,
  PASSWORD_OVERRIDDEN_CLICKED_UPDATE,
  PASSWORD_OVERRIDDEN_CLICKED_NOPE,
  PASSWORD_OVERRIDDEN_NO_INTERACTION,
  UPDATE_PASSWORD_EVENT_COUNT,

  NO_UPDATE_SUBMISSION
};

enum MultiAccountUpdateBubbleUserAction {
  DEFAULT_ACCOUNT_MATCHED_BY_PASSWORD_USER_CHANGED,
  DEFAULT_ACCOUNT_MATCHED_BY_PASSWORD_USER_NOT_CHANGED,
  DEFAULT_ACCOUNT_MATCHED_BY_PASSWORD_USER_REJECTED_UPDATE,
  DEFAULT_ACCOUNT_PREFERRED_USER_CHANGED,
  DEFAULT_ACCOUNT_PREFERRED_USER_NOT_CHANGED,
  DEFAULT_ACCOUNT_PREFERRED_USER_REJECTED_UPDATE,
  DEFAULT_ACCOUNT_FIRST_USER_CHANGED,
  DEFAULT_ACCOUNT_FIRST_USER_NOT_CHANGED,
  DEFAULT_ACCOUNT_FIRST_USER_REJECTED_UPDATE,
  MULTI_ACCOUNT_UPDATE_BUBBLE_USER_ACTION_COUNT
};

enum AutoSigninPromoUserAction {
  AUTO_SIGNIN_NO_ACTION,
  AUTO_SIGNIN_TURN_OFF,
  AUTO_SIGNIN_OK_GOT_IT,
  AUTO_SIGNIN_PROMO_ACTION_COUNT
};

enum AccountChooserUserAction {
  ACCOUNT_CHOOSER_DISMISSED,
  ACCOUNT_CHOOSER_CREDENTIAL_CHOSEN,
  ACCOUNT_CHOOSER_SIGN_IN,
  ACCOUNT_CHOOSER_ACTION_COUNT
};

enum SyncSignInUserAction {
  CHROME_SIGNIN_DISMISSED,
  CHROME_SIGNIN_OK,
  CHROME_SIGNIN_CANCEL,
  CHROME_SIGNIN_ACTION_COUNT
};

enum AccountChooserUsabilityMetric {
  ACCOUNT_CHOOSER_LOOKS_OK,
  ACCOUNT_CHOOSER_EMPTY_USERNAME,
  ACCOUNT_CHOOSER_DUPLICATES,
  ACCOUNT_CHOOSER_EMPTY_USERNAME_AND_DUPLICATES,
  ACCOUNT_CHOOSER_USABILITY_COUNT,
};

enum CredentialManagerGetResult {
  // The promise is rejected.
  CREDENTIAL_MANAGER_GET_REJECTED,
  // Auto sign-in is not allowed in the current context.
  CREDENTIAL_MANAGER_GET_NONE_ZERO_CLICK_OFF,
  // No matching credentials found.
  CREDENTIAL_MANAGER_GET_NONE_EMPTY_STORE,
  // User mediation required due to > 1 matching credentials.
  CREDENTIAL_MANAGER_GET_NONE_MANY_CREDENTIALS,
  // User mediation required due to the signed out state.
  CREDENTIAL_MANAGER_GET_NONE_SIGNED_OUT,
  // User mediation required due to pending first run experience dialog.
  CREDENTIAL_MANAGER_GET_NONE_FIRST_RUN,
  // Return empty credential for whatever reason.
  CREDENTIAL_MANAGER_GET_NONE,
  // Return a credential from the account chooser.
  CREDENTIAL_MANAGER_GET_ACCOUNT_CHOOSER,
  // User is auto signed in.
  CREDENTIAL_MANAGER_GET_AUTOSIGNIN,
  CREDENTIAL_MANAGER_GET_COUNT
};

// Metrics: "PasswordManager.HttpPasswordMigrationMode"
enum HttpPasswordMigrationMode {
  HTTP_PASSWORD_MIGRATION_MODE_MOVE,
  HTTP_PASSWORD_MIGRATION_MODE_COPY,
  HTTP_PASSWORD_MIGRATION_MODE_COUNT
};

enum PasswordReusePasswordFieldDetected {
  NO_PASSWORD_FIELD,
  HAS_PASSWORD_FIELD,
  PASSWORD_REUSE_PASSWORD_FIELD_DETECTED_COUNT
};

// Recorded into a UMA histogram, so order of enumerators should not be changed.
enum class SubmittedFormFrame {
  MAIN_FRAME,
  IFRAME_WITH_SAME_URL_AS_MAIN_FRAME,
  IFRAME_WITH_DIFFERENT_URL_SAME_SIGNON_REALM_AS_MAIN_FRAME,
  IFRAME_WITH_DIFFERENT_SIGNON_REALM,
  SUBMITTED_FORM_FRAME_COUNT
};

// Metrics: "PasswordManager.AccessPasswordInSettings"
enum AccessPasswordInSettingsEvent {
  ACCESS_PASSWORD_VIEWED = 0,
  ACCESS_PASSWORD_COPIED = 1,
  ACCESS_PASSWORD_COUNT
};

// Metrics: PasswordManager.ReauthToAccessPasswordInSettings
enum ReauthToAccessPasswordInSettingsEvent {
  REAUTH_SUCCESS = 0,
  REAUTH_FAILURE = 1,
  REAUTH_SKIPPED = 2,
  REAUTH_COUNT
};

// Metrics: PasswordManager.IE7LookupResult
enum IE7LookupResultStatus {
  IE7_RESULTS_ABSENT = 0,
  IE7_RESULTS_PRESENT = 1,
  IE7_RESULTS_COUNT
};

// Specifies the type of PasswordFormManagers and derived classes to distinguish
// the context in which a PasswordFormManager is being created and used.
enum class CredentialSourceType {
  kUnknown,
  // This is used for form based credential management (PasswordFormManager).
  kPasswordManager,
  // This is used for credential management API based credential management
  // (CredentialManagerPasswordFormManager).
  kCredentialManagementAPI
};

#if defined(OS_WIN) || (defined(OS_MACOSX) && !defined(OS_IOS)) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
enum class SyncPasswordHashChange {
  SAVED_ON_CHROME_SIGNIN,
  SAVED_IN_CONTENT_AREA,
  CLEARED_ON_CHROME_SIGNOUT,
  CHANGED_IN_CONTENT_AREA,
  SAVED_SYNC_PASSWORD_CHANGE_COUNT
};

enum class IsSyncPasswordHashSaved {
  NOT_SAVED,
  SAVED,
  IS_SYNC_PASSWORD_HASH_SAVED_COUNT
};
#endif

// Specifies the context in which the "Show all saved passwords" fallback is
// shown or accepted.
// Metrics:
// - PasswordManager.ShowAllSavedPasswordsAcceptedContext
// - PasswordManager.ShowAllSavedPasswordsShownContext
enum ShowAllSavedPasswordsContext {
  SHOW_ALL_SAVED_PASSWORDS_CONTEXT_NONE,
  // The "Show all saved passwords..." fallback is shown below a list of
  // available passwords.
  SHOW_ALL_SAVED_PASSWORDS_CONTEXT_PASSWORD,
  // The "Show all saved passwords..." fallback is shown when no available
  // passwords can be suggested to the user, e.g. because none are saved or
  // because of technical issues.
  SHOW_ALL_SAVED_PASSWORDS_CONTEXT_MANUAL_FALLBACK,
  SHOW_ALL_SAVED_PASSWORDS_CONTEXT_COUNT
};

// A version of the UMA_HISTOGRAM_BOOLEAN macro that allows the |name|
// to vary over the program's runtime.
void LogUMAHistogramBoolean(const std::string& name, bool sample);

// Log the |reason| a user dismissed the password manager UI.
void LogUIDismissalReason(UIDismissalReason reason);

// Log the appropriate display disposition.
void LogUIDisplayDisposition(UIDisplayDisposition disposition);

// Log if a saved FormData was deserialized correctly.
void LogFormDataDeserializationStatus(FormDeserializationStatus status);

// When a credential was filled, log whether it came from an Android app.
void LogFilledCredentialIsFromAndroidApp(bool from_android);

// Log what's preventing passwords from syncing.
void LogPasswordSyncState(PasswordSyncState state);

// Log submission events related to generation.
void LogPasswordGenerationSubmissionEvent(PasswordSubmissionEvent event);

// Log when password generation is available for a particular form.
void LogPasswordGenerationAvailableSubmissionEvent(
    PasswordSubmissionEvent event);

// Log submission events related to password update.
void LogUpdatePasswordSubmissionEvent(UpdatePasswordSubmissionEvent event);

// Log a user action on showing an update password bubble with multiple
// accounts.
void LogMultiAccountUpdateBubbleUserAction(
    MultiAccountUpdateBubbleUserAction action);

// Log a user action on showing the autosignin first run experience.
void LogAutoSigninPromoUserAction(AutoSigninPromoUserAction action);

// Log a user action on showing the account chooser for one or many accounts.
void LogAccountChooserUserActionOneAccount(AccountChooserUserAction action);
void LogAccountChooserUserActionManyAccounts(AccountChooserUserAction action);

// Log a user action on showing the Chrome sign in promo.
void LogSyncSigninPromoUserAction(SyncSignInUserAction action);

// Logs whether a password was rejected due to same origin but different scheme.
void LogShouldBlockPasswordForSameOriginButDifferentScheme(bool should_block);

// Logs number of passwords migrated from HTTP to HTTPS.
void LogCountHttpMigratedPasswords(int count);

// Logs mode of HTTP password migration.
void LogHttpPasswordMigrationMode(HttpPasswordMigrationMode mode);

// Log if the account chooser has empty username or duplicate usernames. In
// addition record number of the placeholder avatars and total number of rows.
void LogAccountChooserUsability(AccountChooserUsabilityMetric usability,
                                int count_empty_icons,
                                int count_accounts);

// Log the result of navigator.credentials.get.
void LogCredentialManagerGetResult(CredentialManagerGetResult result,
                                   CredentialMediationRequirement mediation);

// Log the password reuse.
void LogPasswordReuse(int password_length,
                      int saved_passwords,
                      int number_matches,
                      bool password_field_detected);

// Log when the user selects the "Login not secure" warning in the password
// autofill dropdown to show more information about the warning.
void LogShowedHttpNotSecureExplanation();

// Log that the Form-Not-Secure warning was shown. Should be called at most once
// per main-frame navigation.
void LogShowedFormNotSecureWarningOnCurrentNavigation();

// Log the context in which the "Show all saved passwords" fallback was shown.
void LogContextOfShowAllSavedPasswordsShown(
    ShowAllSavedPasswordsContext context);

// Log the context in which the "Show all saved passwords" fallback was
// accepted.
void LogContextOfShowAllSavedPasswordsAccepted(
    ShowAllSavedPasswordsContext context);

// Log a password successful submission event.
void LogPasswordSuccessfulSubmissionIndicatorEvent(
    autofill::PasswordForm::SubmissionIndicatorEvent event);

// Log a password successful submission event for accepted by user password save
// or update.
void LogPasswordAcceptedSaveUpdateSubmissionIndicatorEvent(
    autofill::PasswordForm::SubmissionIndicatorEvent event);

// Log a frame of a submitted password form.
void LogSubmittedFormFrame(SubmittedFormFrame frame);

#if defined(OS_WIN) || (defined(OS_MACOSX) && !defined(OS_IOS)) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
// Log a save sync password change event.
void LogSyncPasswordHashChange(SyncPasswordHashChange event);

// Log whether a sync password hash saved.
void LogIsSyncPasswordHashSaved(IsSyncPasswordHashSaved state);
#endif

}  // namespace metrics_util

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_METRICS_UTIL_H_

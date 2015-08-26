// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_METRICS_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_METRICS_UTIL_H_

#include <string>

class PrefService;

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
  MANUAL_BLACKLISTED,  // obsolete.
  AUTOMATIC_GENERATED_PASSWORD_CONFIRMATION,
  AUTOMATIC_CREDENTIAL_REQUEST,
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
  CLICKED_UNBLACKLIST,
  CLICKED_OK,
  CLICKED_CREDENTIAL,
  AUTO_SIGNIN_TOAST_TIMEOUT,
  AUTO_SIGNIN_TOAST_CLICKED,
  CLICKED_BRAND_NAME,
  NUM_UI_RESPONSES,

  // If we add the omnibox icon _without_ intending to display the bubble,
  // we actually call Close() after creating the bubble view. We don't want
  // that to count in the metrics, so we need this placeholder value.
  NOT_DISPLAYED
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

// We monitor the performance of the save password heuristic for a handful of
// domains. For privacy reasons we are not reporting UMA signals by domain, but
// by a domain group. A domain group can contain multiple domains, and a domain
// can be contained in multiple groups.
// For more information see http://goo.gl/vUuFd5.

// The number of groups in which each monitored website appears.
// It is a half of the total number of groups.
const size_t kGroupsPerDomain = 10u;

// Check whether the |url_host| is monitored or not. If yes, we return
// the id of the group which contains the domain name otherwise
// returns 0. |pref_service| needs to be the profile preference service.
size_t MonitoredDomainGroupId(const std::string& url_host,
                              PrefService* pref_service);

// A version of the UMA_HISTOGRAM_ENUMERATION macro that allows the |name|
// to vary over the program's runtime.
void LogUMAHistogramEnumeration(const std::string& name,
                                int sample,
                                int boundary_value);

// A version of the UMA_HISTOGRAM_BOOLEAN macro that allows the |name|
// to vary over the program's runtime.
void LogUMAHistogramBoolean(const std::string& name, bool sample);

// Returns a string which contains group_|group_id|. If the
// |group_id| corresponds to an unmonitored domain returns an empty string.
std::string GroupIdToString(size_t group_id);

// Log the |reason| a user dismissed the password manager UI.
void LogUIDismissalReason(UIDismissalReason reason);

// Given a ResponseType, log the appropriate UIResponse. We'll use this
// mapping to migrate from "PasswordManager.InfoBarResponse" to
// "PasswordManager.UIDismissalReason" so we can accurately evaluate the
// impact of the bubble UI.
//
// TODO(mkwst): Drop this (and the infobar metric itself) once the new metric
// has rolled out to stable.
void LogUIDismissalReason(ResponseType type);

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

}  // namespace metrics_util

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_METRICS_UTIL_H_

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
  MANUAL_BLACKLISTED,
  AUTOMATIC_GENERATED_PASSWORD_CONFIRMATION,
  NUM_DISPLAY_DISPOSITIONS
};

// Metrics: "PasswordManager.UIDismissalReason"
enum UIDismissalReason {
  // We use this to mean both "Bubble lost focus" and "No interaction with the
  // infobar", depending on which experiment is active.
  NO_DIRECT_INTERACTION = 0,
  CLICKED_SAVE,
  CLICKED_NOPE,
  CLICKED_NEVER,
  CLICKED_MANAGE,
  CLICKED_DONE,
  CLICKED_UNBLACKLIST,
  CLICKED_OK,
  NUM_UI_RESPONSES,

  // If we add the omnibox icon _without_ intending to display the bubble,
  // we actually call Close() after creating the bubble view. We don't want
  // that to count in the metrics, so we need this placeholder value.
  NOT_DISPLAYED
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

}  // namespace metrics_util

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_METRICS_UTIL_H_

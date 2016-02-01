// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_metrics_util.h"

#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "url/gurl.h"

using base::ListValue;
using base::FundamentalValue;

namespace password_manager {

namespace metrics_util {

namespace {

// The number of domain groups.
const size_t kNumGroups = 2u * kGroupsPerDomain;

// |kDomainMapping| contains each monitored website together with all ids of
// groups which contain the website. Each website appears in
// |kGroupsPerDomain| groups, and each group includes an equal number of
// websites, so that no two websites have the same set of groups that they
// belong to. All ids are in the range [1, |kNumGroups|].
// For more information about the algorithm used see http://goo.gl/vUuFd5.
struct DomainGroupsPair {
  const char* const domain_name;
  const size_t group_ids[kGroupsPerDomain];
};
const DomainGroupsPair kDomainMapping[] = {
    {"google.com", {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}},
    {"yahoo.com", {1, 2, 3, 4, 5, 11, 12, 13, 14, 15}},
    {"baidu.com", {1, 2, 3, 4, 6, 7, 11, 12, 16, 17}},
    {"wikipedia.org", {1, 2, 3, 4, 5, 6, 11, 12, 16, 18}},
    {"linkedin.com", {1, 6, 8, 11, 13, 14, 15, 16, 17, 19}},
    {"twitter.com", {5, 6, 7, 8, 9, 11, 13, 17, 19, 20}},
    {"facebook.com", {7, 8, 9, 10, 13, 14, 16, 17, 18, 20}},
    {"amazon.com", {2, 5, 9, 10, 12, 14, 15, 18, 19, 20}},
    {"ebay.com", {3, 7, 9, 10, 14, 15, 17, 18, 19, 20}},
    {"tumblr.com", {4, 8, 10, 12, 13, 15, 16, 18, 19, 20}},
};
const size_t kNumDomains = arraysize(kDomainMapping);

// For every monitored domain, this function chooses which of the groups
// containing that domain should be used for reporting. That number is chosen
// randomly and stored in the user's preferences.
size_t GetGroupIndex(size_t domain_index, PrefService* pref_service) {
  DCHECK_LT(domain_index, kNumDomains);

  const base::ListValue* group_indices =
      pref_service->GetList(prefs::kPasswordManagerGroupsForDomains);
  int result = 0;
  if (!group_indices->GetInteger(domain_index, &result)) {
    ListPrefUpdate group_indices_updater(
        pref_service, prefs::kPasswordManagerGroupsForDomains);
    // This value has not been generated yet.
    result = base::checked_cast<int>(base::RandGenerator(kGroupsPerDomain));
    group_indices_updater->Set(domain_index, new FundamentalValue(result));
  }
  return base::checked_cast<size_t>(result);
}

}  // namespace

size_t MonitoredDomainGroupId(const std::string& url_host,
                              PrefService* pref_service) {
  GURL url(url_host);
  for (size_t i = 0; i < kNumDomains; ++i) {
    if (url.DomainIs(kDomainMapping[i].domain_name))
      return kDomainMapping[i].group_ids[GetGroupIndex(i, pref_service)];
  }
  return 0;
}

void LogUMAHistogramEnumeration(const std::string& name,
                                int sample,
                                int boundary_value) {
  DCHECK_LT(sample, boundary_value);

  // Note: This leaks memory, which is expected behavior.
  base::HistogramBase* histogram =
      base::LinearHistogram::FactoryGet(
          name,
          1,
          boundary_value,
          boundary_value + 1,
          base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(sample);
}

void LogUMAHistogramBoolean(const std::string& name, bool sample) {
  // Note: This leaks memory, which is expected behavior.
  base::HistogramBase* histogram = base::BooleanHistogram::FactoryGet(
      name, base::Histogram::kUmaTargetedHistogramFlag);
  histogram->AddBoolean(sample);
}

std::string GroupIdToString(size_t group_id) {
  DCHECK_LE(group_id, kNumGroups);
  if (group_id > 0)
    return "group_" + base::SizeTToString(group_id);
  return std::string();
}

void LogUIDismissalReason(ResponseType type) {
  UIDismissalReason reason = NO_DIRECT_INTERACTION;
  switch (type) {
    case NO_RESPONSE:
      reason = NO_DIRECT_INTERACTION;
      break;
    case REMEMBER_PASSWORD:
      reason = CLICKED_SAVE;
      break;
    case NEVER_REMEMBER_PASSWORD:
      reason = CLICKED_NEVER;
      break;
    case INFOBAR_DISMISSED:
      reason = CLICKED_CANCEL;
      break;
    case NUM_RESPONSE_TYPES:
      NOTREACHED();
      break;
  }
  LogUIDismissalReason(reason);
}

void LogUIDismissalReason(UIDismissalReason reason) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.UIDismissalReason",
                            reason,
                            NUM_UI_RESPONSES);
}

void LogUIDisplayDisposition(UIDisplayDisposition disposition) {
  UMA_HISTOGRAM_ENUMERATION("PasswordBubble.DisplayDisposition",
                            disposition,
                            NUM_DISPLAY_DISPOSITIONS);
}

void LogFormDataDeserializationStatus(FormDeserializationStatus status) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.FormDataDeserializationStatus",
                            status, NUM_DESERIALIZATION_STATUSES);
}

void LogFilledCredentialIsFromAndroidApp(bool from_android) {
  UMA_HISTOGRAM_BOOLEAN(
      "PasswordManager.FilledCredentialWasFromAndroidApp",
      from_android);
}

void LogPasswordSyncState(PasswordSyncState state) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.PasswordSyncState", state,
                            NUM_SYNC_STATES);
}

void LogPasswordGenerationSubmissionEvent(PasswordSubmissionEvent event) {
  UMA_HISTOGRAM_ENUMERATION("PasswordGeneration.SubmissionEvent", event,
                            SUBMISSION_EVENT_ENUM_COUNT);
}

void LogPasswordGenerationAvailableSubmissionEvent(
    PasswordSubmissionEvent event) {
  UMA_HISTOGRAM_ENUMERATION("PasswordGeneration.SubmissionAvailableEvent",
                            event, SUBMISSION_EVENT_ENUM_COUNT);
}

void LogUpdatePasswordSubmissionEvent(UpdatePasswordSubmissionEvent event) {
  DCHECK_LT(event, UPDATE_PASSWORD_EVENT_COUNT);
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.UpdatePasswordSubmissionEvent",
                            event, UPDATE_PASSWORD_EVENT_COUNT);
}

void LogMultiAccountUpdateBubbleUserAction(
    MultiAccountUpdateBubbleUserAction action) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.MultiAccountPasswordUpdateAction",
                            action,
                            MULTI_ACCOUNT_UPDATE_BUBBLE_USER_ACTION_COUNT);
}

}  // namespace metrics_util

}  // namespace password_manager

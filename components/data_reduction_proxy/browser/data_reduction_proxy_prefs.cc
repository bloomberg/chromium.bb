// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/browser/data_reduction_proxy_prefs.h"

#include "base/prefs/pref_registry_simple.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace data_reduction_proxy {

// Make sure any changes here that have the potential to impact android_webview
// are reflected in RegisterSimpleProfilePrefs.
void RegisterSyncableProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      data_reduction_proxy::prefs::kDataReductionProxyEnabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      data_reduction_proxy::prefs::kDataReductionProxyAltEnabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      data_reduction_proxy::prefs::kDataReductionProxyWasEnabledBefore,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

void RegisterSimpleProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      data_reduction_proxy::prefs::kDataReductionProxyEnabled, false);
  registry->RegisterBooleanPref(
      data_reduction_proxy::prefs::kDataReductionProxyAltEnabled, false);
  registry->RegisterBooleanPref(
      data_reduction_proxy::prefs::kDataReductionProxyWasEnabledBefore, false);
}

// Add any new data reduction proxy prefs to the |pref_map_| or the
// |list_pref_map_| in Init() of DataReductionProxyStatisticsPrefs.
void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(
      prefs::kHttpReceivedContentLength, 0);
  registry->RegisterInt64Pref(
      prefs::kHttpOriginalContentLength, 0);
  registry->RegisterListPref(
      prefs::kDailyHttpOriginalContentLength);
  registry->RegisterListPref(
      prefs::kDailyHttpReceivedContentLength);
  registry->RegisterListPref(
      prefs::kDailyOriginalContentLengthWithDataReductionProxyEnabled);
  registry->RegisterListPref(
      prefs::kDailyContentLengthWithDataReductionProxyEnabled);
  registry->RegisterListPref(
      prefs::kDailyContentLengthHttpsWithDataReductionProxyEnabled);
  registry->RegisterListPref(
      prefs::kDailyContentLengthShortBypassWithDataReductionProxyEnabled);
  registry->RegisterListPref(
      prefs::kDailyContentLengthLongBypassWithDataReductionProxyEnabled);
  registry->RegisterListPref(
      prefs::kDailyContentLengthUnknownWithDataReductionProxyEnabled);
  registry->RegisterListPref(
      prefs::kDailyOriginalContentLengthViaDataReductionProxy);
  registry->RegisterListPref(
      prefs::kDailyContentLengthViaDataReductionProxy);
  registry->RegisterInt64Pref(
      prefs::kDailyHttpContentLengthLastUpdateDate, 0L);
}

}  // namespace data_reduction_proxy

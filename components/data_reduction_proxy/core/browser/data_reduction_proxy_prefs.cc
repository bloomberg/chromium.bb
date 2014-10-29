// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h"

#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace {

void TransferPrefList(const char* pref_path,
                      PrefService* src,
                      PrefService* dest) {
  DCHECK(src->FindPreference(pref_path)->GetType() == base::Value::TYPE_LIST);
  ListPrefUpdate update_dest(dest, pref_path);
  scoped_ptr<base::ListValue> src_list(src->GetList(pref_path)->DeepCopy());
  update_dest->Swap(src_list.get());
  ListPrefUpdate update_src(src, pref_path);
  src->ClearPref(pref_path);
}

}  // namespace

namespace data_reduction_proxy {

// Make sure any changes here that have the potential to impact android_webview
// are reflected in RegisterSimpleProfilePrefs.
void RegisterSyncableProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kDataReductionProxyEnabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kDataReductionProxyAltEnabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kDataReductionProxyWasEnabledBefore,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  registry->RegisterInt64Pref(
      prefs::kHttpReceivedContentLength,
      0,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterInt64Pref(
      prefs::kHttpOriginalContentLength,
      0,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  registry->RegisterBooleanPref(
      prefs::kStatisticsPrefsMigrated,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kUpdateDailyReceivedContentLengths,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(
      prefs::kDailyHttpOriginalContentLength,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(
      prefs::kDailyHttpReceivedContentLength,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(
      prefs::kDailyOriginalContentLengthWithDataReductionProxyEnabled,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(
      prefs::kDailyContentLengthWithDataReductionProxyEnabled,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(
      prefs::kDailyContentLengthHttpsWithDataReductionProxyEnabled,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(
      prefs::kDailyContentLengthShortBypassWithDataReductionProxyEnabled,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(
      prefs::kDailyContentLengthLongBypassWithDataReductionProxyEnabled,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(
      prefs::kDailyContentLengthUnknownWithDataReductionProxyEnabled,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(
      prefs::kDailyOriginalContentLengthViaDataReductionProxy,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(
      prefs::kDailyContentLengthViaDataReductionProxy,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterInt64Pref(
      prefs::kDailyHttpContentLengthLastUpdateDate,
      0L,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

void RegisterSimpleProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      prefs::kDataReductionProxyEnabled, false);
  registry->RegisterBooleanPref(
      prefs::kDataReductionProxyAltEnabled, false);
  registry->RegisterBooleanPref(
      prefs::kDataReductionProxyWasEnabledBefore, false);

  registry->RegisterBooleanPref(
      prefs::kStatisticsPrefsMigrated, false);
  RegisterPrefs(registry);
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
  registry->RegisterBooleanPref(prefs::kUpdateDailyReceivedContentLengths,
                                false);
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

void MigrateStatisticsPrefs(PrefService* local_state_prefs,
                            PrefService* profile_prefs) {
  if (profile_prefs->GetBoolean(prefs::kStatisticsPrefsMigrated))
    return;
  if (local_state_prefs == profile_prefs) {
    profile_prefs->SetBoolean(prefs::kStatisticsPrefsMigrated, true);
    return;
  }
  profile_prefs->SetInt64(
      prefs::kHttpReceivedContentLength,
      local_state_prefs->GetInt64(prefs::kHttpReceivedContentLength));
  local_state_prefs->ClearPref(prefs::kHttpReceivedContentLength);
  profile_prefs->SetInt64(
      prefs::kHttpOriginalContentLength,
      local_state_prefs->GetInt64(prefs::kHttpOriginalContentLength));
  local_state_prefs->ClearPref(prefs::kHttpOriginalContentLength);
  TransferPrefList(
      prefs::kDailyHttpOriginalContentLength, local_state_prefs, profile_prefs);
  TransferPrefList(
      prefs::kDailyHttpReceivedContentLength, local_state_prefs, profile_prefs);
  TransferPrefList(
      prefs::kDailyOriginalContentLengthWithDataReductionProxyEnabled,
      local_state_prefs,
      profile_prefs);
  TransferPrefList(
      prefs::kDailyContentLengthWithDataReductionProxyEnabled,
      local_state_prefs,
      profile_prefs);
  TransferPrefList(
      prefs::kDailyContentLengthHttpsWithDataReductionProxyEnabled,
      local_state_prefs,
      profile_prefs);
  TransferPrefList(
      prefs::kDailyContentLengthShortBypassWithDataReductionProxyEnabled,
      local_state_prefs,
      profile_prefs);
  TransferPrefList(
      prefs::kDailyContentLengthLongBypassWithDataReductionProxyEnabled,
      local_state_prefs,
      profile_prefs);
  TransferPrefList(
      prefs::kDailyContentLengthUnknownWithDataReductionProxyEnabled,
      local_state_prefs,
      profile_prefs);
  TransferPrefList(
      prefs::kDailyOriginalContentLengthViaDataReductionProxy,
      local_state_prefs,
      profile_prefs);
  TransferPrefList(
      prefs::kDailyContentLengthViaDataReductionProxy,
      local_state_prefs,
      profile_prefs);
  profile_prefs->SetInt64(
      prefs::kDailyHttpContentLengthLastUpdateDate,
      local_state_prefs->GetInt64(
          prefs::kDailyHttpContentLengthLastUpdateDate));
  local_state_prefs->ClearPref(prefs::kDailyHttpContentLengthLastUpdateDate);
  profile_prefs->SetBoolean(prefs::kStatisticsPrefsMigrated, true);
}

}  // namespace data_reduction_proxy

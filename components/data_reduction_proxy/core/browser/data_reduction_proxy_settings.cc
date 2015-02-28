// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_member.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_statistics_prefs.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"

namespace {
// Key of the UMA DataReductionProxy.StartupState histogram.
const char kUMAProxyStartupStateHistogram[] =
    "DataReductionProxy.StartupState";

int64 GetInt64PrefValue(const base::ListValue& list_value, size_t index) {
  int64 val = 0;
  std::string pref_value;
  bool rv = list_value.GetString(index, &pref_value);
  DCHECK(rv);
  if (rv) {
    rv = base::StringToInt64(pref_value, &val);
    DCHECK(rv);
  }
  return val;
}

bool IsEnabledOnCommandLine() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  return command_line.HasSwitch(
      data_reduction_proxy::switches::kEnableDataReductionProxy);
}

}  // namespace

namespace data_reduction_proxy {

const char kDataReductionPassThroughHeader[] =
    "X-PSA-Client-Options: v=1,m=1\nCache-Control: no-cache";

DataReductionProxySettings::DataReductionProxySettings()
    : unreachable_(false),
      allowed_(false),
      alternative_allowed_(false),
      promo_allowed_(false),
      prefs_(NULL),
      event_store_(NULL),
      config_(nullptr) {
}

DataReductionProxySettings::~DataReductionProxySettings() {
  if (allowed_)
    spdy_proxy_auth_enabled_.Destroy();
}

void DataReductionProxySettings::InitPrefMembers() {
  DCHECK(thread_checker_.CalledOnValidThread());
  spdy_proxy_auth_enabled_.Init(
      prefs::kDataReductionProxyEnabled,
      GetOriginalProfilePrefs(),
      base::Bind(&DataReductionProxySettings::OnProxyEnabledPrefChange,
                 base::Unretained(this)));
  data_reduction_proxy_alternative_enabled_.Init(
      prefs::kDataReductionProxyAltEnabled,
      GetOriginalProfilePrefs(),
      base::Bind(
          &DataReductionProxySettings::OnProxyAlternativeEnabledPrefChange,
          base::Unretained(this)));
}

void DataReductionProxySettings::UpdateConfigValues() {
  DCHECK(config_);
  allowed_ = config_->allowed();
  alternative_allowed_ = config_->alternative_allowed();
  promo_allowed_ = config_->promo_allowed();
  primary_origin_ = config_->Origin().ToURI();
}

void DataReductionProxySettings::InitDataReductionProxySettings(
    PrefService* prefs,
    DataReductionProxyIOData* io_data,
    scoped_ptr<DataReductionProxyService> data_reduction_proxy_service) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(prefs);
  DCHECK(io_data);
  DCHECK(io_data->config());
  DCHECK(io_data->event_store());
  DCHECK(data_reduction_proxy_service.get());
  prefs_ = prefs;
  config_ = io_data->config();
  event_store_ = io_data->event_store();
  data_reduction_proxy_service_ = data_reduction_proxy_service.Pass();
  InitPrefMembers();
  UpdateConfigValues();
  RecordDataReductionInit();
}

void DataReductionProxySettings::SetOnDataReductionEnabledCallback(
    const base::Callback<void(bool)>& on_data_reduction_proxy_enabled) {
  on_data_reduction_proxy_enabled_ = on_data_reduction_proxy_enabled;
  on_data_reduction_proxy_enabled_.Run(IsDataReductionProxyEnabled());
}

bool DataReductionProxySettings::IsDataReductionProxyEnabled() const {
  return spdy_proxy_auth_enabled_.GetValue() || IsEnabledOnCommandLine();
}

bool DataReductionProxySettings::CanUseDataReductionProxy(
    const GURL& url) const {
  return url.is_valid() && url.scheme() == url::kHttpScheme &&
      IsDataReductionProxyEnabled();
}

bool
DataReductionProxySettings::IsDataReductionProxyAlternativeEnabled() const {
  return data_reduction_proxy_alternative_enabled_.GetValue();
}

bool DataReductionProxySettings::IsDataReductionProxyManaged() {
  return spdy_proxy_auth_enabled_.IsManaged();
}

void DataReductionProxySettings::SetDataReductionProxyEnabled(bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Prevent configuring the proxy when it is not allowed to be used.
  if (!allowed_)
    return;

  if (spdy_proxy_auth_enabled_.GetValue() != enabled) {
    spdy_proxy_auth_enabled_.SetValue(enabled);
    OnProxyEnabledPrefChange();
  }
}

void DataReductionProxySettings::SetDataReductionProxyAlternativeEnabled(
    bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Prevent configuring the proxy when it is not allowed to be used.
  if (!alternative_allowed_)
    return;
  if (data_reduction_proxy_alternative_enabled_.GetValue() != enabled) {
    data_reduction_proxy_alternative_enabled_.SetValue(enabled);
    OnProxyAlternativeEnabledPrefChange();
  }
}

int64 DataReductionProxySettings::GetDataReductionLastUpdateTime() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(data_reduction_proxy_service_->statistics_prefs());
  int64 last_update_internal =
      data_reduction_proxy_service_->statistics_prefs()->GetInt64(
          prefs::kDailyHttpContentLengthLastUpdateDate);
  base::Time last_update = base::Time::FromInternalValue(last_update_internal);
  return static_cast<int64>(last_update.ToJsTime());
}

DataReductionProxySettings::ContentLengthList
DataReductionProxySettings::GetDailyOriginalContentLengths() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return GetDailyContentLengths(prefs::kDailyHttpOriginalContentLength);
}

void DataReductionProxySettings::SetUnreachable(bool unreachable) {
  unreachable_ = unreachable;
}

bool DataReductionProxySettings::IsDataReductionProxyUnreachable() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return unreachable_;
}

DataReductionProxySettings::ContentLengthList
DataReductionProxySettings::GetDailyReceivedContentLengths() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return GetDailyContentLengths(prefs::kDailyHttpReceivedContentLength);
}

PrefService* DataReductionProxySettings::GetOriginalProfilePrefs() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return prefs_;
}

void DataReductionProxySettings::OnProxyEnabledPrefChange() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!on_data_reduction_proxy_enabled_.is_null())
    on_data_reduction_proxy_enabled_.Run(IsDataReductionProxyEnabled());
  if (!allowed_)
    return;
  MaybeActivateDataReductionProxy(false);
}

void DataReductionProxySettings::OnProxyAlternativeEnabledPrefChange() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!alternative_allowed_)
    return;
  MaybeActivateDataReductionProxy(false);
}

void DataReductionProxySettings::ResetDataReductionStatistics() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(data_reduction_proxy_service_->statistics_prefs());
  base::ListValue* original_update =
      data_reduction_proxy_service_->statistics_prefs()->GetList(
          prefs::kDailyHttpOriginalContentLength);
  base::ListValue* received_update =
      data_reduction_proxy_service_->statistics_prefs()->GetList(
          prefs::kDailyHttpReceivedContentLength);
  original_update->Clear();
  received_update->Clear();
  for (size_t i = 0; i < kNumDaysInHistory; ++i) {
    original_update->AppendString(base::Int64ToString(0));
    received_update->AppendString(base::Int64ToString(0));
  }
}

void DataReductionProxySettings::MaybeActivateDataReductionProxy(
    bool at_startup) {
  DCHECK(thread_checker_.CalledOnValidThread());
  PrefService* prefs = GetOriginalProfilePrefs();
  // Do nothing if prefs have not been initialized. This allows unit testing
  // of profile related code without having to initialize data reduction proxy
  // related prefs.
  if (!prefs)
    return;
  // TODO(marq): Consider moving this so stats are wiped the first time the
  // proxy settings are actually (not maybe) turned on.
  if (spdy_proxy_auth_enabled_.GetValue() &&
      !prefs->GetBoolean(prefs::kDataReductionProxyWasEnabledBefore)) {
    prefs->SetBoolean(prefs::kDataReductionProxyWasEnabledBefore, true);
    ResetDataReductionStatistics();
  }
  // Configure use of the data reduction proxy if it is enabled.
  config_->SetProxyPrefs(IsDataReductionProxyEnabled(),
                         IsDataReductionProxyAlternativeEnabled(), at_startup);
}

// Metrics methods
void DataReductionProxySettings::RecordDataReductionInit() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ProxyStartupState state = PROXY_NOT_AVAILABLE;
  if (allowed_) {
    if (IsDataReductionProxyEnabled())
      state = PROXY_ENABLED;
    else
      state = PROXY_DISABLED;
  }

  RecordStartupState(state);
}

void DataReductionProxySettings::RecordStartupState(ProxyStartupState state) {
  UMA_HISTOGRAM_ENUMERATION(kUMAProxyStartupStateHistogram,
                            state,
                            PROXY_STARTUP_STATE_COUNT);
}

DataReductionProxySettings::ContentLengthList
DataReductionProxySettings::GetDailyContentLengths(const char* pref_name) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DataReductionProxySettings::ContentLengthList content_lengths;
  DCHECK(data_reduction_proxy_service_->statistics_prefs());
  const base::ListValue* list_value =
      data_reduction_proxy_service_->statistics_prefs()->GetList(pref_name);
  if (list_value->GetSize() == kNumDaysInHistory) {
    for (size_t i = 0; i < kNumDaysInHistory; ++i) {
      content_lengths.push_back(GetInt64PrefValue(*list_value, i));
    }
  }
  return content_lengths;
}

void DataReductionProxySettings::GetContentLengths(
    unsigned int days,
    int64* original_content_length,
    int64* received_content_length,
    int64* last_update_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_LE(days, kNumDaysInHistory);
  DCHECK(data_reduction_proxy_service_->statistics_prefs());

  const base::ListValue* original_list =
      data_reduction_proxy_service_->statistics_prefs()->GetList(
          prefs::kDailyHttpOriginalContentLength);
  const base::ListValue* received_list =
      data_reduction_proxy_service_->statistics_prefs()->GetList(
          prefs::kDailyHttpReceivedContentLength);

  if (original_list->GetSize() != kNumDaysInHistory ||
      received_list->GetSize() != kNumDaysInHistory) {
    *original_content_length = 0L;
    *received_content_length = 0L;
    *last_update_time = 0L;
    return;
  }

  int64 orig = 0L;
  int64 recv = 0L;
  // Include days from the end of the list going backwards.
  for (size_t i = kNumDaysInHistory - days;
       i < kNumDaysInHistory; ++i) {
    orig += GetInt64PrefValue(*original_list, i);
    recv += GetInt64PrefValue(*received_list, i);
  }
  *original_content_length = orig;
  *received_content_length = recv;
  *last_update_time =
      data_reduction_proxy_service_->statistics_prefs()->GetInt64(
          prefs::kDailyHttpContentLengthLastUpdateDate);
}

}  // namespace data_reduction_proxy

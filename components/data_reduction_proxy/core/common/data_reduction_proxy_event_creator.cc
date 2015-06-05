// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_creator.h"

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/proxy/proxy_server.h"

namespace data_reduction_proxy {

namespace {

scoped_ptr<base::Value> BuildDataReductionProxyEvent(
    net::NetLog::EventType type,
    const net::NetLog::Source& source,
    net::NetLog::EventPhase phase,
    const net::NetLog::ParametersCallback& parameters_callback) {
  base::TimeTicks ticks_now = base::TimeTicks::Now();
  net::NetLog::EntryData entry_data(type, source, phase, ticks_now,
                                    &parameters_callback);
  net::NetLog::Entry entry(&entry_data,
                           net::NetLogCaptureMode::IncludeSocketBytes());
  scoped_ptr<base::Value> entry_value(entry.ToValue());

  return entry_value;
}

int64 GetExpirationTicks(int bypass_seconds) {
  base::TimeTicks expiration_ticks =
      base::TimeTicks::Now() + base::TimeDelta::FromSeconds(bypass_seconds);
  return (expiration_ticks - base::TimeTicks()).InMilliseconds();
}

// A callback which creates a base::Value containing information about enabling
// the Data Reduction Proxy.
scoped_ptr<base::Value> EnableDataReductionProxyCallback(
    bool secure_transport_restricted,
    const std::vector<net::ProxyServer>& proxies_for_http,
    const std::vector<net::ProxyServer>& proxies_for_https,
    net::NetLogCaptureMode /* capture_mode */) {
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetBoolean("enabled", true);
  dict->SetBoolean("secure_transport_restricted", secure_transport_restricted);
  scoped_ptr<base::ListValue> http_proxy_list(new base::ListValue());
  for (const auto& proxy : proxies_for_http)
    http_proxy_list->AppendString(proxy.ToURI());

  scoped_ptr<base::ListValue> https_proxy_list(new base::ListValue());
  for (const auto& proxy : proxies_for_https)
    https_proxy_list->AppendString(proxy.ToURI());

  dict->Set("http_proxy_list", http_proxy_list.Pass());
  dict->Set("https_proxy_list", https_proxy_list.Pass());

  return dict.Pass();
}

// A callback which creates a base::Value containing information about disabling
// the Data Reduction Proxy.
scoped_ptr<base::Value> DisableDataReductionProxyCallback(
    net::NetLogCaptureMode /* capture_mode */) {
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetBoolean("enabled", false);
  return dict.Pass();
}

// A callback which creates a base::Value containing information about bypassing
// the Data Reduction Proxy.
scoped_ptr<base::Value> UrlBypassActionCallback(
    DataReductionProxyBypassAction action,
    const std::string& request_method,
    const GURL& url,
    bool should_retry,
    int bypass_seconds,
    int64 expiration_ticks,
    net::NetLogCaptureMode /* capture_mode */) {
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetInteger("bypass_action_type", action);
  dict->SetString("method", request_method);
  dict->SetString("url", url.spec());
  dict->SetBoolean("should_retry", should_retry);
  dict->SetString("bypass_duration_seconds",
                  base::Int64ToString(bypass_seconds));
  dict->SetString("expiration", base::Int64ToString(expiration_ticks));
  return dict.Pass();
}

// A callback which creates a base::Value containing information about bypassing
// the Data Reduction Proxy.
scoped_ptr<base::Value> UrlBypassTypeCallback(
    DataReductionProxyBypassType bypass_type,
    const std::string& request_method,
    const GURL& url,
    bool should_retry,
    int bypass_seconds,
    int64 expiration_ticks,
    net::NetLogCaptureMode /* capture_mode */) {
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetInteger("bypass_type", bypass_type);
  dict->SetString("method", request_method);
  dict->SetString("url", url.spec());
  dict->SetBoolean("should_retry", should_retry);
  dict->SetString("bypass_duration_seconds",
                  base::Int64ToString(bypass_seconds));
  dict->SetString("expiration", base::Int64ToString(expiration_ticks));
  return dict.Pass();
}

// A callback that creates a base::Value containing information about a proxy
// fallback event for a Data Reduction Proxy.
scoped_ptr<base::Value> FallbackCallback(
    const std::string& proxy_url,
    int net_error,
    net::NetLogCaptureMode /* capture_mode */) {
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString("proxy", proxy_url);
  dict->SetInteger("net_error", net_error);
  return dict.Pass();
}

// A callback which creates a base::Value containing information about
// completing the Data Reduction Proxy secure proxy check.
scoped_ptr<base::Value> EndCanaryRequestCallback(
    int net_error,
    int http_response_code,
    bool succeeded,
    net::NetLogCaptureMode /* capture_mode */) {
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetInteger("net_error", net_error);
  dict->SetInteger("http_response_code", http_response_code);
  dict->SetBoolean("check_succeeded", succeeded);
  return dict.Pass();
}

// A callback that creates a base::Value containing information about
// completing the Data Reduction Proxy configuration request.
scoped_ptr<base::Value> EndConfigRequestCallback(
    int net_error,
    int http_response_code,
    int failure_count,
    int64 expiration_ticks,
    net::NetLogCaptureMode /* capture_mode */) {
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetInteger("net_error", net_error);
  dict->SetInteger("http_response_code", http_response_code);
  dict->SetInteger("failure_count", failure_count);
  dict->SetString("expiration", base::Int64ToString(expiration_ticks));
  return dict.Pass();
}

}  // namespace

DataReductionProxyEventCreator::DataReductionProxyEventCreator(
    DataReductionProxyEventStorageDelegate* storage_delegate)
    : storage_delegate_(storage_delegate) {
  DCHECK(storage_delegate);
  // Constructed on the UI thread, but should be checked on the IO thread.
  thread_checker_.DetachFromThread();
}

DataReductionProxyEventCreator::~DataReductionProxyEventCreator() {
}

void DataReductionProxyEventCreator::AddProxyEnabledEvent(
    net::NetLog* net_log,
    bool secure_transport_restricted,
    const std::vector<net::ProxyServer>& proxies_for_http,
    const std::vector<net::ProxyServer>& proxies_for_https) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const net::NetLog::ParametersCallback& parameters_callback =
      base::Bind(&EnableDataReductionProxyCallback, secure_transport_restricted,
                 proxies_for_http, proxies_for_https);
  PostEnabledEvent(net_log, net::NetLog::TYPE_DATA_REDUCTION_PROXY_ENABLED,
                   true, parameters_callback);
}

void DataReductionProxyEventCreator::AddProxyDisabledEvent(
    net::NetLog* net_log) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const net::NetLog::ParametersCallback& parameters_callback =
      base::Bind(&DisableDataReductionProxyCallback);
  PostEnabledEvent(net_log, net::NetLog::TYPE_DATA_REDUCTION_PROXY_ENABLED,
                   false, parameters_callback);
}

void DataReductionProxyEventCreator::AddBypassActionEvent(
    const net::BoundNetLog& net_log,
    DataReductionProxyBypassAction bypass_action,
    const std::string& request_method,
    const GURL& url,
    bool should_retry,
    const base::TimeDelta& bypass_duration) {
  DCHECK(thread_checker_.CalledOnValidThread());
  int64 expiration_ticks = GetExpirationTicks(bypass_duration.InSeconds());
  const net::NetLog::ParametersCallback& parameters_callback =
      base::Bind(&UrlBypassActionCallback, bypass_action, request_method, url,
                 should_retry, bypass_duration.InSeconds(), expiration_ticks);
  PostBoundNetLogBypassEvent(
      net_log, net::NetLog::TYPE_DATA_REDUCTION_PROXY_BYPASS_REQUESTED,
      net::NetLog::PHASE_NONE, expiration_ticks, parameters_callback);
}

void DataReductionProxyEventCreator::AddBypassTypeEvent(
    const net::BoundNetLog& net_log,
    DataReductionProxyBypassType bypass_type,
    const std::string& request_method,
    const GURL& url,
    bool should_retry,
    const base::TimeDelta& bypass_duration) {
  DCHECK(thread_checker_.CalledOnValidThread());
  int64 expiration_ticks = GetExpirationTicks(bypass_duration.InSeconds());
  const net::NetLog::ParametersCallback& parameters_callback =
      base::Bind(&UrlBypassTypeCallback, bypass_type, request_method, url,
                 should_retry, bypass_duration.InSeconds(), expiration_ticks);
  PostBoundNetLogBypassEvent(
      net_log, net::NetLog::TYPE_DATA_REDUCTION_PROXY_BYPASS_REQUESTED,
      net::NetLog::PHASE_NONE, expiration_ticks, parameters_callback);
}

void DataReductionProxyEventCreator::AddProxyFallbackEvent(
    net::NetLog* net_log,
    const std::string& proxy_url,
    int net_error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const net::NetLog::ParametersCallback& parameters_callback =
      base::Bind(&FallbackCallback, proxy_url, net_error);
  PostEvent(net_log, net::NetLog::TYPE_DATA_REDUCTION_PROXY_FALLBACK,
            parameters_callback);
}

void DataReductionProxyEventCreator::BeginSecureProxyCheck(
    const net::BoundNetLog& net_log,
    const GURL& url) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // This callback must be invoked synchronously
  const net::NetLog::ParametersCallback& parameters_callback =
      net::NetLog::StringCallback("url", &url.spec());
  PostBoundNetLogSecureProxyCheckEvent(
      net_log, net::NetLog::TYPE_DATA_REDUCTION_PROXY_CANARY_REQUEST,
      net::NetLog::PHASE_BEGIN,
      DataReductionProxyEventStorageDelegate::CHECK_PENDING,
      parameters_callback);
}

void DataReductionProxyEventCreator::EndSecureProxyCheck(
    const net::BoundNetLog& net_log,
    int net_error,
    int http_response_code,
    bool succeeded) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const net::NetLog::ParametersCallback& parameters_callback = base::Bind(
      &EndCanaryRequestCallback, net_error, http_response_code, succeeded);
  PostBoundNetLogSecureProxyCheckEvent(
      net_log, net::NetLog::TYPE_DATA_REDUCTION_PROXY_CANARY_REQUEST,
      net::NetLog::PHASE_END,
      succeeded ? DataReductionProxyEventStorageDelegate::CHECK_SUCCESS
                : DataReductionProxyEventStorageDelegate::CHECK_FAILED,
      parameters_callback);
}

void DataReductionProxyEventCreator::BeginConfigRequest(
    const net::BoundNetLog& net_log,
    const GURL& url) {
  // This callback must be invoked synchronously.
  const net::NetLog::ParametersCallback& parameters_callback =
      net::NetLog::StringCallback("url", &url.spec());
  PostBoundNetLogConfigRequestEvent(
      net_log, net::NetLog::TYPE_DATA_REDUCTION_PROXY_CONFIG_REQUEST,
      net::NetLog::PHASE_BEGIN, parameters_callback);
}

void DataReductionProxyEventCreator::EndConfigRequest(
    const net::BoundNetLog& net_log,
    int net_error,
    int http_response_code,
    int failure_count,
    const base::TimeDelta& retry_delay) {
  int64 expiration_ticks = GetExpirationTicks(retry_delay.InSeconds());
  const net::NetLog::ParametersCallback& parameters_callback =
      base::Bind(&EndConfigRequestCallback, net_error, http_response_code,
                 failure_count, expiration_ticks);
  PostBoundNetLogConfigRequestEvent(
      net_log, net::NetLog::TYPE_DATA_REDUCTION_PROXY_CONFIG_REQUEST,
      net::NetLog::PHASE_END, parameters_callback);
}

void DataReductionProxyEventCreator::PostEvent(
    net::NetLog* net_log,
    net::NetLog::EventType type,
    const net::NetLog::ParametersCallback& callback) {
  scoped_ptr<base::Value> event = BuildDataReductionProxyEvent(
      type, net::NetLog::Source(), net::NetLog::PHASE_NONE, callback);
  if (event)
    storage_delegate_->AddEvent(event.Pass());

  if (net_log)
    net_log->AddGlobalEntry(type, callback);
}

void DataReductionProxyEventCreator::PostEnabledEvent(
    net::NetLog* net_log,
    net::NetLog::EventType type,
    bool enabled,
    const net::NetLog::ParametersCallback& callback) {
  scoped_ptr<base::Value> event = BuildDataReductionProxyEvent(
      type, net::NetLog::Source(), net::NetLog::PHASE_NONE, callback);
  if (event)
    storage_delegate_->AddEnabledEvent(event.Pass(), enabled);

  if (net_log)
    net_log->AddGlobalEntry(type, callback);
}

void DataReductionProxyEventCreator::PostBoundNetLogBypassEvent(
    const net::BoundNetLog& net_log,
    net::NetLog::EventType type,
    net::NetLog::EventPhase phase,
    int64 expiration_ticks,
    const net::NetLog::ParametersCallback& callback) {
  scoped_ptr<base::Value> event =
      BuildDataReductionProxyEvent(type, net_log.source(), phase, callback);
  if (event)
    storage_delegate_->AddAndSetLastBypassEvent(event.Pass(), expiration_ticks);
  net_log.AddEntry(type, phase, callback);
}

void DataReductionProxyEventCreator::PostBoundNetLogSecureProxyCheckEvent(
    const net::BoundNetLog& net_log,
    net::NetLog::EventType type,
    net::NetLog::EventPhase phase,
    DataReductionProxyEventStorageDelegate::SecureProxyCheckState state,
    const net::NetLog::ParametersCallback& callback) {
  scoped_ptr<base::Value> event(
      BuildDataReductionProxyEvent(type, net_log.source(), phase, callback));
  if (event)
    storage_delegate_->AddEventAndSecureProxyCheckState(event.Pass(), state);
  net_log.AddEntry(type, phase, callback);
}

void DataReductionProxyEventCreator::PostBoundNetLogConfigRequestEvent(
    const net::BoundNetLog& net_log,
    net::NetLog::EventType type,
    net::NetLog::EventPhase phase,
    const net::NetLog::ParametersCallback& callback) {
  scoped_ptr<base::Value> event(
      BuildDataReductionProxyEvent(type, net_log.source(), phase, callback));
  if (event) {
    storage_delegate_->AddEvent(event.Pass());
    net_log.AddEntry(type, phase, callback);
  }
}

}  // namespace data_reduction_proxy

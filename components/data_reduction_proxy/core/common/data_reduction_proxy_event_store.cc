// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_store.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/net_log.h"

namespace {

const size_t kMaxEventsToStore = 100;

scoped_ptr<base::Value> BuildDataReductionProxyEvent(
    net::NetLog::EventType type,
    const net::NetLog::Source& source,
    net::NetLog::EventPhase phase,
    const net::NetLog::ParametersCallback& parameters_callback) {
  base::TimeTicks ticks_now = base::TimeTicks::Now();
  net::NetLog::EntryData entry_data(
      type, source, phase, ticks_now, &parameters_callback);
  net::NetLog::Entry entry(&entry_data, net::NetLog::LOG_ALL);
  scoped_ptr<base::Value> entry_value(entry.ToValue());

  return entry_value;
}

std::string GetExpirationTicks(int bypass_seconds) {
  base::TimeTicks ticks_now = base::TimeTicks::Now();
  return net::NetLog::TickCountToString(
      ticks_now + base::TimeDelta::FromSeconds(bypass_seconds));
}

// The following callbacks create a base::Value which contains information
// about various data reduction proxy events. Ownership of the base::Value is
// passed to the caller.
base::Value* EnableDataReductionProxyCallback(
    bool primary_restricted,
    bool fallback_restricted,
    const std::string& primary_origin,
    const std::string& fallback_origin,
    const std::string& ssl_origin,
    net::NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetBoolean("enabled", true);
  dict->SetBoolean("primary_restricted", primary_restricted);
  dict->SetBoolean("fallback_restricted", fallback_restricted);
  dict->SetString("primary_origin", primary_origin);
  dict->SetString("fallback_origin", fallback_origin);
  dict->SetString("ssl_origin", ssl_origin);
  return dict;
}

base::Value* DisableDataReductionProxyCallback(
    net::NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetBoolean("enabled", false);
  return dict;
}

base::Value* UrlBypassActionCallback(const std::string& action,
                                     const GURL& url,
                                     const base::TimeDelta& bypass_duration,
                                     net::NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  int bypass_seconds = bypass_duration.InSeconds();
  dict->SetString("action", action);
  dict->SetString("url", url.spec());
  dict->SetString("bypass_duration_seconds",
                  base::Int64ToString(bypass_seconds));
  dict->SetString("expiration", GetExpirationTicks(bypass_seconds));
  return dict;
}

base::Value* UrlBypassTypeCallback(
    data_reduction_proxy::DataReductionProxyBypassType bypass_type,
    const GURL& url,
    const base::TimeDelta& bypass_duration,
    net::NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  int bypass_seconds = bypass_duration.InSeconds();
  dict->SetInteger("bypass_type", bypass_type);
  dict->SetString("url", url.spec());
  dict->SetString("bypass_duration_seconds",
                  base::Int64ToString(bypass_seconds));
  dict->SetString("expiration", GetExpirationTicks(bypass_seconds));
  return dict;
}

}  // namespace

namespace data_reduction_proxy {

DataReductionProxyEventStore::DataReductionProxyEventStore(
    const scoped_refptr<base::SingleThreadTaskRunner>& network_task_runner)
    : network_task_runner_(network_task_runner) {
}

DataReductionProxyEventStore::~DataReductionProxyEventStore() {
  STLDeleteElements(&stored_events_);
  stored_events_.clear();
}

void DataReductionProxyEventStore::AddProxyEnabledEvent(
    net::NetLog* net_log,
    bool primary_restricted,
    bool fallback_restricted,
    const std::string& primary_origin,
    const std::string& fallback_origin,
    const std::string& ssl_origin) {
  const net::NetLog::ParametersCallback& parameters_callback =
      base::Bind(&EnableDataReductionProxyCallback, primary_restricted,
                 fallback_restricted, primary_origin, fallback_origin,
                 ssl_origin);
  PostGlobalNetLogEvent(net_log,
                        net::NetLog::TYPE_DATA_REDUCTION_PROXY_ENABLED,
                        parameters_callback);
}

void DataReductionProxyEventStore::AddProxyDisabledEvent(
    net::NetLog* net_log) {
  const net::NetLog::ParametersCallback& parameters_callback =
      base::Bind(&DisableDataReductionProxyCallback);
  PostGlobalNetLogEvent(net_log,
                        net::NetLog::TYPE_DATA_REDUCTION_PROXY_ENABLED,
                        parameters_callback);
}

void DataReductionProxyEventStore::AddBypassActionEvent(
    const net::BoundNetLog& net_log,
    const std::string& bypass_action,
    const GURL& url,
    const base::TimeDelta& bypass_duration) {
  const net::NetLog::ParametersCallback& parameters_callback =
      base::Bind(&UrlBypassActionCallback, bypass_action, url, bypass_duration);
  PostBoundNetLogEvent(net_log,
                       net::NetLog::TYPE_DATA_REDUCTION_PROXY_BYPASS_REQUESTED,
                       net::NetLog::PHASE_NONE,
                       parameters_callback);
}

void DataReductionProxyEventStore::AddBypassTypeEvent(
    const net::BoundNetLog& net_log,
    DataReductionProxyBypassType bypass_type,
    const GURL& url,
    const base::TimeDelta& bypass_duration) {
  const net::NetLog::ParametersCallback& parameters_callback =
      base::Bind(&UrlBypassTypeCallback, bypass_type, url, bypass_duration);
  PostBoundNetLogEvent(net_log,
                       net::NetLog::TYPE_DATA_REDUCTION_PROXY_BYPASS_REQUESTED,
                       net::NetLog::PHASE_NONE,
                       parameters_callback);
}

void DataReductionProxyEventStore::BeginCanaryRequest(
    const net::BoundNetLog& net_log,
    const GURL& url) {
  // This callback must be invoked synchronously
  const net::NetLog::ParametersCallback& parameters_callback =
      net::NetLog::StringCallback("url", &url.spec());
  PostBoundNetLogEvent(net_log,
                       net::NetLog::TYPE_DATA_REDUCTION_PROXY_CANARY_REQUEST,
                       net::NetLog::PHASE_BEGIN,
                       parameters_callback);
}

void DataReductionProxyEventStore::EndCanaryRequest(
    const net::BoundNetLog& net_log,
    int net_error) {
  const net::NetLog::ParametersCallback& parameters_callback =
      net::NetLog::IntegerCallback("net_error", net_error);
  PostBoundNetLogEvent(net_log,
                       net::NetLog::TYPE_DATA_REDUCTION_PROXY_CANARY_REQUEST,
                       net::NetLog::PHASE_END,
                       parameters_callback);
}

void DataReductionProxyEventStore::PostGlobalNetLogEvent(
    net::NetLog* net_log,
    net::NetLog::EventType type,
    const net::NetLog::ParametersCallback& callback) {
  scoped_ptr<base::Value> event = BuildDataReductionProxyEvent(
      type, net::NetLog::Source(), net::NetLog::PHASE_NONE, callback);
  if (event.get()) {
    network_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DataReductionProxyEventStore::AddEventOnIOThread,
                   base::Unretained(this),
                   base::Passed(&event)));
  }

  if (net_log)
    net_log->AddGlobalEntry(type, callback);
}

void DataReductionProxyEventStore::PostBoundNetLogEvent(
    const net::BoundNetLog& net_log,
    net::NetLog::EventType type,
    net::NetLog::EventPhase phase,
    const net::NetLog::ParametersCallback& callback) {
  scoped_ptr<base::Value> event = BuildDataReductionProxyEvent(
      type, net_log.source(), phase, callback);
  if (event.get()) {
    network_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DataReductionProxyEventStore::AddEventOnIOThread,
                   base::Unretained(this),
                   base::Passed(&event)));
  }

  net_log.AddEntry(type, phase, callback);
}

void DataReductionProxyEventStore::AddEventOnIOThread(
    scoped_ptr<base::Value> entry) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  if (stored_events_.size() == kMaxEventsToStore) {
    base::Value* head = stored_events_.front();
    stored_events_.pop_front();
    delete head;
  }

  stored_events_.push_back(entry.release());
}

}  // namespace data_reduction_proxy

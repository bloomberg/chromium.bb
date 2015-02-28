// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_store.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_log.h"
#include "net/proxy/proxy_server.h"
#include "url/gurl.h"

namespace {

const size_t kMaxEventsToStore = 100;

struct StringToConstant {
  const char* name;
  const int constant;
};

const StringToConstant kDataReductionProxyBypassEventTypeTable[] = {
#define BYPASS_EVENT_TYPE(label, value) { \
    # label, data_reduction_proxy::BYPASS_EVENT_TYPE_ ## label },
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_bypass_type_list.h"
#undef BYPASS_EVENT_TYPE
};

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

int64 GetExpirationTicks(int bypass_seconds) {
  base::TimeTicks ticks_now = base::TimeTicks::Now();
  base::TimeTicks expiration_ticks =
      ticks_now + base::TimeDelta::FromSeconds(bypass_seconds);
  return (expiration_ticks - base::TimeTicks()).InMilliseconds();
}

// The following method creates a string resembling the output of
// net::ProxyServer::ToURI().
std::string GetNormalizedProxyString(const std::string& proxy_origin) {
  net::ProxyServer proxy_server = net::ProxyServer::FromURI(
    proxy_origin, net::ProxyServer::SCHEME_HTTP);
  if (proxy_server.is_valid())
    return proxy_origin;
  else
    return std::string();
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
  dict->SetString("primary_origin", GetNormalizedProxyString(primary_origin));
  dict->SetString("fallback_origin", GetNormalizedProxyString(fallback_origin));
  dict->SetString("ssl_origin", GetNormalizedProxyString(ssl_origin));
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
                                     int bypass_seconds,
                                     int64 expiration_ticks,
                                     net::NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetString("action", action);
  dict->SetString("url", url.spec());
  dict->SetString("bypass_duration_seconds",
                  base::Int64ToString(bypass_seconds));
  dict->SetString("expiration", base::Int64ToString(expiration_ticks));
  return dict;
}

base::Value* UrlBypassTypeCallback(
    data_reduction_proxy::DataReductionProxyBypassType bypass_type,
    const GURL& url,
    int bypass_seconds,
    int64 expiration_ticks,
    net::NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetInteger("bypass_type", bypass_type);
  dict->SetString("url", url.spec());
  dict->SetString("bypass_duration_seconds",
                  base::Int64ToString(bypass_seconds));
  dict->SetString("expiration", base::Int64ToString(expiration_ticks));
  return dict;
}

}  // namespace

namespace data_reduction_proxy {

// static
void DataReductionProxyEventStore::AddConstants(
    base::DictionaryValue* constants_dict) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  for (size_t i = 0;
       i < arraysize(kDataReductionProxyBypassEventTypeTable); ++i) {
    dict->SetInteger(kDataReductionProxyBypassEventTypeTable[i].name,
                     kDataReductionProxyBypassEventTypeTable[i].constant);
  }

  constants_dict->Set("dataReductionProxyBypassEventType", dict);
}

DataReductionProxyEventStore::DataReductionProxyEventStore(
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner)
    : ui_task_runner_(ui_task_runner),
      enabled_(false),
      secure_proxy_check_state_(CHECK_UNKNOWN),
      expiration_ticks_(0) {
}

DataReductionProxyEventStore::~DataReductionProxyEventStore() {
  STLDeleteElements(&stored_events_);
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
  PostEnabledEvent(net_log,
                   net::NetLog::TYPE_DATA_REDUCTION_PROXY_ENABLED,
                   true,
                   parameters_callback);
}

void DataReductionProxyEventStore::AddProxyDisabledEvent(
    net::NetLog* net_log) {
  const net::NetLog::ParametersCallback& parameters_callback =
      base::Bind(&DisableDataReductionProxyCallback);
  PostEnabledEvent(net_log,
                   net::NetLog::TYPE_DATA_REDUCTION_PROXY_ENABLED,
                   false,
                   parameters_callback);
}

void DataReductionProxyEventStore::AddBypassActionEvent(
    const net::BoundNetLog& net_log,
    const std::string& bypass_action,
    const GURL& url,
    const base::TimeDelta& bypass_duration) {
  int64 expiration_ticks = GetExpirationTicks(bypass_duration.InSeconds());
  const net::NetLog::ParametersCallback& parameters_callback =
      base::Bind(&UrlBypassActionCallback, bypass_action, url,
                 bypass_duration.InSeconds(), expiration_ticks);
  PostBoundNetLogBypassEvent(
      net_log,
      net::NetLog::TYPE_DATA_REDUCTION_PROXY_BYPASS_REQUESTED,
      net::NetLog::PHASE_NONE,
      expiration_ticks,
      parameters_callback);
}

void DataReductionProxyEventStore::AddBypassTypeEvent(
    const net::BoundNetLog& net_log,
    DataReductionProxyBypassType bypass_type,
    const GURL& url,
    const base::TimeDelta& bypass_duration) {
  int64 expiration_ticks = GetExpirationTicks(bypass_duration.InSeconds());
  const net::NetLog::ParametersCallback& parameters_callback =
      base::Bind(&UrlBypassTypeCallback, bypass_type, url,
                 bypass_duration.InSeconds(), expiration_ticks);
  PostBoundNetLogBypassEvent(
      net_log,
      net::NetLog::TYPE_DATA_REDUCTION_PROXY_BYPASS_REQUESTED,
      net::NetLog::PHASE_NONE,
      expiration_ticks,
      parameters_callback);
}

void DataReductionProxyEventStore::BeginSecureProxyCheck(
    const net::BoundNetLog& net_log,
    const GURL& url) {
  // This callback must be invoked synchronously
  const net::NetLog::ParametersCallback& parameters_callback =
      net::NetLog::StringCallback("url", &url.spec());
  PostBoundNetLogSecureProxyCheckEvent(
      net_log,
      net::NetLog::TYPE_DATA_REDUCTION_PROXY_CANARY_REQUEST,
      net::NetLog::PHASE_BEGIN,
      CHECK_PENDING,
      parameters_callback);
}

void DataReductionProxyEventStore::EndSecureProxyCheck(
    const net::BoundNetLog& net_log,
    int net_error) {
  const net::NetLog::ParametersCallback& parameters_callback =
      net::NetLog::IntegerCallback("net_error", net_error);
  PostBoundNetLogSecureProxyCheckEvent(
      net_log,
      net::NetLog::TYPE_DATA_REDUCTION_PROXY_CANARY_REQUEST,
      net::NetLog::PHASE_END,
      net_error == 0 ? CHECK_SUCCESS : CHECK_FAILED,
      parameters_callback);
}

void DataReductionProxyEventStore::PostEnabledEvent(
    net::NetLog* net_log,
    net::NetLog::EventType type,
    bool enabled,
    const net::NetLog::ParametersCallback& callback) {
  scoped_ptr<base::Value> event = BuildDataReductionProxyEvent(
      type, net::NetLog::Source(), net::NetLog::PHASE_NONE, callback);
  if (event.get()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DataReductionProxyEventStore::AddEnabledEventOnUIThread,
                   base::Unretained(this),
                   base::Passed(&event),
                   enabled));
  }

  if (net_log)
    net_log->AddGlobalEntry(type, callback);
}

void DataReductionProxyEventStore::PostBoundNetLogBypassEvent(
    const net::BoundNetLog& net_log,
    net::NetLog::EventType type,
    net::NetLog::EventPhase phase,
    int64 expiration_ticks,
    const net::NetLog::ParametersCallback& callback) {
  scoped_ptr<base::Value> event = BuildDataReductionProxyEvent(
      type, net_log.source(), phase, callback);
  if (event.get()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(
            &DataReductionProxyEventStore::AddAndSetLastBypassEventOnUIThread,
            base::Unretained(this),
            base::Passed(&event),
            expiration_ticks));
  }

  net_log.AddEntry(type, phase, callback);
}

void DataReductionProxyEventStore::PostBoundNetLogSecureProxyCheckEvent(
    const net::BoundNetLog& net_log,
    net::NetLog::EventType type,
    net::NetLog::EventPhase phase,
    SecureProxyCheckState state,
    const net::NetLog::ParametersCallback& callback) {
  scoped_ptr<base::Value> event(
      BuildDataReductionProxyEvent(type, net_log.source(), phase, callback));
  if (event.get()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(
            &DataReductionProxyEventStore::
                AddEventAndSecureProxyCheckStateOnUIThread,
            base::Unretained(this),
            base::Passed(&event),
            state));
  }
  net_log.AddEntry(type, phase, callback);
}

base::Value* DataReductionProxyEventStore::GetSummaryValue() const {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  scoped_ptr<base::DictionaryValue> data_reduction_proxy_values(
      new base::DictionaryValue());
  data_reduction_proxy_values->SetBoolean("enabled", enabled_);

  base::Value* current_configuration = current_configuration_.get();
  if (current_configuration != nullptr) {
    data_reduction_proxy_values->Set("proxy_config",
                                     current_configuration->DeepCopy());
  }

  switch (secure_proxy_check_state_) {
    case CHECK_PENDING:
      data_reduction_proxy_values->SetString("probe", "Pending");
      break;
    case CHECK_SUCCESS:
      data_reduction_proxy_values->SetString("probe", "Success");
      break;
    case CHECK_FAILED:
      data_reduction_proxy_values->SetString("probe", "Failed");
      break;
    case CHECK_UNKNOWN:
      break;
    default:
      NOTREACHED();
      break;
  }

  base::Value* last_bypass_event = last_bypass_event_.get();
  if (last_bypass_event != nullptr) {
    int current_time_ticks_ms =
        (base::TimeTicks::Now() - base::TimeTicks()).InMilliseconds();
    if (expiration_ticks_ > current_time_ticks_ms) {
      data_reduction_proxy_values->Set("last_bypass",
                                       last_bypass_event->DeepCopy());
    }
  }

  base::ListValue* eventsList = new base::ListValue();
  for (size_t i = 0; i < stored_events_.size(); ++i)
    eventsList->Append(stored_events_[i]->DeepCopy());

  data_reduction_proxy_values->Set("events", eventsList);

  return data_reduction_proxy_values.release();
}

void DataReductionProxyEventStore::AddEventOnUIThread(
    scoped_ptr<base::Value> entry) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  if (stored_events_.size() == kMaxEventsToStore) {
    base::Value* head = stored_events_.front();
    stored_events_.pop_front();
    delete head;
  }

  stored_events_.push_back(entry.release());
}

void DataReductionProxyEventStore::AddEnabledEventOnUIThread(
    scoped_ptr<base::Value> entry,
    bool enabled) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  enabled_ = enabled;
  if (enabled)
    current_configuration_.reset(entry->DeepCopy());
  else
    current_configuration_.reset();
  AddEventOnUIThread(entry.Pass());
}

void DataReductionProxyEventStore::AddEventAndSecureProxyCheckStateOnUIThread(
    scoped_ptr<base::Value> entry,
    SecureProxyCheckState state) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  secure_proxy_check_state_ = state;
  AddEventOnUIThread(entry.Pass());
}

void DataReductionProxyEventStore::AddAndSetLastBypassEventOnUIThread(
    scoped_ptr<base::Value> entry,
    int64 expiration_ticks) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  last_bypass_event_.reset(entry->DeepCopy());
  expiration_ticks_ = expiration_ticks;
  AddEventOnUIThread(entry.Pass());
}

}  // namespace data_reduction_proxy

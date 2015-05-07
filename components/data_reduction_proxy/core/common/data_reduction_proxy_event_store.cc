// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_store.h"

#include "base/basictypes.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_storage_delegate.h"

namespace {

const size_t kMaxEventsToStore = 100;

struct StringToConstant {
  const char* name;
  const int constant;
};

// Creates an associative array of the enum name to enum value for
// DataReductionProxyBypassType. Ensures that the same enum space is used
// without having to keep an enum map in sync.
const StringToConstant kDataReductionProxyBypassEventTypeTable[] = {
#define BYPASS_EVENT_TYPE(label, value) { \
    # label, data_reduction_proxy::BYPASS_EVENT_TYPE_ ## label },
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_bypass_type_list.h"
#undef BYPASS_EVENT_TYPE
};

// Creates an associate array of the enum name to enum value for
// DataReductionProxyBypassAction. Ensures that the same enum space is used
// without having to keep an enum map in sync.
const StringToConstant kDataReductionProxyBypassActionTypeTable[] = {
#define BYPASS_ACTION_TYPE(label, value)                       \
  { #label, data_reduction_proxy::BYPASS_ACTION_TYPE_##label } \
  ,
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_bypass_action_list.h"
#undef BYPASS_ACTION_TYPE
};

}  // namespace

namespace data_reduction_proxy {

// static
void DataReductionProxyEventStore::AddConstants(
    base::DictionaryValue* constants_dict) {
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  for (size_t i = 0;
       i < arraysize(kDataReductionProxyBypassEventTypeTable); ++i) {
    dict->SetInteger(kDataReductionProxyBypassEventTypeTable[i].name,
                     kDataReductionProxyBypassEventTypeTable[i].constant);
  }

  constants_dict->Set("dataReductionProxyBypassEventType", dict.Pass());

  dict.reset(new base::DictionaryValue());
  for (size_t i = 0; i < arraysize(kDataReductionProxyBypassActionTypeTable);
       ++i) {
    dict->SetInteger(kDataReductionProxyBypassActionTypeTable[i].name,
                     kDataReductionProxyBypassActionTypeTable[i].constant);
  }

  constants_dict->Set("dataReductionProxyBypassActionType", dict.Pass());
}

DataReductionProxyEventStore::DataReductionProxyEventStore()
    : enabled_(false),
      secure_proxy_check_state_(CHECK_UNKNOWN),
      expiration_ticks_(0) {
}

DataReductionProxyEventStore::~DataReductionProxyEventStore() {
  STLDeleteElements(&stored_events_);
}

base::Value* DataReductionProxyEventStore::GetSummaryValue() const {
  DCHECK(thread_checker_.CalledOnValidThread());
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

void DataReductionProxyEventStore::AddEvent(scoped_ptr<base::Value> event) {
  if (stored_events_.size() == kMaxEventsToStore) {
    base::Value* head = stored_events_.front();
    stored_events_.pop_front();
    delete head;
  }

  stored_events_.push_back(event.release());
}

void DataReductionProxyEventStore::AddEnabledEvent(
    scoped_ptr<base::Value> event,
    bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  enabled_ = enabled;
  if (enabled)
    current_configuration_.reset(event->DeepCopy());
  else
    current_configuration_.reset();
  AddEvent(event.Pass());
}

void DataReductionProxyEventStore::AddEventAndSecureProxyCheckState(
    scoped_ptr<base::Value> event,
    SecureProxyCheckState state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  secure_proxy_check_state_ = state;
  AddEvent(event.Pass());
}

void DataReductionProxyEventStore::AddAndSetLastBypassEvent(
    scoped_ptr<base::Value> event,
    int64 expiration_ticks) {
  DCHECK(thread_checker_.CalledOnValidThread());
  last_bypass_event_.reset(event->DeepCopy());
  expiration_ticks_ = expiration_ticks;
  AddEvent(event.Pass());
}

}  // namespace data_reduction_proxy

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/js/js_mutation_event_observer.h"

#include <string>

#include "base/location.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/sync/js/js_event_details.h"
#include "chrome/browser/sync/js/js_event_handler.h"

namespace browser_sync {

JsMutationEventObserver::JsMutationEventObserver()
    : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {}

JsMutationEventObserver::~JsMutationEventObserver() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
}

base::WeakPtr<JsMutationEventObserver> JsMutationEventObserver::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void JsMutationEventObserver::InvalidateWeakPtrs() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void JsMutationEventObserver::SetJsEventHandler(
    const WeakHandle<JsEventHandler>& event_handler) {
  event_handler_ = event_handler;
}

namespace {

// Max number of changes we attempt to convert to values (to avoid
// running out of memory).
const size_t kChangeLimit = 100;

}  // namespace

void JsMutationEventObserver::OnChangesApplied(
    syncable::ModelType model_type,
    int64 write_transaction_id,
    const sync_api::ImmutableChangeRecordList& changes) {
  if (!event_handler_.IsInitialized()) {
    return;
  }
  DictionaryValue details;
  details.SetString("modelType", syncable::ModelTypeToString(model_type));
  details.SetString("writeTransactionId",
                    base::Int64ToString(write_transaction_id));
  base::Value* changes_value = NULL;
  const size_t changes_size = changes.Get().size();
  if (changes_size <= kChangeLimit) {
    ListValue* changes_list = new ListValue();
    for (sync_api::ChangeRecordList::const_iterator it =
             changes.Get().begin(); it != changes.Get().end(); ++it) {
      changes_list->Append(it->ToValue());
    }
    changes_value = changes_list;
  } else {
    changes_value =
        Value::CreateStringValue(
            base::Uint64ToString(static_cast<uint64>(changes_size)) +
            " changes");
  }
  details.Set("changes", changes_value);
  HandleJsEvent(FROM_HERE, "onChangesApplied", JsEventDetails(&details));
}

void JsMutationEventObserver::OnChangesComplete(
    syncable::ModelType model_type) {
  if (!event_handler_.IsInitialized()) {
    return;
  }
  DictionaryValue details;
  details.SetString("modelType", syncable::ModelTypeToString(model_type));
  HandleJsEvent(FROM_HERE, "onChangesComplete", JsEventDetails(&details));
}

void JsMutationEventObserver::OnTransactionWrite(
    const syncable::ImmutableWriteTransactionInfo& write_transaction_info,
    const syncable::ModelTypeBitSet& models_with_changes) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  if (!event_handler_.IsInitialized()) {
    return;
  }
  DictionaryValue details;
  details.Set("writeTransactionInfo",
              write_transaction_info.Get().ToValue(kChangeLimit));
  details.Set("modelsWithChanges",
              syncable::ModelTypeBitSetToValue(models_with_changes));
  HandleJsEvent(FROM_HERE, "onTransactionWrite", JsEventDetails(&details));
}

void JsMutationEventObserver::HandleJsEvent(
    const tracked_objects::Location& from_here,
    const std::string& name, const JsEventDetails& details) {
  if (!event_handler_.IsInitialized()) {
    NOTREACHED();
    return;
  }
  event_handler_.Call(from_here,
                      &JsEventHandler::HandleJsEvent, name, details);
}

}  // namespace browser_sync

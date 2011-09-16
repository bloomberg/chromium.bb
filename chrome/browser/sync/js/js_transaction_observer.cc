// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/js/js_transaction_observer.h"

#include <string>

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/tracked.h"
#include "base/values.h"
#include "chrome/browser/sync/js/js_event_details.h"
#include "chrome/browser/sync/js/js_event_handler.h"

namespace browser_sync {

JsTransactionObserver::JsTransactionObserver() {}

JsTransactionObserver::~JsTransactionObserver() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
}

void JsTransactionObserver::SetJsEventHandler(
    const WeakHandle<JsEventHandler>& event_handler) {
  event_handler_ = event_handler;
}

void JsTransactionObserver::OnTransactionStart(
    const tracked_objects::Location& location,
    const syncable::WriterTag& writer) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  if (!event_handler_.IsInitialized()) {
    return;
  }
  DictionaryValue details;
  details.SetString("location", location.ToString());
  details.SetString("writer", syncable::WriterTagToString(writer));
  HandleJsEvent(FROM_HERE, "onTransactionStart", JsEventDetails(&details));
}

namespace {

// Max number of mutations we attempt to convert to values (to avoid
// running out of memory).
const size_t kMutationLimit = 300;

}  // namespace

void JsTransactionObserver::OnTransactionMutate(
    const tracked_objects::Location& location,
    const syncable::WriterTag& writer,
    const syncable::ImmutableEntryKernelMutationMap& mutations,
    const syncable::ModelTypeBitSet& models_with_changes) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  if (!event_handler_.IsInitialized()) {
    return;
  }
  DictionaryValue details;
  details.SetString("location", location.ToString());
  details.SetString("writer", syncable::WriterTagToString(writer));
  Value* mutations_value = NULL;
  const size_t mutations_size = mutations.Get().size();
  if (mutations_size <= kMutationLimit) {
    mutations_value = syncable::EntryKernelMutationMapToValue(mutations.Get());
  } else {
    mutations_value =
        Value::CreateStringValue(
            base::Uint64ToString(static_cast<uint64>(mutations_size)) +
            " mutations");
  }
  details.Set("mutations", mutations_value);
  details.Set("modelsWithChanges",
              syncable::ModelTypeBitSetToValue(models_with_changes));
  HandleJsEvent(FROM_HERE, "onTransactionMutate", JsEventDetails(&details));
}

void JsTransactionObserver::OnTransactionEnd(
    const tracked_objects::Location& location,
    const syncable::WriterTag& writer) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  if (!event_handler_.IsInitialized()) {
    return;
  }
  DictionaryValue details;
  details.SetString("location", location.ToString());
  details.SetString("writer", syncable::WriterTagToString(writer));
  HandleJsEvent(FROM_HERE, "onTransactionEnd", JsEventDetails(&details));
}

void JsTransactionObserver::HandleJsEvent(
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

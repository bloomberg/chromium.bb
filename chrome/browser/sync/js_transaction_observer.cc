// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/js_transaction_observer.h"

#include <sstream>
#include <string>

#include "base/logging.h"
#include "base/tracked.h"
#include "base/values.h"
#include "chrome/browser/sync/js_event_details.h"
#include "chrome/browser/sync/js_event_router.h"

namespace browser_sync {

JsTransactionObserver::JsTransactionObserver(
    JsEventRouter* parent_router)
    : parent_router_(parent_router) {
  DCHECK(parent_router_);
}

JsTransactionObserver::~JsTransactionObserver() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
}

namespace {

std::string GetLocationString(const tracked_objects::Location& location) {
  std::ostringstream oss;
  oss << location.function_name() << "@"
      << location.file_name() << ":" << location.line_number();
  return oss.str();
}

}  // namespace

void JsTransactionObserver::OnTransactionStart(
    const tracked_objects::Location& location,
    const syncable::WriterTag& writer) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  DictionaryValue details;
  details.SetString("location", GetLocationString(location));
  details.SetString("writer", syncable::WriterTagToString(writer));
  parent_router_->RouteJsEvent("onTransactionStart",
                               JsEventDetails(&details));
}

void JsTransactionObserver::OnTransactionMutate(
    const tracked_objects::Location& location,
    const syncable::WriterTag& writer,
    const syncable::EntryKernelMutationSet& mutations,
    const syncable::ModelTypeBitSet& models_with_changes) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  DictionaryValue details;
  details.SetString("location", GetLocationString(location));
  details.SetString("writer", syncable::WriterTagToString(writer));
  details.Set("mutations", syncable::EntryKernelMutationSetToValue(mutations));
  details.Set("modelsWithChanges",
              syncable::ModelTypeBitSetToValue(models_with_changes));
  parent_router_->RouteJsEvent("onTransactionMutate",
                               JsEventDetails(&details));
}

void JsTransactionObserver::OnTransactionEnd(
    const tracked_objects::Location& location,
    const syncable::WriterTag& writer) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  DictionaryValue details;
  details.SetString("location", GetLocationString(location));
  details.SetString("writer", syncable::WriterTagToString(writer));
  parent_router_->RouteJsEvent("onTransactionEnd",
                               JsEventDetails(&details));
}

}  // namespace browser_sync

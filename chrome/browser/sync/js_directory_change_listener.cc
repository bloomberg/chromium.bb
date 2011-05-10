// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/js_directory_change_listener.h"

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/sync/js_event_details.h"
#include "chrome/browser/sync/js_event_router.h"

namespace browser_sync {

JsDirectoryChangeListener::JsDirectoryChangeListener(
    JsEventRouter* parent_router)
    : parent_router_(parent_router) {
  DCHECK(parent_router_);
}

JsDirectoryChangeListener::~JsDirectoryChangeListener() {}

void JsDirectoryChangeListener::HandleCalculateChangesChangeEventFromSyncApi(
    const syncable::OriginalEntries& originals,
    const syncable::WriterTag& writer,
    syncable::BaseTransaction* trans) {
  DictionaryValue details;
  details.Set("originals", syncable::OriginalEntriesToValue(originals));
  details.SetString("writer", syncable::WriterTagToString(writer));
  parent_router_->RouteJsEvent("handleCalculateChangesChangeEventFromSyncApi",
                               JsEventDetails(&details));
}

void JsDirectoryChangeListener::HandleCalculateChangesChangeEventFromSyncer(
    const syncable::OriginalEntries& originals,
    const syncable::WriterTag& writer,
    syncable::BaseTransaction* trans) {
  DictionaryValue details;
  details.Set("originals", syncable::OriginalEntriesToValue(originals));
  details.SetString("writer", syncable::WriterTagToString(writer));
  parent_router_->RouteJsEvent("handleCalculateChangesChangeEventFromSyncer",
                               JsEventDetails(&details));
}

syncable::ModelTypeBitSet
    JsDirectoryChangeListener::HandleTransactionEndingChangeEvent(
        syncable::BaseTransaction* trans) {
  parent_router_->RouteJsEvent("handleTransactionEndingChangeEvent",
                               JsEventDetails());
  return syncable::ModelTypeBitSet();
}

void JsDirectoryChangeListener::HandleTransactionCompleteChangeEvent(
    const syncable::ModelTypeBitSet& models_with_changes) {
  DictionaryValue details;
  details.Set("modelsWithChanges",
              syncable::ModelTypeBitSetToValue(models_with_changes));
  parent_router_->RouteJsEvent("handleTransactionCompleteChangeEvent",
                               JsEventDetails(&details));
}

}  // namespace browser_sync

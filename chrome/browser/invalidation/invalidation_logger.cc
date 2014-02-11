// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/invalidation/invalidation_logger.h"

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/invalidation/invalidation_logger_observer.h"

namespace syncer {
class ObjectIdInvalidationMap;
}

namespace invalidation {
class InvalidationLoggerObserver;

InvalidationLogger::InvalidationLogger()
    : last_invalidator_state_(syncer::TRANSIENT_INVALIDATION_ERROR) {}

InvalidationLogger::~InvalidationLogger() {}

void InvalidationLogger::OnRegistration(const base::DictionaryValue& details) {
  FOR_EACH_OBSERVER(
      InvalidationLoggerObserver, observer_list_, OnRegistration(details));
}

void InvalidationLogger::OnUnregistration(
    const base::DictionaryValue& details) {
  FOR_EACH_OBSERVER(
      InvalidationLoggerObserver, observer_list_, OnUnregistration(details));
}

void InvalidationLogger::OnStateChange(
    const syncer::InvalidatorState& newState) {
  last_invalidator_state_ = newState;
  EmitState();
}

void InvalidationLogger::EmitState() {
  FOR_EACH_OBSERVER(InvalidationLoggerObserver,
                    observer_list_,
                    OnStateChange(last_invalidator_state_));
}

void InvalidationLogger::OnUpdateIds(const base::DictionaryValue& details) {
  FOR_EACH_OBSERVER(
      InvalidationLoggerObserver, observer_list_, OnUpdateIds(details));
}

void InvalidationLogger::OnDebugMessage(const base::DictionaryValue& details) {
  FOR_EACH_OBSERVER(
      InvalidationLoggerObserver, observer_list_, OnDebugMessage(details));
}

void InvalidationLogger::OnInvalidation(
    const syncer::ObjectIdInvalidationMap& details) {
  FOR_EACH_OBSERVER(
      InvalidationLoggerObserver, observer_list_, OnInvalidation(details));
}

void InvalidationLogger::EmitContent() {
  // Here we add content to send to the observers not via real time push
  // but on explicit request.
  EmitState();
}

// Obtain a target object to call whenever we have
// debug messages to display
void InvalidationLogger::RegisterForDebug(
    InvalidationLoggerObserver* debug_observer) {
  observer_list_.AddObserver(debug_observer);
}

// Removes the target object to call whenever we have
// debug messages to display
void InvalidationLogger::UnregisterForDebug(
    InvalidationLoggerObserver* debug_observer) {
  observer_list_.RemoveObserver(debug_observer);
}
}  // namespace invalidation

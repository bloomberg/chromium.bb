// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INVALIDATION_INVALIDATION_LOGGER_H_
#define CHROME_BROWSER_INVALIDATION_INVALIDATION_LOGGER_H_

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "sync/notifier/invalidator_state.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace syncer {
class ObjectIdInvalidationMap;
}

namespace invalidation {
class InvalidationLoggerObserver;

class InvalidationLogger {
 public:
  // Pass through to any registered InvalidationLoggerObservers.
  // We will do local logging here too.
  void OnRegistration(const base::DictionaryValue& details);
  void OnUnregistration(const base::DictionaryValue& details);
  void OnStateChange(const syncer::InvalidatorState& newState);
  void OnUpdateIds(const base::DictionaryValue& details);
  void OnDebugMessage(const base::DictionaryValue& details);
  void OnInvalidation(const syncer::ObjectIdInvalidationMap& details);

  void EmitContent();

  InvalidationLogger();
  ~InvalidationLogger();

  void RegisterForDebug(InvalidationLoggerObserver* debug_observer);
  void UnregisterForDebug(InvalidationLoggerObserver* debug_observer);

 private:
  void EmitState();
  // The list of every observer currently listening for notifications.
  ObserverList<InvalidationLoggerObserver> observer_list_;
  // The last InvalidatorState updated by the InvalidatorService.
  syncer::InvalidatorState last_invalidator_state_;
};

}  // namespace invalidation
#endif  // CHROME_BROWSER_INVALIDATION_INVALIDATION_LOGGER_H_

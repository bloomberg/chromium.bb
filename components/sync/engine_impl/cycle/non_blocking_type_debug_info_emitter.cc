// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/cycle/non_blocking_type_debug_info_emitter.h"

#include <vector>

#include "components/sync/engine/cycle/status_counters.h"
#include "components/sync/engine/cycle/type_debug_info_observer.h"

namespace syncer {

NonBlockingTypeDebugInfoEmitter::NonBlockingTypeDebugInfoEmitter(
    ModelType type,
    base::ObserverList<TypeDebugInfoObserver>* observers)
    : DataTypeDebugInfoEmitter(type, observers) {}

NonBlockingTypeDebugInfoEmitter::~NonBlockingTypeDebugInfoEmitter() {}

void NonBlockingTypeDebugInfoEmitter::EmitStatusCountersUpdate() {
  // TODO(gangwu): this function is to show Total Entries on "Types" tab on
  // chrome://sync-internals, we decide to show zero right now, because it is
  // hard to let emitter to get object of SharedModelTypeProcessor or
  // ModelTypeStore, and also people can get the numbers from "about" tab on
  // chrome://sync-internals.
  StatusCounters counters;
  for (auto& observer : *type_debug_info_observers_)
    observer.OnStatusCountersUpdated(type_, counters);
}

}  // namespace syncer

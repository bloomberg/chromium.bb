// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/mock_model_safe_workers.h"

namespace browser_sync {

ModelSafeGroup MockUIModelWorker::GetModelSafeGroup() { return GROUP_UI; }
bool MockUIModelWorker::CurrentThreadIsWorkThread() { return true; }

ModelSafeGroup MockDBModelWorker::GetModelSafeGroup() { return GROUP_DB; }
bool MockDBModelWorker::CurrentThreadIsWorkThread() { return true; }

MockModelSafeWorkerRegistrar::~MockModelSafeWorkerRegistrar() {}

// static
MockModelSafeWorkerRegistrar*
    MockModelSafeWorkerRegistrar::PassiveBookmarks() {
  ModelSafeRoutingInfo routes;
  routes[syncable::BOOKMARKS] = GROUP_PASSIVE;
  MockModelSafeWorkerRegistrar* m = new MockModelSafeWorkerRegistrar(routes);
  m->passive_worker_ = new ModelSafeWorker();
  return m;
}

MockModelSafeWorkerRegistrar* MockModelSafeWorkerRegistrar::PassiveForTypes(
    const syncable::ModelTypeBitSet& set) {
  ModelSafeRoutingInfo routes;
  for (int i = syncable::UNSPECIFIED ; i < syncable::MODEL_TYPE_COUNT; ++i) {
      syncable::ModelType type = syncable::ModelTypeFromInt(i);
      if (set[type]) {
        routes[type] = GROUP_PASSIVE;
      }
  }
  MockModelSafeWorkerRegistrar* m = new MockModelSafeWorkerRegistrar(routes);
  m->passive_worker_ = new ModelSafeWorker();
  return m;
}


void MockModelSafeWorkerRegistrar::GetWorkers(
    std::vector<ModelSafeWorker*>* out) {
  if (passive_worker_.get())
    out->push_back(passive_worker_.get());
}

void MockModelSafeWorkerRegistrar::GetModelSafeRoutingInfo(
    ModelSafeRoutingInfo* out) {
  *out = routes_;
}

MockModelSafeWorkerRegistrar::MockModelSafeWorkerRegistrar(
    const ModelSafeRoutingInfo& routes) {
  routes_ = routes;
}

}  // namespace browser_sync

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/engine/fake_model_safe_worker_registrar.h"

#include "chrome/browser/sync/test/engine/fake_model_worker.h"

namespace browser_sync {

FakeModelSafeWorkerRegistrar::FakeModelSafeWorkerRegistrar(
    const ModelSafeRoutingInfo& routes) : routes_(routes) {
  std::set<ModelSafeGroup> groups;
  for (ModelSafeRoutingInfo::const_iterator it = routes_.begin();
       it != routes_.end(); ++it) {
    groups.insert(it->second);
  }

  for (std::set<ModelSafeGroup>::const_iterator it = groups.begin();
       it != groups.end(); ++it) {
    workers_.push_back(make_scoped_refptr(new FakeModelWorker(*it)));
  }
}

FakeModelSafeWorkerRegistrar::~FakeModelSafeWorkerRegistrar() {}

void FakeModelSafeWorkerRegistrar::GetWorkers(
    std::vector<ModelSafeWorker*>* out) {
  for (std::vector<scoped_refptr<ModelSafeWorker> >::const_iterator it =
           workers_.begin(); it != workers_.end(); ++it) {
    out->push_back(it->get());
  }
}

void FakeModelSafeWorkerRegistrar::GetModelSafeRoutingInfo(
    ModelSafeRoutingInfo* out) {
  *out = routes_;
}

}  // namespace browser_sync

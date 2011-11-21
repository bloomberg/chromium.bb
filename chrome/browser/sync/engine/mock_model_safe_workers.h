// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_MOCK_MODEL_SAFE_WORKERS_H_
#define CHROME_BROWSER_SYNC_ENGINE_MOCK_MODEL_SAFE_WORKERS_H_
#pragma once

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/syncable/model_type.h"

namespace browser_sync {

class MockUIModelWorker : public ModelSafeWorker {
 public:
  virtual ModelSafeGroup GetModelSafeGroup() OVERRIDE;
};

class MockDBModelWorker : public ModelSafeWorker {
 public:
  virtual ModelSafeGroup GetModelSafeGroup() OVERRIDE;
};

class MockFileModelWorker : public ModelSafeWorker {
 public:
  virtual ModelSafeGroup GetModelSafeGroup() OVERRIDE;
};

class MockModelSafeWorkerRegistrar : public ModelSafeWorkerRegistrar {
 public:
  virtual ~MockModelSafeWorkerRegistrar();
  static MockModelSafeWorkerRegistrar* PassiveBookmarks();
  static MockModelSafeWorkerRegistrar* PassiveForTypes(
      const syncable::ModelTypeBitSet& set);
  virtual void GetWorkers(std::vector<ModelSafeWorker*>* out) OVERRIDE;
  virtual void GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out) OVERRIDE;

 private:
  explicit MockModelSafeWorkerRegistrar(const ModelSafeRoutingInfo& routes);

  scoped_refptr<ModelSafeWorker> passive_worker_;
  ModelSafeRoutingInfo routes_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_MOCK_MODEL_SAFE_WORKERS_H_


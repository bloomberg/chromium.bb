// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_ENGINE_FAKE_MODEL_SAFE_WORKER_REGISTRAR_H_
#define CHROME_BROWSER_SYNC_TEST_ENGINE_FAKE_MODEL_SAFE_WORKER_REGISTRAR_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"

namespace browser_sync {

class FakeModelSafeWorkerRegistrar : public ModelSafeWorkerRegistrar {
 public:
  explicit FakeModelSafeWorkerRegistrar(const ModelSafeRoutingInfo& routes);

  virtual ~FakeModelSafeWorkerRegistrar();
  virtual void GetWorkers(std::vector<ModelSafeWorker*>* out) OVERRIDE;
  virtual void GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out) OVERRIDE;

 private:
  const ModelSafeRoutingInfo routes_;
  std::vector<scoped_refptr<ModelSafeWorker> > workers_;

  DISALLOW_COPY_AND_ASSIGN(FakeModelSafeWorkerRegistrar);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_TEST_ENGINE_FAKE_MODEL_SAFE_WORKER_REGISTRAR_H_


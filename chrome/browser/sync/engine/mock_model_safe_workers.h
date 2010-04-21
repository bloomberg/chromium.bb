// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_MOCK_MODEL_SAFE_WORKERS_H_
#define CHROME_BROWSER_SYNC_ENGINE_MOCK_MODEL_SAFE_WORKERS_H_

#include "chrome/browser/sync/engine/model_safe_worker.h"

namespace browser_sync {

class MockUIModelWorker : public ModelSafeWorker {
 public:
  virtual ModelSafeGroup GetModelSafeGroup() { return GROUP_UI; }
  virtual bool CurrentThreadIsWorkThread() { return true; }
};

class MockDBModelWorker : public ModelSafeWorker {
 public:
  virtual ModelSafeGroup GetModelSafeGroup() { return GROUP_DB; }
  virtual bool CurrentThreadIsWorkThread() { return true; }
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_MOCK_MODEL_SAFE_WORKERS_H_


// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_BACKGROUND_SYNC_CONTEXT_IMPL_H_
#define CONTENT_TEST_TEST_BACKGROUND_SYNC_CONTEXT_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "content/browser/background_sync/background_sync_context_impl.h"

namespace content {

class TestBackgroundSyncManager;

// A BackgroundSyncContextImpl for use in unit tests, primarily to create a test
// BackgroundSyncManager.
class TestBackgroundSyncContextImpl : public BackgroundSyncContextImpl {
 public:
  TestBackgroundSyncContextImpl() = default;

 protected:
  ~TestBackgroundSyncContextImpl() override = default;

  // BackgroundSyncContextImpl:
  void CreateBackgroundSyncManager(
      scoped_refptr<ServiceWorkerContextWrapper> context) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestBackgroundSyncContextImpl);
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_BACKGROUND_SYNC_CONTEXT_IMPL_H_

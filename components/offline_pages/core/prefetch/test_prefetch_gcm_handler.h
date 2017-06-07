// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TEST_PREFETCH_GCM_HANDLER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TEST_PREFETCH_GCM_HANDLER_H_

#include <string>

#include "components/offline_pages/core/prefetch/prefetch_gcm_handler.h"

namespace offline_pages {

// Test for testing.
class TestPrefetchGCMHandler : public PrefetchGCMHandler {
 public:
  TestPrefetchGCMHandler();
  ~TestPrefetchGCMHandler() override;

  gcm::GCMAppHandler* AsGCMAppHandler() override;
  std::string GetAppId() const override;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TEST_PREFETCH_GCM_HANDLER_H_

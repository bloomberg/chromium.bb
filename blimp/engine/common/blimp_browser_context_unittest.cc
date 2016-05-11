// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/common/blimp_browser_context.h"

#include "base/message_loop/message_loop.h"
#include "base/metrics/user_metrics.h"
#include "components/metrics/metrics_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/log/net_log.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace engine {

class BlimpBrowserContextTest : public testing::Test {
 public:
  BlimpBrowserContextTest() {}

 protected:
  void SetUp() override {
    base::SetRecordActionTaskRunner(
        base::MessageLoop::current()->task_runner());
    // Create BlimpBrowserContext.
    browser_context_.reset(new BlimpBrowserContext(false /* off_the_record */,
                                                   &net_log_));
  }

  void TearDown() override {
    // Clears static variable in MetricsService
    metrics::MetricsService::SetExecutionPhase(
        metrics::MetricsService::UNINITIALIZED_PHASE,
        browser_context_->GetPrefService());
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  net::NetLog net_log_;
  std::unique_ptr<BlimpBrowserContext> browser_context_;

  DISALLOW_COPY_AND_ASSIGN(BlimpBrowserContextTest);
};

TEST_F(BlimpBrowserContextTest, CtorDtor) {
  // Covers construction and destruction of BlimpBrowserContext and
  // associated metrics setup and teardown.
}

}  // namespace engine
}  // namespace blimp

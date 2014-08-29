// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/metrics/cast_metrics_service_client.h"

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/testing_pref_service.h"
#include "components/metrics/metrics_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {

class CastMetricsTest : public testing::Test {
 public:
  CastMetricsTest() {}
  virtual ~CastMetricsTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
    message_loop_.reset(new base::MessageLoop());
    prefs_.reset(new TestingPrefServiceSimple());
    ::metrics::MetricsService::RegisterPrefs(prefs_->registry());
  }

  TestingPrefServiceSimple* prefs() { return prefs_.get(); }

 private:
  scoped_ptr<base::MessageLoop> message_loop_;
  scoped_ptr<TestingPrefServiceSimple> prefs_;

  DISALLOW_COPY_AND_ASSIGN(CastMetricsTest);
};

TEST_F(CastMetricsTest, CreateMetricsServiceClient) {
  // Create and expect this to not crash.
  metrics::CastMetricsServiceClient::Create(prefs(), NULL);
}

}  // namespace chromecast

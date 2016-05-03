// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/app/blimp_metrics_service_client.h"

#include <list>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "components/metrics/metrics_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace engine {
// Shows notifications which correspond to PersistentPrefStore's reading errors.
void HandleReadError(PersistentPrefStore::PrefReadError error) {
}

class BlimpMetricsServiceClientTest : public testing::Test {
 protected:
  void SetUp() override;
  void TearDown() override;

  void SetUpPrefRegistry();
  void SetUpPrefService();
  void SetUpRequestContextGetter();

  base::MessageLoop message_loop_;
  scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry_;
  std::unique_ptr<PrefService> pref_service_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
};

void BlimpMetricsServiceClientTest::SetUp() {
  // Set up all deps and then InitializeBlimpMetrics.
  SetUpPrefRegistry();
  SetUpPrefService();
  SetUpRequestContextGetter();
  base::SetRecordActionTaskRunner(message_loop_.task_runner());
  InitializeBlimpMetrics(std::move(pref_service_), request_context_getter_);
  base::RunLoop().RunUntilIdle();

  // Checks of InitializeBlimpMetrics.
  // TODO(jessicag): Check request_context_getter_ set.
  // PrefService unique pointer moved to BlimpMetricsServiceClient.
  EXPECT_EQ(nullptr, pref_service_.get());

  // TODO(jessicag): Confirm registration of metrics providers.
  // TODO(jessicag): MetricsService initializes recording state.
  // TODO(jessicag): Confirm MetricsService::Start() is called.
}

void BlimpMetricsServiceClientTest::TearDown() {
  FinalizeBlimpMetrics();
  base::RunLoop().RunUntilIdle();
  // TODO(jessicag): Ensure MetricsService::Stop() is called.
}

void BlimpMetricsServiceClientTest::SetUpPrefRegistry() {
  pref_registry_ = new user_prefs::PrefRegistrySyncable();
  metrics::MetricsService::RegisterPrefs(pref_registry_.get());
}

void BlimpMetricsServiceClientTest::SetUpPrefService() {
  PrefServiceFactory pref_service_factory;
  pref_service_factory.set_user_prefs(new InMemoryPrefStore());
  pref_service_factory.set_read_error_callback(base::Bind(&HandleReadError));
  pref_service_ = pref_service_factory.Create(pref_registry_.get());
  ASSERT_NE(nullptr, pref_service_.get());
}

void BlimpMetricsServiceClientTest::SetUpRequestContextGetter() {
  request_context_getter_ =
      new net::TestURLRequestContextGetter(base::ThreadTaskRunnerHandle::Get());
}

TEST_F(BlimpMetricsServiceClientTest, InitializeFinalize) {
  // SetUp and TearDown handle initialization and finalization checks.
}

}  // namespace engine
}  // namespace blimp

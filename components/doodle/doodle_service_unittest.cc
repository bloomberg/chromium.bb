// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/doodle/doodle_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Eq;
using testing::StrictMock;

namespace doodle {

namespace {

class FakeDoodleFetcher : public DoodleFetcher {
 public:
  FakeDoodleFetcher() = default;
  ~FakeDoodleFetcher() override = default;

  void FetchDoodle(FinishedCallback callback) override {
    callbacks_.push_back(std::move(callback));
  }

  size_t num_pending_callbacks() const { return callbacks_.size(); }

  void ServeAllCallbacks(DoodleState state,
                         const base::Optional<DoodleConfig>& config) {
    for (auto& callback : callbacks_) {
      std::move(callback).Run(state, config);
    }
    callbacks_.clear();
  }

 private:
  std::vector<FinishedCallback> callbacks_;
};

class MockDoodleObserver : public DoodleService::Observer {
 public:
  MOCK_METHOD1(OnDoodleConfigUpdated,
               void(const base::Optional<DoodleConfig>&));
};

}  // namespace

// Equality operator for DoodleConfigs, for use by testing::Eq.
// Note: This must be outside of the anonymous namespace.
bool operator==(const DoodleConfig& lhs, const DoodleConfig& rhs) {
  return lhs.IsEquivalent(rhs);
}

class DoodleServiceTest : public testing::Test {
 public:
  DoodleServiceTest() : fetcher_(nullptr) {
    auto fetcher = base::MakeUnique<FakeDoodleFetcher>();
    fetcher_ = fetcher.get();
    service_ = base::MakeUnique<DoodleService>(std::move(fetcher));
  }

  DoodleService* service() { return service_.get(); }
  FakeDoodleFetcher* fetcher() { return fetcher_; }

 private:
  std::unique_ptr<DoodleService> service_;
  FakeDoodleFetcher* fetcher_;
};

TEST_F(DoodleServiceTest, FetchesConfigOnRefresh) {
  ASSERT_THAT(service()->config(), Eq(base::nullopt));

  // Request a refresh of the doodle config.
  service()->Refresh();
  // The request should have arrived at the fetcher.
  EXPECT_THAT(fetcher()->num_pending_callbacks(), Eq(1u));

  // Serve it (with an arbitrary config).
  DoodleConfig config;
  config.doodle_type = DoodleType::SIMPLE;
  fetcher()->ServeAllCallbacks(DoodleState::AVAILABLE, config);

  // The config should be available.
  EXPECT_THAT(service()->config(), Eq(config));

  // Request a refresh again.
  service()->Refresh();
  // The request should have arrived at the fetcher again.
  EXPECT_THAT(fetcher()->num_pending_callbacks(), Eq(1u));

  // Serve it with a different config.
  DoodleConfig other_config;
  other_config.doodle_type = DoodleType::SLIDESHOW;
  DCHECK(!config.IsEquivalent(other_config));
  fetcher()->ServeAllCallbacks(DoodleState::AVAILABLE, other_config);

  // The config should have been updated.
  EXPECT_THAT(service()->config(), Eq(other_config));
}

TEST_F(DoodleServiceTest, CallsObserverOnConfigReceived) {
  StrictMock<MockDoodleObserver> observer;
  service()->AddObserver(&observer);

  ASSERT_THAT(service()->config(), Eq(base::nullopt));

  // Request a refresh of the doodle config.
  service()->Refresh();
  // The request should have arrived at the fetcher.
  ASSERT_THAT(fetcher()->num_pending_callbacks(), Eq(1u));

  // Serve it (with an arbitrary config). The observer should get notified.
  DoodleConfig config;
  config.doodle_type = DoodleType::SIMPLE;
  EXPECT_CALL(observer, OnDoodleConfigUpdated(Eq(config)));
  fetcher()->ServeAllCallbacks(DoodleState::AVAILABLE, config);

  // Remove the observer before the service gets destroyed.
  service()->RemoveObserver(&observer);
}

TEST_F(DoodleServiceTest, CallsObserverOnConfigRemoved) {
  // Load some doodle config.
  service()->Refresh();
  DoodleConfig config;
  config.doodle_type = DoodleType::SIMPLE;
  fetcher()->ServeAllCallbacks(DoodleState::AVAILABLE, config);
  ASSERT_THAT(service()->config(), Eq(config));

  // Register an observer and request a refresh.
  StrictMock<MockDoodleObserver> observer;
  service()->AddObserver(&observer);
  service()->Refresh();
  ASSERT_THAT(fetcher()->num_pending_callbacks(), Eq(1u));

  // Serve the request with an empty doodle config. The observer should get
  // notified.
  EXPECT_CALL(observer, OnDoodleConfigUpdated(Eq(base::nullopt)));
  fetcher()->ServeAllCallbacks(DoodleState::NO_DOODLE, base::nullopt);

  // Remove the observer before the service gets destroyed.
  service()->RemoveObserver(&observer);
}

TEST_F(DoodleServiceTest, CallsObserverOnConfigUpdated) {
  // Load some doodle config.
  service()->Refresh();
  DoodleConfig config;
  config.doodle_type = DoodleType::SIMPLE;
  fetcher()->ServeAllCallbacks(DoodleState::AVAILABLE, config);
  ASSERT_THAT(service()->config(), Eq(config));

  // Register an observer and request a refresh.
  StrictMock<MockDoodleObserver> observer;
  service()->AddObserver(&observer);
  service()->Refresh();
  ASSERT_THAT(fetcher()->num_pending_callbacks(), Eq(1u));

  // Serve the request with a different doodle config. The observer should get
  // notified.
  DoodleConfig other_config;
  other_config.doodle_type = DoodleType::SLIDESHOW;
  DCHECK(!config.IsEquivalent(other_config));
  EXPECT_CALL(observer, OnDoodleConfigUpdated(Eq(other_config)));
  fetcher()->ServeAllCallbacks(DoodleState::AVAILABLE, other_config);

  // Remove the observer before the service gets destroyed.
  service()->RemoveObserver(&observer);
}

TEST_F(DoodleServiceTest, DoesNotCallObserverWhenConfigEquivalent) {
  // Load some doodle config.
  service()->Refresh();
  DoodleConfig config;
  config.doodle_type = DoodleType::SIMPLE;
  fetcher()->ServeAllCallbacks(DoodleState::AVAILABLE, config);
  ASSERT_THAT(service()->config(), Eq(config));

  // Register an observer and request a refresh.
  StrictMock<MockDoodleObserver> observer;
  service()->AddObserver(&observer);
  service()->Refresh();
  ASSERT_THAT(fetcher()->num_pending_callbacks(), Eq(1u));

  // Serve the request with an equivalent doodle config. The observer should
  // *not* get notified.
  DoodleConfig equivalent_config;
  equivalent_config.doodle_type = DoodleType::SIMPLE;
  DCHECK(config.IsEquivalent(equivalent_config));
  fetcher()->ServeAllCallbacks(DoodleState::AVAILABLE, equivalent_config);

  // Remove the observer before the service gets destroyed.
  service()->RemoveObserver(&observer);
}

}  // namespace doodle

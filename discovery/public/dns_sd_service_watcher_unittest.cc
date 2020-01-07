// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/public/dns_sd_service_watcher.h"

#include <algorithm>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::_;
using testing::ContainerEq;
using testing::IsSubsetOf;
using testing::IsSupersetOf;
using testing::StrictMock;

namespace openscreen {
namespace discovery {
namespace {

std::vector<std::string> ConvertRefs(
    const std::vector<std::reference_wrapper<const std::string>>& value) {
  std::vector<std::string> strings;

  // This loop is required to unwrap reference_wrapper objects.
  for (const std::string& val : value) {
    strings.push_back(val);
  }
  return strings;
}

static const IPAddress kAddressV4(192, 168, 0, 0);
static const IPEndpoint kEndpointV4{kAddressV4, 0};
static const std::string kCastServiceId = "_googlecast._tcp";
static const std::string kCastDomainId = "local";

class MockDnsSdService : public DnsSdService {
 public:
  MockDnsSdService() : querier_(this) {}

  DnsSdQuerier* GetQuerier() override { return &querier_; }
  DnsSdPublisher* GetPublisher() override { return nullptr; }

  MOCK_METHOD2(StartQuery,
               void(const std::string& service, DnsSdQuerier::Callback* cb));
  MOCK_METHOD2(StopQuery,
               void(const std::string& service, DnsSdQuerier::Callback* cb));
  MOCK_METHOD1(ReinitializeQueries, void(const std::string& service));

 private:
  class MockQuerier : public DnsSdQuerier {
   public:
    explicit MockQuerier(MockDnsSdService* service) : mock_service_(service) {
      OSP_DCHECK(service);
    }

    void StartQuery(const std::string& service,
                    DnsSdQuerier::Callback* cb) override {
      mock_service_->StartQuery(service, cb);
    }

    void StopQuery(const std::string& service,
                   DnsSdQuerier::Callback* cb) override {
      mock_service_->StopQuery(service, cb);
    }

    void ReinitializeQueries(const std::string& service) override {
      mock_service_->ReinitializeQueries(service);
    }

   private:
    MockDnsSdService* const mock_service_;
  };

  MockQuerier querier_;
};

}  // namespace

class TestServiceWatcher : public DnsSdServiceWatcher<std::string> {
 public:
  using DnsSdServiceWatcher<std::string>::ConstRefT;

  explicit TestServiceWatcher(MockDnsSdService* service)
      : DnsSdServiceWatcher<std::string>(
            service,
            kCastServiceId,
            [this](const DnsSdInstanceRecord& record) {
              return Convert(record);
            },
            [this](std::vector<ConstRefT> ref) { Callback(std::move(ref)); }) {}

  MOCK_METHOD1(Callback, void(std::vector<ConstRefT>));

  using DnsSdServiceWatcher<std::string>::OnInstanceCreated;
  using DnsSdServiceWatcher<std::string>::OnInstanceUpdated;
  using DnsSdServiceWatcher<std::string>::OnInstanceDeleted;

 private:
  std::string Convert(const DnsSdInstanceRecord& record) {
    return record.instance_id();
  }
};

class DnsSdServiceWatcherTests : public testing::Test {
 public:
  DnsSdServiceWatcherTests() : watcher_(&service_) {
    // Start service discovery, since all other tests need it
    EXPECT_FALSE(watcher_.is_running());
    EXPECT_CALL(service_, StartQuery(kCastServiceId, _));
    watcher_.StartDiscovery();
    testing::Mock::VerifyAndClearExpectations(&service_);
  }

 protected:
  void CreateNewInstance(const DnsSdInstanceRecord& record) {
    const std::vector<std::string> services_before =
        ConvertRefs(watcher_.GetServices());
    const size_t count = services_before.size();

    std::vector<std::string> callbacked_services;
    EXPECT_CALL(watcher_, Callback(_))
        .WillOnce([services = &callbacked_services](
                      std::vector<TestServiceWatcher::ConstRefT> value) {
          *services = ConvertRefs(value);
        });
    watcher_.OnInstanceCreated(record);
    testing::Mock::VerifyAndClearExpectations(&watcher_);

    std::vector<std::string> fetched_services =
        ConvertRefs(watcher_.GetServices());
    EXPECT_EQ(fetched_services.size(), count + 1);

    EXPECT_THAT(fetched_services, ContainerEq(callbacked_services));
    EXPECT_THAT(fetched_services, IsSupersetOf(services_before));
  }

  void CreateExistingInstance(const DnsSdInstanceRecord& record) {
    const std::vector<std::string> services_before =
        ConvertRefs(watcher_.GetServices());
    const size_t count = services_before.size();

    std::vector<std::string> callbacked_services;
    EXPECT_CALL(watcher_, Callback(_))
        .WillOnce([services = &callbacked_services](
                      std::vector<TestServiceWatcher::ConstRefT> value) {
          *services = ConvertRefs(value);
        });
    watcher_.OnInstanceCreated(record);
    testing::Mock::VerifyAndClearExpectations(&watcher_);

    const std::vector<std::string> fetched_services =
        ConvertRefs(watcher_.GetServices());
    EXPECT_EQ(fetched_services.size(), count);

    EXPECT_THAT(fetched_services, ContainerEq(callbacked_services));
    EXPECT_THAT(fetched_services, ContainerEq(services_before));
  }

  void UpdateExistingInstance(const DnsSdInstanceRecord& record) {
    const std::vector<std::string> services_before =
        ConvertRefs(watcher_.GetServices());
    const size_t count = services_before.size();

    std::vector<std::string> callbacked_services;
    EXPECT_CALL(watcher_, Callback(_))
        .WillOnce([services = &callbacked_services](
                      std::vector<TestServiceWatcher::ConstRefT> value) {
          *services = ConvertRefs(value);
        });
    watcher_.OnInstanceUpdated(record);
    testing::Mock::VerifyAndClearExpectations(&watcher_);

    const std::vector<std::string> fetched_services =
        ConvertRefs(watcher_.GetServices());
    EXPECT_EQ(fetched_services.size(), count);

    EXPECT_THAT(fetched_services, ContainerEq(callbacked_services));
    EXPECT_THAT(fetched_services, ContainerEq(services_before));
  }

  void DeleteExistingInstance(const DnsSdInstanceRecord& record) {
    const std::vector<std::string> services_before =
        ConvertRefs(watcher_.GetServices());
    const size_t count = services_before.size();

    std::vector<std::string> callbacked_services;
    EXPECT_CALL(watcher_, Callback(_))
        .WillOnce([services = &callbacked_services](
                      std::vector<TestServiceWatcher::ConstRefT> value) {
          *services = ConvertRefs(value);
        });
    watcher_.OnInstanceDeleted(record);
    testing::Mock::VerifyAndClearExpectations(&watcher_);

    const std::vector<std::string> fetched_services =
        ConvertRefs(watcher_.GetServices());
    EXPECT_EQ(fetched_services.size(), count - 1);
  }

  void UpdateNonExistingInstance(const DnsSdInstanceRecord& record) {
    const std::vector<std::string> services_before =
        ConvertRefs(watcher_.GetServices());
    const size_t count = services_before.size();

    EXPECT_CALL(watcher_, Callback(_)).Times(0);
    watcher_.OnInstanceUpdated(record);
    testing::Mock::VerifyAndClearExpectations(&watcher_);

    const std::vector<std::string> fetched_services =
        ConvertRefs(watcher_.GetServices());
    EXPECT_EQ(fetched_services.size(), count);

    EXPECT_THAT(services_before, ContainerEq(fetched_services));
  }

  void DeleteNonExistingInstance(const DnsSdInstanceRecord& record) {
    const std::vector<std::string> services_before =
        ConvertRefs(watcher_.GetServices());
    const size_t count = services_before.size();

    EXPECT_CALL(watcher_, Callback(_)).Times(0);
    watcher_.OnInstanceDeleted(record);
    testing::Mock::VerifyAndClearExpectations(&watcher_);

    const std::vector<std::string> fetched_services =
        ConvertRefs(watcher_.GetServices());
    EXPECT_EQ(fetched_services.size(), count);

    EXPECT_THAT(services_before, ContainerEq(fetched_services));
  }

  bool ContainsService(const DnsSdInstanceRecord& record) {
    const std::string& service = record.instance_id();
    const std::vector<TestServiceWatcher::ConstRefT> services =
        watcher_.GetServices();
    return std::find_if(services.begin(), services.end(),
                        [&service](const std::string& ref) {
                          return service == ref;
                        }) != services.end();
  }

  StrictMock<MockDnsSdService> service_;
  StrictMock<TestServiceWatcher> watcher_;
  std::vector<std::string> fetched_services;
};

TEST_F(DnsSdServiceWatcherTests, StartStopDiscoveryWorks) {
  EXPECT_TRUE(watcher_.is_running());
  EXPECT_CALL(service_, StopQuery(kCastServiceId, _));
  watcher_.StopDiscovery();
  EXPECT_FALSE(watcher_.is_running());
}

TEST(DnsSdServiceWatcherTest, RefreshFailsBeforeDiscoveryStarts) {
  StrictMock<MockDnsSdService> service;
  StrictMock<TestServiceWatcher> watcher(&service);
  EXPECT_FALSE(watcher.DiscoverNow().ok());
  EXPECT_FALSE(watcher.ForceRefresh().ok());
}

TEST_F(DnsSdServiceWatcherTests, RefreshDiscoveryWorks) {
  const DnsSdInstanceRecord record("Instance", kCastServiceId, kCastDomainId,
                                   kEndpointV4, DnsSdTxtRecord{});
  CreateNewInstance(record);

  // Refresh services.
  EXPECT_CALL(service_, ReinitializeQueries(kCastServiceId));
  EXPECT_TRUE(watcher_.DiscoverNow().ok());
  EXPECT_EQ(watcher_.GetServices().size(), size_t{1});
  testing::Mock::VerifyAndClearExpectations(&service_);

  EXPECT_CALL(service_, ReinitializeQueries(kCastServiceId));
  EXPECT_TRUE(watcher_.ForceRefresh().ok());
  EXPECT_EQ(watcher_.GetServices().size(), size_t{0});
  testing::Mock::VerifyAndClearExpectations(&service_);
}

TEST_F(DnsSdServiceWatcherTests, CreatingUpdatingDeletingInstancesWork) {
  const DnsSdInstanceRecord record("Instance", kCastServiceId, kCastDomainId,
                                   kEndpointV4, DnsSdTxtRecord{});
  const DnsSdInstanceRecord record2("Instance2", kCastServiceId, kCastDomainId,
                                    kEndpointV4, DnsSdTxtRecord{});

  EXPECT_FALSE(ContainsService(record));
  EXPECT_FALSE(ContainsService(record2));

  CreateNewInstance(record);
  EXPECT_TRUE(ContainsService(record));
  EXPECT_FALSE(ContainsService(record2));

  CreateExistingInstance(record);
  EXPECT_TRUE(ContainsService(record));
  EXPECT_FALSE(ContainsService(record2));

  UpdateNonExistingInstance(record2);
  EXPECT_TRUE(ContainsService(record));
  EXPECT_FALSE(ContainsService(record2));

  DeleteNonExistingInstance(record2);
  EXPECT_TRUE(ContainsService(record));
  EXPECT_FALSE(ContainsService(record2));

  CreateNewInstance(record2);
  EXPECT_TRUE(ContainsService(record));
  EXPECT_TRUE(ContainsService(record2));

  UpdateExistingInstance(record2);
  EXPECT_TRUE(ContainsService(record));
  EXPECT_TRUE(ContainsService(record2));

  UpdateExistingInstance(record);
  EXPECT_TRUE(ContainsService(record));
  EXPECT_TRUE(ContainsService(record2));

  DeleteExistingInstance(record);
  EXPECT_FALSE(ContainsService(record));
  EXPECT_TRUE(ContainsService(record2));

  UpdateNonExistingInstance(record);
  EXPECT_FALSE(ContainsService(record));
  EXPECT_TRUE(ContainsService(record2));

  DeleteNonExistingInstance(record);
  EXPECT_FALSE(ContainsService(record));
  EXPECT_TRUE(ContainsService(record2));

  DeleteExistingInstance(record2);
  EXPECT_FALSE(ContainsService(record));
  EXPECT_FALSE(ContainsService(record2));
}

}  // namespace discovery
}  // namespace openscreen

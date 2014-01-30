// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "chrome/browser/local_discovery/privet_device_lister_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::SaveArg;
using testing::_;

namespace local_discovery {

namespace {

class MockServiceResolver;
class MockServiceWatcher;

class ServiceDiscoveryMockDelegate {
 public:
  virtual void ServiceWatcherStarted(const std::string& service_type,
                                     MockServiceWatcher* watcher) = 0;
  virtual void ServiceResolverStarted(const std::string& service_type,
                                      MockServiceResolver* resolver) = 0;
};

class MockServiceWatcher : public ServiceWatcher {
 public:
  MockServiceWatcher(const std::string& service_type,
                     const ServiceWatcher::UpdatedCallback& callback,
                     ServiceDiscoveryMockDelegate* mock_delegate)
      : started_(false), service_type_(service_type),  callback_(callback),
        mock_delegate_(mock_delegate) {
  }

  virtual ~MockServiceWatcher() {
  }

  virtual void Start() {
    DCHECK(!started_);
    started_ = true;
    mock_delegate_->ServiceWatcherStarted(service_type_, this);
  }

  MOCK_METHOD1(DiscoverNewServices, void(bool force_update));

  MOCK_METHOD1(SetActivelyRefreshServices, void(
      bool actively_refresh_services));

  virtual std::string GetServiceType() const {
    return service_type_;
  }

  bool started() {
    return started_;
  }

  ServiceWatcher::UpdatedCallback callback() {
    return callback_;
  }

 private:
  bool started_;
  std::string service_type_;
  ServiceWatcher::UpdatedCallback callback_;
  ServiceDiscoveryMockDelegate* mock_delegate_;
};

class MockServiceResolver : public ServiceResolver {
 public:
  MockServiceResolver(const std::string& service_name,
                      const ServiceResolver::ResolveCompleteCallback& callback,
                      ServiceDiscoveryMockDelegate* mock_delegate)
      : started_resolving_(false), service_name_(service_name),
        callback_(callback), mock_delegate_(mock_delegate) {
  }

  virtual ~MockServiceResolver() {
  }

  virtual void StartResolving() OVERRIDE {
    started_resolving_ = true;
    mock_delegate_->ServiceResolverStarted(service_name_, this);
  }

  bool IsResolving() const {
    return started_resolving_;
  }

  virtual std::string GetName() const OVERRIDE {
    return service_name_;
  }

  const ServiceResolver::ResolveCompleteCallback& callback() {
    return callback_; }

 private:
  bool started_resolving_;
  std::string service_name_;
  ServiceResolver::ResolveCompleteCallback callback_;
  ServiceDiscoveryMockDelegate* mock_delegate_;
};

class MockServiceDiscoveryClient : public ServiceDiscoveryClient {
 public:
  explicit MockServiceDiscoveryClient(
      ServiceDiscoveryMockDelegate* mock_delegate)
      : mock_delegate_(mock_delegate) {
  }

  virtual ~MockServiceDiscoveryClient() {
  }

  // Create a service watcher object listening for DNS-SD service announcements
  // on service type |service_type|.
  virtual scoped_ptr<ServiceWatcher> CreateServiceWatcher(
      const std::string& service_type,
      const ServiceWatcher::UpdatedCallback& callback) OVERRIDE {
    scoped_ptr<MockServiceWatcher> mock_service_watcher(
        new MockServiceWatcher(service_type, callback, mock_delegate_));
    return mock_service_watcher.PassAs<ServiceWatcher>();
  }

  // Create a service resolver object for getting detailed service information
  // for the service called |service_name|.
  virtual scoped_ptr<ServiceResolver> CreateServiceResolver(
      const std::string& service_name,
      const ServiceResolver::ResolveCompleteCallback& callback) OVERRIDE {
    scoped_ptr<MockServiceResolver> mock_service_resolver(
        new MockServiceResolver(service_name, callback, mock_delegate_));
    return mock_service_resolver.PassAs<ServiceResolver>();
  }

  // Not used in this test.
  virtual scoped_ptr<LocalDomainResolver> CreateLocalDomainResolver(
    const std::string& domain,
    net::AddressFamily address_family,
    const LocalDomainResolver::IPAddressCallback& callback) OVERRIDE {
    NOTREACHED();
    return scoped_ptr<LocalDomainResolver>();
  }

 private:
  ServiceDiscoveryMockDelegate* mock_delegate_;
};

class MockServiceDiscoveryMockDelegate : public ServiceDiscoveryMockDelegate {
 public:
  MOCK_METHOD2(ServiceWatcherStarted, void(const std::string& service_type,
                                           MockServiceWatcher* watcher));
  MOCK_METHOD2(ServiceResolverStarted, void(const std::string& service_type,
                                            MockServiceResolver* resolver));
};

class MockDeviceListerDelegate : public PrivetDeviceLister::Delegate {
 public:
  MockDeviceListerDelegate() {
  }

  virtual ~MockDeviceListerDelegate() {
  }

  MOCK_METHOD3(DeviceChanged, void(bool added,
                                   const std::string& name,
                                   const DeviceDescription& description));

  MOCK_METHOD1(DeviceRemoved, void(const std::string& name));

  MOCK_METHOD0(DeviceCacheFlushed, void());
};

class PrivetDeviceListerTest : public testing::Test {
 public:
  PrivetDeviceListerTest() : mock_client_(&mock_delegate_) {
  }

  virtual ~PrivetDeviceListerTest() {
  }

  virtual void SetUp() OVERRIDE {
    example_attrs_.push_back("tXtvers=1");
    example_attrs_.push_back("ty=My Printer");
    example_attrs_.push_back("nOte=This is my Printer");
    example_attrs_.push_back("CS=ONlInE");
    example_attrs_.push_back("id=");

    service_description_.service_name = "myprinter._privet._tcp.local";
    service_description_.address = net::HostPortPair("myprinter.local", 6006);
    service_description_.metadata = example_attrs_;
    service_description_.last_seen = base::Time() +
        base::TimeDelta::FromSeconds(5);
    service_description_.ip_address.push_back(1);
    service_description_.ip_address.push_back(2);
    service_description_.ip_address.push_back(3);
    service_description_.ip_address.push_back(4);
  }

 protected:
  testing::StrictMock<MockServiceDiscoveryMockDelegate> mock_delegate_;
  MockServiceDiscoveryClient mock_client_;
  MockDeviceListerDelegate delegate_;
  std::vector<std::string> example_attrs_;
  ServiceDescription service_description_;
};

TEST_F(PrivetDeviceListerTest, SimpleUpdateTest) {
  DeviceDescription outgoing_description;

  MockServiceWatcher* service_watcher;
  MockServiceResolver* service_resolver;

  EXPECT_CALL(mock_delegate_,
              ServiceWatcherStarted("_privet._tcp.local", _))
      .WillOnce(SaveArg<1>(&service_watcher));
  PrivetDeviceListerImpl privet_lister(&mock_client_, &delegate_);
  privet_lister.Start();
  testing::Mock::VerifyAndClear(&mock_delegate_);

  EXPECT_CALL(mock_delegate_,
              ServiceResolverStarted("myprinter._privet._tcp.local", _))
      .WillOnce(SaveArg<1>(&service_resolver));
  service_watcher->callback().Run(ServiceWatcher::UPDATE_ADDED,
                                  "myprinter._privet._tcp.local");
  testing::Mock::VerifyAndClear(&mock_delegate_);

  EXPECT_CALL(delegate_, DeviceChanged(true,
                                       "myprinter._privet._tcp.local",
                                       _))
              .WillOnce(SaveArg<2>(&outgoing_description));

  service_resolver->callback().Run(ServiceResolver::STATUS_SUCCESS,
                                   service_description_);

  EXPECT_EQ(service_description_.address.host(),
            outgoing_description.address.host());
  EXPECT_EQ(service_description_.address.port(),
            outgoing_description.address.port());
  EXPECT_EQ(service_description_.ip_address, outgoing_description.ip_address);
  EXPECT_EQ(service_description_.last_seen, outgoing_description.last_seen);
  EXPECT_EQ("My Printer", outgoing_description.name);
  EXPECT_EQ("This is my Printer", outgoing_description.description);
  EXPECT_EQ("", outgoing_description.id);
  EXPECT_EQ(DeviceDescription::ONLINE, outgoing_description.connection_state);

  EXPECT_CALL(delegate_, DeviceRemoved("myprinter._privet._tcp.local"));

  service_watcher->callback().Run(ServiceWatcher::UPDATE_REMOVED,
                                  "myprinter._privet._tcp.local");
}

TEST_F(PrivetDeviceListerTest, MultipleUpdatesPostResolve) {
  MockServiceWatcher* service_watcher;
  MockServiceResolver* service_resolver;

  EXPECT_CALL(mock_delegate_,
              ServiceWatcherStarted("_privet._tcp.local", _))
      .WillOnce(SaveArg<1>(&service_watcher));
  PrivetDeviceListerImpl privet_lister(&mock_client_, &delegate_);
  privet_lister.Start();
  testing::Mock::VerifyAndClear(&mock_delegate_);

  EXPECT_CALL(mock_delegate_,
              ServiceResolverStarted("myprinter._privet._tcp.local", _))
      .WillOnce(SaveArg<1>(&service_resolver));

  service_watcher->callback().Run(ServiceWatcher::UPDATE_CHANGED,
                                  "myprinter._privet._tcp.local");
  testing::Mock::VerifyAndClear(&mock_delegate_);

  EXPECT_CALL(delegate_, DeviceChanged(false,
                                       "myprinter._privet._tcp.local",
                                       _));
  service_resolver->callback().Run(ServiceResolver::STATUS_SUCCESS,
                                   service_description_);

  EXPECT_CALL(mock_delegate_,
              ServiceResolverStarted("myprinter._privet._tcp.local", _));
  service_watcher->callback().Run(ServiceWatcher::UPDATE_CHANGED,
                                  "myprinter._privet._tcp.local");
  testing::Mock::VerifyAndClear(&mock_delegate_);
}

// Check that the device lister does not create a still-working resolver
TEST_F(PrivetDeviceListerTest, MultipleUpdatesPreResolve) {
  MockServiceWatcher* service_watcher;

  EXPECT_CALL(mock_delegate_,
              ServiceWatcherStarted("_privet._tcp.local", _))
      .WillOnce(SaveArg<1>(&service_watcher));
  PrivetDeviceListerImpl privet_lister(&mock_client_, &delegate_);
  privet_lister.Start();
  testing::Mock::VerifyAndClear(&mock_delegate_);

  EXPECT_CALL(mock_delegate_,
              ServiceResolverStarted("myprinter._privet._tcp.local", _))
      .Times(1);
  service_watcher->callback().Run(ServiceWatcher::UPDATE_CHANGED,
                                  "myprinter._privet._tcp.local");
  service_watcher->callback().Run(ServiceWatcher::UPDATE_CHANGED,
                                  "myprinter._privet._tcp.local");
}

TEST_F(PrivetDeviceListerTest, DiscoverNewDevices) {
  MockServiceWatcher* service_watcher;

  EXPECT_CALL(mock_delegate_,
              ServiceWatcherStarted("_privet._tcp.local", _))
      .WillOnce(SaveArg<1>(&service_watcher));
  PrivetDeviceListerImpl privet_lister(&mock_client_, &delegate_);
  privet_lister.Start();
  testing::Mock::VerifyAndClear(&mock_delegate_);

  EXPECT_CALL(*service_watcher, DiscoverNewServices(false));
  privet_lister.DiscoverNewDevices(false);
}


}  // namespace

}  // namespace local_discovery

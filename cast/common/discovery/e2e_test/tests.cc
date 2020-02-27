// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atomic>
#include <functional>
#include <map>

#include "cast/common/public/service_info.h"
#include "discovery/common/config.h"
#include "discovery/common/reporting_client.h"
#include "discovery/dnssd/public/dns_sd_publisher.h"
#include "discovery/dnssd/public/dns_sd_service.h"
#include "discovery/public/dns_sd_service_watcher.h"
#include "gtest/gtest.h"
#include "platform/api/logging.h"
#include "platform/api/udp_socket.h"
#include "platform/base/interface_info.h"
#include "platform/impl/logging.h"
#include "platform/impl/network_interface.h"
#include "platform/impl/platform_client_posix.h"
#include "platform/impl/task_runner.h"

namespace openscreen {
namespace cast {
namespace {

// Total wait time = 4 seconds.
constexpr std::chrono::milliseconds kWaitLoopSleepTime =
    std::chrono::milliseconds(500);
constexpr int kMaxWaitLoopIterations = 8;

// Total wait time = 2.5 seconds.
// NOTE: This must be less than the above wait time.
constexpr std::chrono::milliseconds kCheckLoopSleepTime =
    std::chrono::milliseconds(100);
constexpr int kMaxCheckLoopIterations = 25;

}  // namespace

// Publishes new service instances.
class Publisher : public discovery::DnsSdPublisher::Client {
 public:
  Publisher(discovery::DnsSdService* service)
      : dnssd_publisher_(service->GetPublisher()) {
    OSP_LOG << "Initializing Publisher...\n";
  }

  ~Publisher() override = default;

  Error Register(const ServiceInfo& info) {
    if (!info.IsValid()) {
      OSP_LOG << "Invalid record provided to Register\n";
      return Error::Code::kParameterInvalid;
    }

    return dnssd_publisher_->Register(ServiceInfoToDnsSdRecord(info), this);
  }

  Error UpdateRegistration(const ServiceInfo& info) {
    if (!info.IsValid()) {
      return Error::Code::kParameterInvalid;
    }

    return dnssd_publisher_->UpdateRegistration(ServiceInfoToDnsSdRecord(info));
  }

  int DeregisterAll() {
    return dnssd_publisher_->DeregisterAll(kCastV2ServiceId);
  }

  absl::optional<std::string> GetClaimedInstanceId(
      const std::string& requested_id) {
    auto it = instance_ids_.find(requested_id);
    if (it == instance_ids_.end()) {
      return absl::nullopt;
    } else {
      return it->second;
    }
  }

 private:
  // DnsSdPublisher::Client overrides.
  void OnInstanceClaimed(
      const discovery::DnsSdInstanceRecord& requested_record,
      const discovery::DnsSdInstanceRecord& claimed_record) override {
    instance_ids_.emplace(requested_record.instance_id(),
                          claimed_record.instance_id());
  }

  discovery::DnsSdPublisher* const dnssd_publisher_;
  std::map<std::string, std::string> instance_ids_;
};

// Receives incoming services and outputs their results to stdout.
class Receiver : public discovery::DnsSdServiceWatcher<ServiceInfo> {
 public:
  Receiver(discovery::DnsSdService* service)
      : discovery::DnsSdServiceWatcher<ServiceInfo>(
            service,
            kCastV2ServiceId,
            DnsSdRecordToServiceInfo,
            [this](
                std::vector<std::reference_wrapper<const ServiceInfo>> infos) {
              ProcessResults(std::move(infos));
            }) {
    OSP_LOG << "Initializing Receiver...";
  }

  bool IsServiceFound(const ServiceInfo& check_service) {
    return std::find_if(service_infos_.begin(), service_infos_.end(),
                        [&check_service](const ServiceInfo& info) {
                          return info.friendly_name ==
                                 check_service.friendly_name;
                        }) != service_infos_.end();
  }

 private:
  void ProcessResults(
      std::vector<std::reference_wrapper<const ServiceInfo>> infos) {
    service_infos_ = std::vector<ServiceInfo>();
    for (const ServiceInfo& info : infos) {
      service_infos_.push_back(info);
      OSP_VLOG << "\tFound instance name: " << info.friendly_name;
    }
    OSP_VLOG << "\t--------\n";
  }

  std::vector<ServiceInfo> service_infos_;
};

class FailOnErrorReporting : public discovery::ReportingClient {
  void OnFatalError(Error error) override {
    // TODO(rwkeane): Change this to OSP_NOTREACHED() pending resolution of
    // socket initialization issue.
    OSP_LOG << "Fatal error received: '" << error << "'";
  }

  void OnRecoverableError(Error error) override {
    // Pending resolution of openscreen:105, logging recoverable errors is
    // disabled, as this will end up polluting the output with logs related to
    // mDNS messages received from non-loopback network interfaces over which
    // we have no control.
  }
};

discovery::Config GetConfigSettings() {
  discovery::Config config;

  // Get the loopback interface to run on.
  absl::optional<InterfaceInfo> loopback = GetLoopbackInterfaceForTesting();
  OSP_DCHECK(loopback.has_value());
  config.interface = loopback.value();

  return config;
}

class DiscoveryE2ETest : public testing::Test {
 public:
  DiscoveryE2ETest() {
    // Sleep to let any packets clear off the network before further tests.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Set log level so info logs go to stdout.
    SetLogLevel(LogLevel::kInfo);

    PlatformClientPosix::Create(Clock::duration{50}, Clock::duration{50});
    task_runner_ = PlatformClientPosix::GetInstance()->GetTaskRunner();
  }

  ~DiscoveryE2ETest() {
    OSP_LOG << "TEST COMPLETE!";
    dnssd_service_.reset();
    PlatformClientPosix::ShutDown();
  }

 protected:
  ServiceInfo GetInfoV4() {
    ServiceInfo hosted_service;
    hosted_service.v4_endpoint = IPEndpoint{{10, 0, 0, 1}, 25252};
    hosted_service.unique_id = "id1";
    hosted_service.model_name = "openscreen-ModelV4";
    hosted_service.friendly_name = "DemoV4!";
    return hosted_service;
  }

  ServiceInfo GetInfoV6() {
    ServiceInfo hosted_service;
    hosted_service.v6_endpoint = IPEndpoint{{1, 2, 3, 4, 5, 6, 7, 8}, 25253};
    hosted_service.unique_id = "id2";
    hosted_service.model_name = "openscreen-ModelV6";
    hosted_service.friendly_name = "DemoV6!";
    return hosted_service;
  }

  ServiceInfo GetInfoV4V6() {
    ServiceInfo hosted_service;
    hosted_service.v4_endpoint = IPEndpoint{{10, 0, 0, 2}, 25254};
    hosted_service.v6_endpoint = IPEndpoint{{1, 2, 3, 4, 5, 6, 7, 9}, 25255};
    hosted_service.unique_id = "id3";
    hosted_service.model_name = "openscreen-ModelV4andV6";
    hosted_service.friendly_name = "DemoV4andV6!";
    return hosted_service;
  }

  void SetUpService(const discovery::Config& config) {
    OSP_DCHECK(!dnssd_service_.get());
    dnssd_service_ = discovery::DnsSdService::Create(
        task_runner_, &reporting_client_, config);
    receiver_ = std::make_unique<Receiver>(dnssd_service_.get());
    publisher_ = std::make_unique<Publisher>(dnssd_service_.get());
  }

  void StartDiscovery() {
    OSP_DCHECK(dnssd_service_.get());
    task_runner_->PostTask([this]() { receiver_->StartDiscovery(); });
  }

  template <typename... RecordTypes>
  void UpdateRecords(RecordTypes... records) {
    OSP_DCHECK(dnssd_service_.get());
    OSP_DCHECK(publisher_.get());

    std::vector<ServiceInfo> record_set{std::move(records)...};
    for (ServiceInfo& record : record_set) {
      task_runner_->PostTask([this, r = std::move(record)]() {
        auto error = publisher_->UpdateRegistration(r);
        OSP_DCHECK(error.ok()) << "\tFailed to update service instance '"
                               << r.friendly_name << "': " << error << "!";
      });
    }
  }

  template <typename... RecordTypes>
  void PublishRecords(RecordTypes... records) {
    OSP_DCHECK(dnssd_service_.get());
    OSP_DCHECK(publisher_.get());

    std::vector<ServiceInfo> record_set{std::move(records)...};
    for (ServiceInfo& record : record_set) {
      task_runner_->PostTask([this, r = std::move(record)]() {
        auto error = publisher_->Register(r);
        OSP_DCHECK(error.ok()) << "\tFailed to publish service instance '"
                               << r.friendly_name << "': " << error << "!";
      });
    }
  }

  template <typename... AtomicBoolPtrs>
  void WaitUntilSeen(bool should_be_seen, AtomicBoolPtrs... bools) {
    OSP_DCHECK(dnssd_service_.get());
    std::vector<std::atomic_bool*> atomic_bools{bools...};

    int waiting_on = atomic_bools.size();
    for (int i = 0; i < kMaxWaitLoopIterations; i++) {
      waiting_on = atomic_bools.size();
      for (std::atomic_bool* atomic : atomic_bools) {
        if (*atomic) {
          OSP_DCHECK(should_be_seen) << "Found service instance!";
          waiting_on--;
        }
      }

      if (waiting_on) {
        OSP_LOG << "\tWaiting on " << waiting_on << "...";
        std::this_thread::sleep_for(kWaitLoopSleepTime);
        continue;
      }
      return;
    }
    OSP_DCHECK(!should_be_seen)
        << "Could not find " << waiting_on << " service instances!";
  }

  void CheckForClaimedIds(ServiceInfo service_info,
                          std::atomic_bool* has_been_seen) {
    OSP_DCHECK(dnssd_service_.get());
    task_runner_->PostTask(
        [this, info = std::move(service_info), has_been_seen]() mutable {
          CheckForClaimedIds(std::move(info), has_been_seen, 0);
        });
  }

  void CheckForPublishedService(ServiceInfo service_info,
                                std::atomic_bool* has_been_seen) {
    OSP_DCHECK(dnssd_service_.get());
    task_runner_->PostTask(
        [this, info = std::move(service_info), has_been_seen]() mutable {
          CheckForPublishedService(std::move(info), has_been_seen, 0, true);
        });
  }

  void CheckNotPublishedService(ServiceInfo service_info,
                                std::atomic_bool* has_been_seen) {
    OSP_DCHECK(dnssd_service_.get());
    task_runner_->PostTask(
        [this, info = std::move(service_info), has_been_seen]() mutable {
          CheckForPublishedService(std::move(info), has_been_seen, 0, false);
        });
  }

  TaskRunner* task_runner_;
  FailOnErrorReporting reporting_client_;
  SerialDeletePtr<discovery::DnsSdService> dnssd_service_;
  std::unique_ptr<Receiver> receiver_;
  std::unique_ptr<Publisher> publisher_;

 private:
  void CheckForClaimedIds(ServiceInfo service_info,
                          std::atomic_bool* has_been_seen,
                          int attempts) {
    if (publisher_->GetClaimedInstanceId(service_info.GetInstanceId()) ==
        absl::nullopt) {
      if (attempts++ > kMaxCheckLoopIterations) {
        OSP_NOTREACHED() << "Service " << service_info.friendly_name
                         << " publication failed.";
      }
      task_runner_->PostTaskWithDelay(
          [this, info = std::move(service_info), has_been_seen,
           attempts]() mutable {
            CheckForClaimedIds(std::move(info), has_been_seen, attempts);
          },
          kCheckLoopSleepTime);
    } else {
      // TODO(crbug.com/openscreen/110): Log the published service instance.
      *has_been_seen = true;
    }
  }

  void CheckForPublishedService(ServiceInfo service_info,
                                std::atomic_bool* has_been_seen,
                                int attempts,
                                bool expect_to_be_present) {
    if (!receiver_->IsServiceFound(service_info)) {
      if (attempts++ > kMaxCheckLoopIterations) {
        OSP_DCHECK(!expect_to_be_present)
            << "Service " << service_info.friendly_name << " discovery failed.";
        return;
      }
      task_runner_->PostTaskWithDelay(
          [this, info = std::move(service_info), has_been_seen, attempts,
           expect_to_be_present]() mutable {
            CheckForPublishedService(std::move(info), has_been_seen, attempts,
                                     expect_to_be_present);
          },
          kCheckLoopSleepTime);
    } else if (expect_to_be_present) {
      // TODO(crbug.com/openscreen/110): Log the discovered service instance.
      *has_been_seen = true;
    } else {
      OSP_NOTREACHED() << "Found instance '" << service_info.friendly_name
                       << "'!";
    }
  }
};

// The below runs an E2E tests. These test requires no user interaction and is
// intended to perform a set series of actions to validate that discovery is
// functioning as intended.
//
// Known issues:
// - The ipv6 socket in discovery/mdns/service_impl.cc fails to bind to an ipv6
//   address on the loopback interface. Investigating this issue is pending
//   resolution of bug
//   https://bugs.chromium.org/p/openscreen/issues/detail?id=105.
//
// In this test, the following operations are performed:
// 1) Start up the Cast platform for a posix system.
// 2) Publish 3 CastV2 service instances to the loopback interface using mDNS,
//    with record announcement disabled.
// 3) Wait for the probing phase to successfully complete.
// 4) Query for records published over the loopback interface, and validate that
//    all 3 previously published services are discovered.
TEST_F(DiscoveryE2ETest, ValidateQueryFlow) {
  // Set up demo infra.
  auto discovery_config = GetConfigSettings();
  discovery_config.new_record_announcement_count = 0;
  SetUpService(discovery_config);

  auto v4 = GetInfoV4();
  auto v6 = GetInfoV6();
  auto multi_address = GetInfoV4V6();

  // Start discovery and publication.
  StartDiscovery();
  PublishRecords(v4, v6, multi_address);

  // Wait until all probe phases complete and all instance ids are claimed. At
  // this point, all records should be published.
  OSP_LOG << "Service publication in progress...";
  std::atomic_bool v4_found{false};
  std::atomic_bool v6_found{false};
  std::atomic_bool multi_address_found{false};
  CheckForClaimedIds(v4, &v4_found);
  CheckForClaimedIds(v6, &v6_found);
  CheckForClaimedIds(multi_address, &multi_address_found);
  WaitUntilSeen(true, &v4_found, &v6_found, &multi_address_found);
  OSP_LOG << "\tAll services successfully published!\n";

  // Make sure all services are found through discovery.
  OSP_LOG << "Service discovery in progress...";
  v4_found = false;
  v6_found = false;
  multi_address_found = false;
  CheckForPublishedService(v4, &v4_found);
  CheckForPublishedService(v6, &v6_found);
  CheckForPublishedService(multi_address, &multi_address_found);
  WaitUntilSeen(true, &v4_found, &v6_found, &multi_address_found);
  OSP_LOG << "\tAll services successfully discovered!\n";
}

// In this test, the following operations are performed:
// 1) Start up the Cast platform for a posix system.
// 2) Start service discovery and new queries, with no query messages being
//    sent.
// 3) Publish 3 CastV2 service instances to the loopback interface using mDNS,
//    with record announcement enabled.
// 4) Ensure the correct records were published over the loopback interface.
TEST_F(DiscoveryE2ETest, ValidateAnnouncementFlow) {
  // Set up demo infra.
  auto discovery_config = GetConfigSettings();
  discovery_config.should_announce_new_queries_ = false;
  SetUpService(discovery_config);

  auto v4 = GetInfoV4();
  auto v6 = GetInfoV6();
  auto multi_address = GetInfoV4V6();

  // Start discovery and publication.
  StartDiscovery();
  PublishRecords(v4, v6, multi_address);

  // Wait until all probe phases complete and all instance ids are claimed. At
  // this point, all records should be published.
  OSP_LOG << "Service publication in progress...";
  std::atomic_bool v4_found{false};
  std::atomic_bool v6_found{false};
  std::atomic_bool multi_address_found{false};
  CheckForClaimedIds(v4, &v4_found);
  CheckForClaimedIds(v6, &v6_found);
  CheckForClaimedIds(multi_address, &multi_address_found);
  WaitUntilSeen(true, &v4_found, &v6_found, &multi_address_found);
  OSP_LOG << "\tAll services successfully published and announced!\n";

  // Make sure all services are found through discovery.
  OSP_LOG << "Service discovery in progress...";
  v4_found = false;
  v6_found = false;
  multi_address_found = false;
  CheckForPublishedService(v4, &v4_found);
  CheckForPublishedService(v6, &v6_found);
  CheckForPublishedService(multi_address, &multi_address_found);
  WaitUntilSeen(true, &v4_found, &v6_found, &multi_address_found);
  OSP_LOG << "\tAll services successfully discovered!\n";
}

// In this test, the following operations are performed:
// 1) Start up the Cast platform for a posix system.
// 2) Publish one service and ensure it is NOT received.
// 3) Start service discovery and new queries.
// 4) Ensure above published service IS received.
// 5) Stop the started query.
// 6) Update a service, and ensure that no callback is received.
// 7) Restart the query and ensure that only the expected callbacks are
// received.
TEST_F(DiscoveryE2ETest, ValidateRecordsOnlyReceivedWhenQueryRunning) {
  // Set up demo infra.
  auto discovery_config = GetConfigSettings();
  discovery_config.new_record_announcement_count = 1;
  discovery_config.should_announce_new_queries_ = true;
  SetUpService(discovery_config);

  auto v4 = GetInfoV4();

  // Start discovery and publication.
  PublishRecords(v4);

  // Wait until all probe phases complete and all instance ids are claimed. At
  // this point, all records should be published.
  OSP_LOG << "Service publication in progress...";
  std::atomic_bool v4_found{false};
  CheckForClaimedIds(v4, &v4_found);
  WaitUntilSeen(true, &v4_found);

  // And ensure stopped discovery does not find the records.
  OSP_LOG << "Validating no service discovery occurs when discovery stopped...";
  v4_found = false;
  CheckNotPublishedService(v4, &v4_found);
  WaitUntilSeen(false, &v4_found);

  // Make sure all services are found through discovery.
  StartDiscovery();
  OSP_LOG << "Service discovery in progress...";
  v4_found = false;
  CheckForPublishedService(v4, &v4_found);
  WaitUntilSeen(true, &v4_found);

  // Update discovery and ensure that the updated service is seen.
  OSP_LOG << "Updating service and waiting for discovery...";
  auto updated_v4 = v4;
  updated_v4.friendly_name = "OtherName";
  v4_found = false;
  UpdateRecords(updated_v4);
  CheckForPublishedService(updated_v4, &v4_found);
  WaitUntilSeen(true, &v4_found);

  // And ensure the old service has been removed.
  v4_found = false;
  CheckNotPublishedService(v4, &v4_found);
  WaitUntilSeen(false, &v4_found);

  // Stop discovery.
  OSP_LOG << "Stopping discovery...";
  task_runner_->PostTask([this]() { receiver_->StopDiscovery(); });

  // Update discovery and ensure that the updated service is NOT seen.
  OSP_LOG << "Updating service and validating the change isn't received...";
  v4_found = false;
  v4.friendly_name = "ThirdName";
  UpdateRecords(v4);
  CheckNotPublishedService(v4, &v4_found);
  WaitUntilSeen(false, &v4_found);

  // Restart discovery and ensure that only the updated record is returned.
  StartDiscovery();
  OSP_LOG << "Service discovery in progress...";
  v4_found = false;
  CheckNotPublishedService(updated_v4, &v4_found);
  WaitUntilSeen(false, &v4_found);

  v4_found = false;
  CheckForPublishedService(v4, &v4_found);
  WaitUntilSeen(true, &v4_found);
}

}  // namespace cast
}  // namespace openscreen

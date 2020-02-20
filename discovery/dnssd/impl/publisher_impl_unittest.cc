// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/publisher_impl.h"

#include <utility>
#include <vector>

#include "discovery/common/testing/mock_reporting_client.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"

namespace openscreen {
namespace discovery {
namespace {

using testing::_;
using testing::Return;
using testing::StrictMock;

class MockClient : public DnsSdPublisher::Client {
 public:
  MOCK_METHOD2(OnInstanceClaimed,
               void(const DnsSdInstanceRecord&, const DnsSdInstanceRecord&));
};

class MockMdnsService : public MdnsService {
 public:
  void StartQuery(const DomainName& name,
                  DnsType dns_type,
                  DnsClass dns_class,
                  MdnsRecordChangedCallback* callback) override {
    FAIL();
  }

  void StopQuery(const DomainName& name,
                 DnsType dns_type,
                 DnsClass dns_class,
                 MdnsRecordChangedCallback* callback) override {
    FAIL();
  }

  void ReinitializeQueries(const DomainName& name) override { FAIL(); }

  MOCK_METHOD3(StartProbe,
               Error(MdnsDomainConfirmedProvider*, DomainName, IPAddress));
  MOCK_METHOD2(UpdateRegisteredRecord,
               Error(const MdnsRecord&, const MdnsRecord&));
  MOCK_METHOD1(RegisterRecord, Error(const MdnsRecord& record));
  MOCK_METHOD1(UnregisterRecord, Error(const MdnsRecord& record));
};

class PublisherImplTest : public testing::Test {
 public:
  PublisherImplTest()
      : clock_(Clock::now()),
        task_runner_(&clock_),
        publisher_(&mock_service_, &reporting_client_, &task_runner_) {}

  MockMdnsService* mdns_service() { return &mock_service_; }
  TaskRunner* task_runner() { return &task_runner_; }
  PublisherImpl* publisher() { return &publisher_; }

  // Calls PublisherImpl::OnDomainFound() through the public interface it
  // implements.
  void CallOnDomainFound(const DomainName& domain, const DomainName& domain2) {
    static_cast<MdnsDomainConfirmedProvider&>(publisher_)
        .OnDomainFound(domain, domain2);
  }

 private:
  FakeClock clock_;
  FakeTaskRunner task_runner_;
  StrictMock<MockMdnsService> mock_service_;
  StrictMock<MockReportingClient> reporting_client_;
  PublisherImpl publisher_;
};

TEST_F(PublisherImplTest, TestRegistrationAndDegrestration) {
  IPAddress address = IPAddress(192, 168, 0, 0);
  const DomainName domain{"instance", "_service", "_udp", "domain"};
  const DomainName domain2{"instance2", "_service", "_udp", "domain"};
  const DnsSdInstanceRecord record("instance", "_service._udp", "domain",
                                   {address, 80}, {});
  const DnsSdInstanceRecord record2("instance2", "_service._udp", "domain",
                                    {address, 80}, {});
  MockClient client;

  EXPECT_CALL(*mdns_service(), StartProbe(publisher(), domain, _)).Times(1);
  publisher()->Register(record, &client);
  testing::Mock::VerifyAndClearExpectations(mdns_service());

  int seen = 0;
  EXPECT_CALL(*mdns_service(), RegisterRecord(_))
      .Times(4)
      .WillRepeatedly([&seen, &address,
                       &domain2](const MdnsRecord& record) mutable -> Error {
        if (record.dns_type() == DnsType::kA) {
          const ARecordRdata& data = absl::get<ARecordRdata>(record.rdata());
          if (data.ipv4_address() == address) {
            seen++;
          }
        } else if (record.dns_type() == DnsType::kSRV) {
          const SrvRecordRdata& data =
              absl::get<SrvRecordRdata>(record.rdata());
          if (data.port() == 80) {
            seen++;
          }
        }

        if (record.dns_type() != DnsType::kPTR) {
          EXPECT_EQ(record.name(), domain2);
        }
        return Error::None();
      });
  EXPECT_CALL(client, OnInstanceClaimed(record, record2));
  CallOnDomainFound(domain, domain2);
  EXPECT_EQ(seen, 2);
  testing::Mock::VerifyAndClearExpectations(mdns_service());
  testing::Mock::VerifyAndClearExpectations(&client);

  seen = 0;
  EXPECT_CALL(*mdns_service(), UnregisterRecord(_))
      .Times(4)
      .WillRepeatedly([&seen,
                       &address](const MdnsRecord& record) mutable -> Error {
        if (record.dns_type() == DnsType::kA) {
          const ARecordRdata& data = absl::get<ARecordRdata>(record.rdata());
          if (data.ipv4_address() == address) {
            seen++;
          }
        } else if (record.dns_type() == DnsType::kSRV) {
          const SrvRecordRdata& data =
              absl::get<SrvRecordRdata>(record.rdata());
          if (data.port() == 80) {
            seen++;
          }
        }
        return Error::None();
      });
  publisher()->DeregisterAll("_service._udp");
  EXPECT_EQ(seen, 2);
}

TEST_F(PublisherImplTest, TestUpdate) {
  IPAddress address = IPAddress(192, 168, 0, 0);
  DomainName domain{"instance", "_service", "_udp", "domain"};
  DnsSdTxtRecord txt;
  txt.SetFlag("id", true);
  DnsSdInstanceRecord record("instance", "_service._udp", "domain",
                             {address, 80}, std::move(txt));
  MockClient client;

  // Update a non-existent record
  EXPECT_FALSE(publisher()->UpdateRegistration(record).ok());

  // Update a record during the probing phase
  EXPECT_CALL(*mdns_service(), StartProbe(publisher(), domain, _)).Times(1);
  EXPECT_EQ(publisher()->Register(record, &client), Error::None());
  testing::Mock::VerifyAndClearExpectations(mdns_service());

  IPAddress address2 = IPAddress(1, 2, 3, 4, 5, 6, 7, 8);
  DnsSdTxtRecord txt2;
  txt2.SetFlag("id2", true);
  DnsSdInstanceRecord record2("instance", "_service._udp", "domain",
                              {address2, 80}, std::move(txt2));
  EXPECT_EQ(publisher()->UpdateRegistration(record2), Error::None());

  bool seen_v6 = false;
  EXPECT_CALL(*mdns_service(), RegisterRecord(_))
      .Times(4)
      .WillRepeatedly([&seen_v6](const MdnsRecord& record) mutable -> Error {
        EXPECT_NE(record.dns_type(), DnsType::kA);
        if (record.dns_type() == DnsType::kAAAA) {
          seen_v6 = true;
        }
        return Error::None();
      });
  EXPECT_CALL(client, OnInstanceClaimed(record2, record2));
  CallOnDomainFound(domain, domain);
  EXPECT_TRUE(seen_v6);
  testing::Mock::VerifyAndClearExpectations(mdns_service());
  testing::Mock::VerifyAndClearExpectations(&client);

  // Update a record once it has been published.
  EXPECT_CALL(*mdns_service(), RegisterRecord(_))
      .WillOnce([](const MdnsRecord& record) -> Error {
        EXPECT_EQ(record.dns_type(), DnsType::kA);
        return Error::None();
      });
  EXPECT_CALL(*mdns_service(), UnregisterRecord(_))
      .WillOnce([](const MdnsRecord& record) -> Error {
        EXPECT_EQ(record.dns_type(), DnsType::kAAAA);
        return Error::None();
      });
  EXPECT_CALL(*mdns_service(), UpdateRegisteredRecord(_, _))
      .WillOnce(
          [](const MdnsRecord& record, const MdnsRecord& record2) -> Error {
            EXPECT_EQ(record.dns_type(), DnsType::kTXT);
            EXPECT_EQ(record2.dns_type(), DnsType::kTXT);
            return Error::None();
          });
  EXPECT_EQ(publisher()->UpdateRegistration(record), Error::None());
  testing::Mock::VerifyAndClearExpectations(mdns_service());
}

}  // namespace
}  // namespace discovery
}  // namespace openscreen

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/publisher_impl.h"

#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace openscreen {
namespace discovery {

using testing::_;
using testing::Return;
using testing::StrictMock;

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
               Error(MdnsDomainConfirmedProvider*, DomainName, IPEndpoint));
  MOCK_METHOD2(UpdateRegisteredRecord,
               Error(const MdnsRecord&, const MdnsRecord&));
  MOCK_METHOD1(RegisterRecord, Error(const MdnsRecord& record));
  MOCK_METHOD1(UnregisterRecord, Error(const MdnsRecord& record));
};

class PublisherTesting : public PublisherImpl {
 public:
  PublisherTesting() : PublisherImpl(&mock_service_) {}

  MockMdnsService& mdns_service() { return mock_service_; }

 private:
  StrictMock<MockMdnsService> mock_service_;
};

TEST(DnsSdPublisherImplTests, TestRegisterAndDeregister) {
  PublisherTesting publisher;
  IPAddress address = IPAddress(192, 168, 0, 0);
  DnsSdInstanceRecord record("instance", "_service._udp", "domain",
                             {address, 80}, {});

  int seen = 0;
  EXPECT_CALL(publisher.mdns_service(), RegisterRecord(_))
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
        };
        return Error::None();
      });
  publisher.Register(record);
  EXPECT_EQ(seen, 2);

  seen = 0;
  EXPECT_CALL(publisher.mdns_service(), UnregisterRecord(_))
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
        };
        return Error::None();
      });
  publisher.DeregisterAll("_service._udp");
  EXPECT_EQ(seen, 2);
}

TEST(DnsSdPublisherImplTests, TestUpdate) {
  PublisherTesting publisher;
  IPAddress address = IPAddress{192, 168, 0, 0};
  DnsSdTxtRecord txt;
  txt.SetFlag("id", true);
  DnsSdInstanceRecord record("instance", "_service._udp", "domain",
                             {address, 80}, std::move(txt));
  EXPECT_NE(publisher.UpdateRegistration(record), Error::None());

  EXPECT_CALL(publisher.mdns_service(), RegisterRecord(_)).Times(4);
  EXPECT_EQ(publisher.Register(record), Error::None());
  testing::Mock::VerifyAndClearExpectations(&publisher.mdns_service());

  EXPECT_NE(publisher.UpdateRegistration(record), Error::None());

  IPAddress address2 = IPAddress(1, 2, 3, 4, 5, 6, 7, 8);
  DnsSdTxtRecord txt2;
  txt2.SetFlag("id2", true);
  DnsSdInstanceRecord record2("instance", "_service._udp", "domain",
                              {address2, 80}, std::move(txt2));
  EXPECT_CALL(publisher.mdns_service(), RegisterRecord(_))
      .WillOnce([](const MdnsRecord& record) -> Error {
        EXPECT_EQ(record.dns_type(), DnsType::kAAAA);
        return Error::None();
      });
  EXPECT_CALL(publisher.mdns_service(), UnregisterRecord(_))
      .WillOnce([](const MdnsRecord& record) -> Error {
        EXPECT_EQ(record.dns_type(), DnsType::kA);
        return Error::None();
      });
  EXPECT_CALL(publisher.mdns_service(), UpdateRegisteredRecord(_, _))
      .WillOnce(
          [](const MdnsRecord& record, const MdnsRecord& record2) -> Error {
            EXPECT_EQ(record.dns_type(), DnsType::kTXT);
            EXPECT_EQ(record2.dns_type(), DnsType::kTXT);
            return Error::None();
          });
  EXPECT_EQ(publisher.UpdateRegistration(record2), Error::None());
}

}  // namespace discovery
}  // namespace openscreen

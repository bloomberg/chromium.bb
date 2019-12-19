// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/mdns_publisher.h"

#include <chrono>
#include <vector>

#include "discovery/mdns/mdns_sender.h"
#include "discovery/mdns/testing/mdns_test_util.h"
#include "platform/test/fake_task_runner.h"
#include "platform/test/fake_udp_socket.h"

using testing::_;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

namespace openscreen {
namespace discovery {
namespace {

constexpr Clock::duration kAnnounceGoodbyeDelay =
    std::chrono::duration_cast<Clock::duration>(std::chrono::milliseconds(25));

bool ContainsRecord(const std::vector<MdnsRecord::ConstRef>& records,
                    MdnsRecord record) {
  return std::find_if(records.begin(), records.end(),
                      [&record](const MdnsRecord& ref) {
                        return ref == record;
                      }) != records.end();
}

}  // namespace

class MockMdnsSender : public MdnsSender {
 public:
  MockMdnsSender(UdpSocket* socket) : MdnsSender(socket) {}

  MOCK_METHOD1(SendMulticast, Error(const MdnsMessage& message));
  MOCK_METHOD2(SendUnicast,
               Error(const MdnsMessage& message, const IPEndpoint& endpoint));
};

class MdnsPublisherTesting : public MdnsPublisher {
 public:
  using MdnsPublisher::MdnsPublisher;
  ~MdnsPublisherTesting() override = default;

  MOCK_METHOD1(IsOwned, bool(const DomainName&));

  bool IsExclusiveOwner(const DomainName& name) override {
    bool is_owned = IsOwned(name);

    if (is_owned && records_.find(name) == records_.end()) {
      records_.emplace(name, std::vector<std::unique_ptr<RecordAnnouncer>>{});
    }

    return is_owned;
  }

  bool IsMappedAsExclusiveOwner(const DomainName& name) {
    return records_.find(name) != records_.end();
  }

  const MdnsRecord* GetPtrRecord(const DomainName& domain) {
    std::vector<MdnsRecord::ConstRef> records =
        GetRecords(domain, DnsType::kPTR, DnsClass::kANY);
    EXPECT_LE(records.size(), size_t{1});
    return records.empty() ? nullptr : &records[0].get();
  }

  std::vector<MdnsRecord::ConstRef> GetNonPtrRecords(const DomainName& domain,
                                                     DnsType type) {
    return GetRecords(domain, type, DnsClass::kANY);
  }
};

class MdnsPublisherTest : public testing::Test {
 public:
  MdnsPublisherTest()
      : clock_(Clock::now()),
        task_runner_(&clock_),
        socket_(FakeUdpSocket::CreateDefault()),
        sender_(socket_.get()),
        publisher_(nullptr, &sender_, &task_runner_, nullptr, FakeClock::now) {}

  ~MdnsPublisherTest() {
    // Clear out any remaining calls in the task runner queue.
    clock_.Advance(
        std::chrono::duration_cast<Clock::duration>(std::chrono::seconds(1)));
  }

 protected:
  Error IsAnnounced(const MdnsRecord& original, const MdnsMessage& message) {
    EXPECT_EQ(message.type(), MessageType::Response);
    EXPECT_EQ(message.questions().size(), size_t{0});
    EXPECT_EQ(message.authority_records().size(), size_t{0});
    EXPECT_EQ(message.additional_records().size(), size_t{0});
    EXPECT_EQ(message.answers().size(), size_t{1});

    const MdnsRecord& sent = message.answers()[0];
    EXPECT_EQ(original.name(), sent.name());
    EXPECT_EQ(original.dns_type(), sent.dns_type());
    EXPECT_EQ(original.dns_class(), sent.dns_class());
    EXPECT_EQ(original.record_type(), sent.record_type());
    EXPECT_EQ(original.rdata(), sent.rdata());
    EXPECT_EQ(original.ttl(), sent.ttl());
    return Error::None();
  }

  Error IsGoodbyeRecord(const MdnsRecord& original,
                        const MdnsMessage& message) {
    EXPECT_EQ(message.type(), MessageType::Response);
    EXPECT_EQ(message.questions().size(), size_t{0});
    EXPECT_EQ(message.authority_records().size(), size_t{0});
    EXPECT_EQ(message.additional_records().size(), size_t{0});
    EXPECT_EQ(message.answers().size(), size_t{1});

    const MdnsRecord& sent = message.answers()[0];
    EXPECT_EQ(original.name(), sent.name());
    EXPECT_EQ(original.dns_type(), sent.dns_type());
    EXPECT_EQ(original.dns_class(), sent.dns_class());
    EXPECT_EQ(original.record_type(), sent.record_type());
    EXPECT_EQ(original.rdata(), sent.rdata());
    EXPECT_EQ(std::chrono::seconds(0), sent.ttl());
    return Error::None();
  }

  void TestUniqueRecordRegistrationWorkflow(MdnsRecord record,
                                            MdnsRecord record2) {
    EXPECT_CALL(publisher_, IsOwned(domain_)).WillRepeatedly(Return(true));
    DnsType type = record.dns_type();

    // Check preconditions.
    ASSERT_EQ(record.dns_type(), record2.dns_type());
    auto records = publisher_.GetNonPtrRecords(domain_, type);
    ASSERT_EQ(publisher_.GetRecordCount(), size_t{0});
    ASSERT_EQ(records.size(), size_t{0});
    ASSERT_NE(record, record2);
    ASSERT_TRUE(records.empty());

    // Register a new record.
    EXPECT_CALL(sender_, SendMulticast(_))
        .WillOnce([this, &record](const MdnsMessage& message) -> Error {
          return IsAnnounced(record, message);
        });
    EXPECT_TRUE(publisher_.RegisterRecord(record).ok());
    clock_.Advance(kAnnounceGoodbyeDelay);
    testing::Mock::VerifyAndClearExpectations(&sender_);
    EXPECT_EQ(publisher_.GetRecordCount(), size_t{1});
    records = publisher_.GetNonPtrRecords(domain_, type);
    EXPECT_EQ(records.size(), size_t{1});
    EXPECT_TRUE(ContainsRecord(records, record));
    EXPECT_TRUE(!ContainsRecord(records, record2));
    EXPECT_TRUE(publisher_.IsMappedAsExclusiveOwner(domain_));

    // Re-register the same record.
    EXPECT_FALSE(publisher_.RegisterRecord(record).ok());
    clock_.Advance(kAnnounceGoodbyeDelay);
    testing::Mock::VerifyAndClearExpectations(&sender_);
    EXPECT_EQ(publisher_.GetRecordCount(), size_t{1});
    records = publisher_.GetNonPtrRecords(domain_, type);
    EXPECT_EQ(records.size(), size_t{1});
    EXPECT_TRUE(ContainsRecord(records, record));
    EXPECT_TRUE(!ContainsRecord(records, record2));
    EXPECT_TRUE(publisher_.IsMappedAsExclusiveOwner(domain_));

    // Update a record that doesn't exist
    EXPECT_FALSE(publisher_.UpdateRegisteredRecord(record2, record).ok());
    clock_.Advance(kAnnounceGoodbyeDelay);
    testing::Mock::VerifyAndClearExpectations(&sender_);
    EXPECT_EQ(publisher_.GetRecordCount(), size_t{1});
    records = publisher_.GetNonPtrRecords(domain_, type);
    EXPECT_EQ(records.size(), size_t{1});
    EXPECT_TRUE(ContainsRecord(records, record));
    EXPECT_TRUE(!ContainsRecord(records, record2));
    EXPECT_TRUE(publisher_.IsMappedAsExclusiveOwner(domain_));

    // Update an existing record.
    EXPECT_CALL(sender_, SendMulticast(_))
        .WillOnce([this, &record2](const MdnsMessage& message) -> Error {
          return IsAnnounced(record2, message);
        });
    EXPECT_TRUE(publisher_.UpdateRegisteredRecord(record, record2).ok());
    clock_.Advance(kAnnounceGoodbyeDelay);
    testing::Mock::VerifyAndClearExpectations(&sender_);
    EXPECT_EQ(publisher_.GetRecordCount(), size_t{1});
    records = publisher_.GetNonPtrRecords(domain_, type);
    EXPECT_EQ(records.size(), size_t{1});
    EXPECT_TRUE(!ContainsRecord(records, record));
    EXPECT_TRUE(ContainsRecord(records, record2));
    EXPECT_TRUE(publisher_.IsMappedAsExclusiveOwner(domain_));

    // Add back the original record
    EXPECT_CALL(sender_, SendMulticast(_))
        .WillOnce([this, &record](const MdnsMessage& message) -> Error {
          return IsAnnounced(record, message);
        });
    EXPECT_TRUE(publisher_.RegisterRecord(record).ok());
    clock_.Advance(kAnnounceGoodbyeDelay);
    testing::Mock::VerifyAndClearExpectations(&sender_);
    EXPECT_EQ(publisher_.GetRecordCount(), size_t{2});
    records = publisher_.GetNonPtrRecords(domain_, type);
    EXPECT_EQ(records.size(), size_t{2});
    EXPECT_TRUE(ContainsRecord(records, record));
    EXPECT_TRUE(ContainsRecord(records, record2));
    EXPECT_TRUE(publisher_.IsMappedAsExclusiveOwner(domain_));

    // Delete an existing record.
    EXPECT_CALL(sender_, SendMulticast(_))
        .WillOnce([this, &record2](const MdnsMessage& message) -> Error {
          return IsGoodbyeRecord(record2, message);
        });
    EXPECT_TRUE(publisher_.UnregisterRecord(record2).ok());
    clock_.Advance(kAnnounceGoodbyeDelay);
    testing::Mock::VerifyAndClearExpectations(&sender_);
    EXPECT_EQ(publisher_.GetRecordCount(), size_t{1});
    records = publisher_.GetNonPtrRecords(domain_, type);
    EXPECT_EQ(records.size(), size_t{1});
    EXPECT_TRUE(ContainsRecord(records, record));
    EXPECT_TRUE(!ContainsRecord(records, record2));
    EXPECT_TRUE(publisher_.IsMappedAsExclusiveOwner(domain_));

    // Delete a non-existing record.
    EXPECT_FALSE(publisher_.UnregisterRecord(record2).ok());
    clock_.Advance(kAnnounceGoodbyeDelay);
    testing::Mock::VerifyAndClearExpectations(&sender_);
    EXPECT_EQ(publisher_.GetRecordCount(), size_t{1});
    records = publisher_.GetNonPtrRecords(domain_, type);
    EXPECT_EQ(records.size(), size_t{1});
    EXPECT_TRUE(ContainsRecord(records, record));
    EXPECT_TRUE(!ContainsRecord(records, record2));
    EXPECT_TRUE(publisher_.IsMappedAsExclusiveOwner(domain_));

    // Delete the last record
    EXPECT_CALL(sender_, SendMulticast(_))
        .WillOnce([this, &record](const MdnsMessage& message) -> Error {
          return IsGoodbyeRecord(record, message);
        });
    EXPECT_TRUE(publisher_.UnregisterRecord(record).ok());
    clock_.Advance(kAnnounceGoodbyeDelay);
    testing::Mock::VerifyAndClearExpectations(&sender_);
    EXPECT_EQ(publisher_.GetRecordCount(), size_t{0});
    records = publisher_.GetNonPtrRecords(domain_, type);
    EXPECT_EQ(records.size(), size_t{0});
    EXPECT_TRUE(!ContainsRecord(records, record));
    EXPECT_TRUE(!ContainsRecord(records, record2));
    EXPECT_FALSE(publisher_.IsMappedAsExclusiveOwner(domain_));
  }

  FakeClock clock_;
  FakeTaskRunner task_runner_;
  std::unique_ptr<FakeUdpSocket> socket_;
  StrictMock<MockMdnsSender> sender_;
  MdnsPublisherTesting publisher_;

  DomainName domain_{"instance", "_googlecast", "_tcp", "local"};
};

TEST_F(MdnsPublisherTest, ARecordRegistrationWorkflow) {
  const MdnsRecord record1 = GetFakeARecord(domain_);
  const MdnsRecord record2 =
      GetFakeARecord(domain_, std::chrono::seconds(1000));
  TestUniqueRecordRegistrationWorkflow(record1, record2);
}

TEST_F(MdnsPublisherTest, AAAARecordRegistrationWorkflow) {
  const MdnsRecord record1 = GetFakeAAAARecord(domain_);
  const MdnsRecord record2 =
      GetFakeAAAARecord(domain_, std::chrono::seconds(1000));
  TestUniqueRecordRegistrationWorkflow(record1, record2);
}

TEST_F(MdnsPublisherTest, TXTRecordRegistrationWorkflow) {
  const MdnsRecord record1 = GetFakeTxtRecord(domain_);
  const MdnsRecord record2 =
      GetFakeTxtRecord(domain_, std::chrono::seconds(1000));
  TestUniqueRecordRegistrationWorkflow(record1, record2);
}

TEST_F(MdnsPublisherTest, SRVRecordRegistrationWorkflow) {
  const MdnsRecord record1 = GetFakeSrvRecord(domain_);
  const MdnsRecord record2 =
      GetFakeSrvRecord(domain_, std::chrono::seconds(1000));
  TestUniqueRecordRegistrationWorkflow(record1, record2);
}

TEST_F(MdnsPublisherTest, PTRRecordRegistrationWorkflow) {
  EXPECT_CALL(publisher_, IsOwned(domain_)).WillRepeatedly(Return(true));
  const MdnsRecord record1 = GetFakePtrRecord(domain_);
  const MdnsRecord record2 =
      GetFakePtrRecord(domain_, std::chrono::seconds(1000));

  // Register a new record.
  EXPECT_CALL(sender_, SendMulticast(_))
      .WillOnce([this, &record1](const MdnsMessage& message) -> Error {
        return IsAnnounced(record1, message);
      });
  EXPECT_TRUE(publisher_.RegisterRecord(record1).ok());
  clock_.Advance(kAnnounceGoodbyeDelay);
  testing::Mock::VerifyAndClearExpectations(&sender_);
  EXPECT_EQ(publisher_.GetRecordCount(), size_t{1});
  auto* record = publisher_.GetPtrRecord(record1.name());
  ASSERT_NE(record, nullptr);
  EXPECT_EQ(*record, record1);
  EXPECT_TRUE(publisher_.IsMappedAsExclusiveOwner(domain_));

  // Register another record with the same domain name.
  EXPECT_FALSE(publisher_.RegisterRecord(record2).ok());
  clock_.Advance(kAnnounceGoodbyeDelay);
  testing::Mock::VerifyAndClearExpectations(&sender_);
  EXPECT_EQ(publisher_.GetRecordCount(), size_t{1});
  record = publisher_.GetPtrRecord(record1.name());
  ASSERT_NE(record, nullptr);
  EXPECT_EQ(*record, record1);
  EXPECT_TRUE(publisher_.IsMappedAsExclusiveOwner(domain_));

  // Delete an existing record.
  EXPECT_CALL(sender_, SendMulticast(_))
      .WillOnce([this, &record1](const MdnsMessage& message) -> Error {
        return IsGoodbyeRecord(record1, message);
      });
  EXPECT_TRUE(publisher_.UnregisterRecord(record1).ok());
  clock_.Advance(kAnnounceGoodbyeDelay);
  testing::Mock::VerifyAndClearExpectations(&sender_);
  EXPECT_EQ(publisher_.GetRecordCount(), size_t{0});
  record = publisher_.GetPtrRecord(record1.name());
  ASSERT_EQ(record, nullptr);
  EXPECT_FALSE(publisher_.IsMappedAsExclusiveOwner(domain_));
}

TEST_F(MdnsPublisherTest, RegisteringUnownedRecordsFail) {
  EXPECT_CALL(publisher_, IsOwned(domain_)).WillRepeatedly(Return(false));
  EXPECT_FALSE(publisher_.RegisterRecord(GetFakePtrRecord(domain_)).ok());
  EXPECT_FALSE(publisher_.RegisterRecord(GetFakeSrvRecord(domain_)).ok());
  EXPECT_FALSE(publisher_.RegisterRecord(GetFakeTxtRecord(domain_)).ok());
  EXPECT_FALSE(publisher_.RegisterRecord(GetFakeARecord(domain_)).ok());
  EXPECT_FALSE(publisher_.RegisterRecord(GetFakeAAAARecord(domain_)).ok());
}

TEST_F(MdnsPublisherTest, RegistrationAnnouncesEightTimes) {
  EXPECT_CALL(publisher_, IsOwned(domain_)).WillRepeatedly(Return(true));
  constexpr Clock::duration kOneSecond =
      std::chrono::duration_cast<Clock::duration>(std::chrono::seconds(1));

  // First announce, at registration.
  const MdnsRecord record = GetFakeARecord(domain_);
  EXPECT_CALL(sender_, SendMulticast(_))
      .WillOnce([this, &record](const MdnsMessage& message) -> Error {
        return IsAnnounced(record, message);
      });
  EXPECT_TRUE(publisher_.RegisterRecord(record).ok());
  clock_.Advance(kAnnounceGoodbyeDelay);

  // Second announce, at 2 seconds.
  testing::Mock::VerifyAndClearExpectations(&sender_);
  EXPECT_CALL(sender_, SendMulticast(_))
      .WillOnce([this, &record](const MdnsMessage& message) -> Error {
        return IsAnnounced(record, message);
      });
  clock_.Advance(kOneSecond);
  clock_.Advance(kAnnounceGoodbyeDelay);
  testing::Mock::VerifyAndClearExpectations(&sender_);

  // Third announce, at 4 seconds.
  clock_.Advance(kOneSecond);
  clock_.Advance(kAnnounceGoodbyeDelay);
  EXPECT_CALL(sender_, SendMulticast(_))
      .WillOnce([this, &record](const MdnsMessage& message) -> Error {
        return IsAnnounced(record, message);
      });
  clock_.Advance(kOneSecond);
  clock_.Advance(kAnnounceGoodbyeDelay);
  testing::Mock::VerifyAndClearExpectations(&sender_);

  // Fourth announce, at 8 seconds.
  clock_.Advance(kOneSecond * 3);
  EXPECT_CALL(sender_, SendMulticast(_))
      .WillOnce([this, &record](const MdnsMessage& message) -> Error {
        return IsAnnounced(record, message);
      });
  clock_.Advance(kOneSecond);
  clock_.Advance(kAnnounceGoodbyeDelay);
  testing::Mock::VerifyAndClearExpectations(&sender_);

  // Fifth announce, at 16 seconds.
  clock_.Advance(kOneSecond * 7);
  EXPECT_CALL(sender_, SendMulticast(_))
      .WillOnce([this, &record](const MdnsMessage& message) -> Error {
        return IsAnnounced(record, message);
      });
  clock_.Advance(kOneSecond);
  clock_.Advance(kAnnounceGoodbyeDelay);
  testing::Mock::VerifyAndClearExpectations(&sender_);

  // Sixth announce, at 32 seconds.
  clock_.Advance(kOneSecond * 15);
  EXPECT_CALL(sender_, SendMulticast(_))
      .WillOnce([this, &record](const MdnsMessage& message) -> Error {
        return IsAnnounced(record, message);
      });
  clock_.Advance(kOneSecond);
  clock_.Advance(kAnnounceGoodbyeDelay);
  testing::Mock::VerifyAndClearExpectations(&sender_);

  // Seventh announce, at 64 seconds.
  clock_.Advance(kOneSecond * 31);
  EXPECT_CALL(sender_, SendMulticast(_))
      .WillOnce([this, &record](const MdnsMessage& message) -> Error {
        return IsAnnounced(record, message);
      });
  clock_.Advance(kOneSecond);
  clock_.Advance(kAnnounceGoodbyeDelay);
  testing::Mock::VerifyAndClearExpectations(&sender_);

  // Eighth announce, at 128 seconds.
  clock_.Advance(kOneSecond * 63);
  EXPECT_CALL(sender_, SendMulticast(_))
      .WillOnce([this, &record](const MdnsMessage& message) -> Error {
        return IsAnnounced(record, message);
      });
  clock_.Advance(kOneSecond);
  clock_.Advance(kAnnounceGoodbyeDelay);
  testing::Mock::VerifyAndClearExpectations(&sender_);

  // No more announcements
  clock_.Advance(kOneSecond * 1024);
  clock_.Advance(kAnnounceGoodbyeDelay);
  testing::Mock::VerifyAndClearExpectations(&sender_);

  // Sends goodbye message when removed.
  EXPECT_CALL(sender_, SendMulticast(_))
      .WillOnce([this, &record](const MdnsMessage& message) -> Error {
        return IsGoodbyeRecord(record, message);
      });
  EXPECT_TRUE(publisher_.UnregisterRecord(record).ok());
  clock_.Advance(kAnnounceGoodbyeDelay);
}

}  // namespace discovery
}  // namespace openscreen

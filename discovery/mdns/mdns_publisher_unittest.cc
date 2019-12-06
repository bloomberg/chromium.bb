// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/mdns_publisher.h"

#include <vector>

#include "discovery/mdns/testing/mdns_test_util.h"
#include "platform/test/fake_task_runner.h"
#include "platform/test/fake_udp_socket.h"

using testing::Return;

namespace openscreen {
namespace discovery {
namespace {

bool ContainsRecord(const std::vector<MdnsRecord::ConstRef>& records,
                    MdnsRecord record) {
  return std::find_if(records.begin(), records.end(),
                      [&record](const MdnsRecord& ref) {
                        return ref == record;
                      }) != records.end();
}

}  // namespace

class MdnsPublisherTesting : public MdnsPublisher {
 public:
  using MdnsPublisher::MdnsPublisher;
  ~MdnsPublisherTesting() override = default;

  MOCK_METHOD1(IsOwned, bool(const DomainName&));

  bool IsExclusiveOwner(const DomainName& name) override {
    bool is_owned = IsOwned(name);

    if (is_owned && records_.find(name) == records_.end()) {
      records_.emplace(name, std::vector<MdnsRecord>{});
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
        publisher_(nullptr, nullptr, &task_runner_, nullptr) {}

 protected:
  void TestUniqueRecordRegistrationWorkflow(MdnsRecord record,
                                            MdnsRecord record2) {
    EXPECT_CALL(publisher_, IsOwned(domain_)).WillRepeatedly(Return(true));
    DnsType type = record.dns_type();

    // Check preconditions.
    ASSERT_EQ(record.dns_type(), record2.dns_type());
    auto records = publisher_.GetNonPtrRecords(domain_, type);
    ASSERT_EQ(publisher_.GetRecordCount(), size_t{0});
    ASSERT_EQ(records.size(), size_t{0});
    ASSERT_TRUE(records.empty());

    // Register a new record.
    EXPECT_TRUE(publisher_.RegisterRecord(record).ok());
    EXPECT_EQ(publisher_.GetRecordCount(), size_t{1});
    records = publisher_.GetNonPtrRecords(domain_, type);
    EXPECT_EQ(records.size(), size_t{1});
    EXPECT_TRUE(ContainsRecord(records, record));
    EXPECT_TRUE(!ContainsRecord(records, record2));
    EXPECT_TRUE(publisher_.IsMappedAsExclusiveOwner(domain_));

    // Re-register the same record.
    EXPECT_FALSE(publisher_.RegisterRecord(record).ok());
    EXPECT_EQ(publisher_.GetRecordCount(), size_t{1});
    records = publisher_.GetNonPtrRecords(domain_, type);
    EXPECT_EQ(records.size(), size_t{1});
    EXPECT_TRUE(ContainsRecord(records, record));
    EXPECT_TRUE(!ContainsRecord(records, record2));
    EXPECT_TRUE(publisher_.IsMappedAsExclusiveOwner(domain_));

    // Update a record that doesn't exist
    EXPECT_FALSE(publisher_.UpdateRegisteredRecord(record2, record).ok());
    EXPECT_EQ(publisher_.GetRecordCount(), size_t{1});
    records = publisher_.GetNonPtrRecords(domain_, type);
    EXPECT_EQ(records.size(), size_t{1});
    EXPECT_TRUE(ContainsRecord(records, record));
    EXPECT_TRUE(!ContainsRecord(records, record2));
    EXPECT_TRUE(publisher_.IsMappedAsExclusiveOwner(domain_));

    // Update an existing record.
    EXPECT_TRUE(publisher_.UpdateRegisteredRecord(record, record2).ok());
    EXPECT_EQ(publisher_.GetRecordCount(), size_t{1});
    records = publisher_.GetNonPtrRecords(domain_, type);
    EXPECT_EQ(records.size(), size_t{1});
    EXPECT_TRUE(!ContainsRecord(records, record));
    EXPECT_TRUE(ContainsRecord(records, record2));
    EXPECT_TRUE(publisher_.IsMappedAsExclusiveOwner(domain_));

    // Add back the original record
    EXPECT_TRUE(publisher_.RegisterRecord(record).ok());
    EXPECT_EQ(publisher_.GetRecordCount(), size_t{2});
    records = publisher_.GetNonPtrRecords(domain_, type);
    EXPECT_EQ(records.size(), size_t{2});
    EXPECT_TRUE(ContainsRecord(records, record));
    EXPECT_TRUE(ContainsRecord(records, record2));
    EXPECT_TRUE(publisher_.IsMappedAsExclusiveOwner(domain_));

    // Delete an existing record.
    EXPECT_TRUE(publisher_.UnregisterRecord(record2).ok());
    EXPECT_EQ(publisher_.GetRecordCount(), size_t{1});
    records = publisher_.GetNonPtrRecords(domain_, type);
    EXPECT_EQ(records.size(), size_t{1});
    EXPECT_TRUE(ContainsRecord(records, record));
    EXPECT_TRUE(!ContainsRecord(records, record2));
    EXPECT_TRUE(publisher_.IsMappedAsExclusiveOwner(domain_));

    // Delete a non-existing record.
    EXPECT_FALSE(publisher_.UnregisterRecord(record2).ok());
    EXPECT_EQ(publisher_.GetRecordCount(), size_t{1});
    records = publisher_.GetNonPtrRecords(domain_, type);
    EXPECT_EQ(records.size(), size_t{1});
    EXPECT_TRUE(ContainsRecord(records, record));
    EXPECT_TRUE(!ContainsRecord(records, record2));
    EXPECT_TRUE(publisher_.IsMappedAsExclusiveOwner(domain_));

    // Delete the last record
    EXPECT_TRUE(publisher_.UnregisterRecord(record).ok());
    EXPECT_EQ(publisher_.GetRecordCount(), size_t{0});
    records = publisher_.GetNonPtrRecords(domain_, type);
    EXPECT_EQ(records.size(), size_t{0});
    EXPECT_TRUE(!ContainsRecord(records, record));
    EXPECT_TRUE(!ContainsRecord(records, record2));
    EXPECT_FALSE(publisher_.IsMappedAsExclusiveOwner(domain_));
  }

  FakeClock clock_;
  FakeTaskRunner task_runner_;
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
  EXPECT_TRUE(publisher_.RegisterRecord(record1).ok());
  EXPECT_EQ(publisher_.GetRecordCount(), size_t{1});
  auto* record = publisher_.GetPtrRecord(record1.name());
  ASSERT_NE(record, nullptr);
  EXPECT_EQ(*record, record1);
  EXPECT_TRUE(publisher_.IsMappedAsExclusiveOwner(domain_));

  // Register another record with the same domain name.
  EXPECT_FALSE(publisher_.RegisterRecord(record2).ok());
  EXPECT_EQ(publisher_.GetRecordCount(), size_t{1});
  record = publisher_.GetPtrRecord(record1.name());
  ASSERT_NE(record, nullptr);
  EXPECT_EQ(*record, record1);
  EXPECT_TRUE(publisher_.IsMappedAsExclusiveOwner(domain_));

  // Delete an existing record.
  EXPECT_TRUE(publisher_.UnregisterRecord(record1).ok());
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

}  // namespace discovery
}  // namespace openscreen

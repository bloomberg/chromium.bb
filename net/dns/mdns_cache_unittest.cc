// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/mdns_cache.h"
#include "net/dns/record_parsed.h"
#include "net/dns/record_rdata.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;
using ::testing::StrictMock;

namespace net {

static const uint8 kTestResponsesDifferentAnswers[] = {
  // Answer 1
  // ghs.l.google.com in DNS format.
  0x03, 'g', 'h', 's',
  0x01, 'l',
  0x06, 'g', 'o', 'o', 'g', 'l', 'e',
  0x03, 'c', 'o', 'm',
  0x00,
  0x00, 0x01,         // TYPE is A.
  0x00, 0x01,         // CLASS is IN.
  0x00, 0x00,         // TTL (4 bytes) is 53 seconds.
  0x00, 0x35,
  0x00, 0x04,         // RDLENGTH is 4 bytes.
  0x4a, 0x7d,         // RDATA is the IP: 74.125.95.121
  0x5f, 0x79,

  // Answer 2
  // Pointer to answer 1
  0xc0, 0x00,
  0x00, 0x01,         // TYPE is A.
  0x00, 0x01,         // CLASS is IN.
  0x00, 0x00,         // TTL (4 bytes) is 53 seconds.
  0x00, 0x35,
  0x00, 0x04,         // RDLENGTH is 4 bytes.
  0x4a, 0x7d,         // RDATA is the IP: 74.125.95.122
  0x5f, 0x80,
};

static const uint8 kTestResponsesSameAnswers[] = {
  // Answer 1
  // ghs.l.google.com in DNS format.
  0x03, 'g', 'h', 's',
  0x01, 'l',
  0x06, 'g', 'o', 'o', 'g', 'l', 'e',
  0x03, 'c', 'o', 'm',
  0x00,
  0x00, 0x01,         // TYPE is A.
  0x00, 0x01,         // CLASS is IN.
  0x00, 0x00,         // TTL (4 bytes) is 53 seconds.
  0x00, 0x35,
  0x00, 0x04,         // RDLENGTH is 4 bytes.
  0x4a, 0x7d,         // RDATA is the IP: 74.125.95.121
  0x5f, 0x79,

  // Answer 2
  // Pointer to answer 1
  0xc0, 0x00,
  0x00, 0x01,         // TYPE is A.
  0x00, 0x01,         // CLASS is IN.
  0x00, 0x00,         // TTL (4 bytes) is 112 seconds.
  0x00, 0x70,
  0x00, 0x04,         // RDLENGTH is 4 bytes.
  0x4a, 0x7d,         // RDATA is the IP: 74.125.95.121
  0x5f, 0x79,
};

class RecordRemovalMock {
 public:
  MOCK_METHOD1(OnRecordRemoved, void(const RecordParsed*));
};

class MDnsCacheTest : public ::testing::Test {
 public:
  MDnsCacheTest()
      : default_time_(base::Time::FromDoubleT(1234.0)) {}
  virtual ~MDnsCacheTest() {}

 protected:
  base::Time default_time_;
  StrictMock<RecordRemovalMock> record_removal_;
  MDnsCache cache_;
};

// Test a single insert, corresponding lookup, and unsuccessful lookup.
TEST_F(MDnsCacheTest, InsertLookupSingle) {
  DnsRecordParser parser(kT1ResponseDatagram, sizeof(kT1ResponseDatagram),
                         sizeof(dns_protocol::Header));
  parser.SkipQuestion();

  scoped_ptr<const RecordParsed> record1;
  scoped_ptr<const RecordParsed> record2;
  std::vector<const RecordParsed*> results;

  record1 = RecordParsed::CreateFrom(&parser, default_time_);
  record2 = RecordParsed::CreateFrom(&parser, default_time_);

  EXPECT_EQ(MDnsCache::RecordAdded, cache_.UpdateDnsRecord(record1.Pass()));

  EXPECT_EQ(MDnsCache::RecordAdded, cache_.UpdateDnsRecord(record2.Pass()));

  cache_.FindDnsRecords(ARecordRdata::kType, "ghs.l.google.com", &results,
                        default_time_);

  EXPECT_EQ(1u, results.size());
  EXPECT_EQ(default_time_, results.front()->time_created());

  EXPECT_EQ("ghs.l.google.com", results.front()->name());

  results.clear();
  cache_.FindDnsRecords(PtrRecordRdata::kType, "ghs.l.google.com", &results,
                        default_time_);

  EXPECT_EQ(0u, results.size());
}

// Test that records expire when their ttl has passed.
TEST_F(MDnsCacheTest, Expiration) {
  DnsRecordParser parser(kT1ResponseDatagram, sizeof(kT1ResponseDatagram),
                         sizeof(dns_protocol::Header));
  parser.SkipQuestion();
  scoped_ptr<const RecordParsed> record1;
  scoped_ptr<const RecordParsed> record2;

  std::vector<const RecordParsed*> results;
  const RecordParsed* record_to_be_deleted;

  record1 = RecordParsed::CreateFrom(&parser, default_time_);
  base::TimeDelta ttl1 = base::TimeDelta::FromSeconds(record1->ttl());

  record2 = RecordParsed::CreateFrom(&parser, default_time_);
  base::TimeDelta ttl2 = base::TimeDelta::FromSeconds(record2->ttl());
  record_to_be_deleted = record2.get();

  EXPECT_EQ(MDnsCache::RecordAdded, cache_.UpdateDnsRecord(record1.Pass()));
  EXPECT_EQ(MDnsCache::RecordAdded, cache_.UpdateDnsRecord(record2.Pass()));

  cache_.FindDnsRecords(ARecordRdata::kType, "ghs.l.google.com", &results,
                        default_time_);

  EXPECT_EQ(1u, results.size());

  EXPECT_EQ(default_time_ + ttl2, cache_.next_expiration());


  cache_.FindDnsRecords(ARecordRdata::kType, "ghs.l.google.com", &results,
                        default_time_ + ttl2);

  EXPECT_EQ(0u, results.size());

  EXPECT_CALL(record_removal_, OnRecordRemoved(record_to_be_deleted));

  cache_.CleanupRecords(default_time_ + ttl2, base::Bind(
      &RecordRemovalMock::OnRecordRemoved, base::Unretained(&record_removal_)));

  // To make sure that we've indeed removed them from the map, check no funny
  // business happens once they're deleted for good.

  EXPECT_EQ(default_time_ + ttl1, cache_.next_expiration());
  cache_.FindDnsRecords(ARecordRdata::kType, "ghs.l.google.com", &results,
                        default_time_ + ttl2);

  EXPECT_EQ(0u, results.size());
}

// Test that a new record replacing one with the same identity (name/rrtype for
// unique records) causes the cache to output a "record changed" event.
TEST_F(MDnsCacheTest, RecordChange) {
  DnsRecordParser parser(kTestResponsesDifferentAnswers,
                         sizeof(kTestResponsesDifferentAnswers),
                         0);

  scoped_ptr<const RecordParsed> record1;
  scoped_ptr<const RecordParsed> record2;
  std::vector<const RecordParsed*> results;

  record1 = RecordParsed::CreateFrom(&parser, default_time_);
  record2 = RecordParsed::CreateFrom(&parser, default_time_);

  EXPECT_EQ(MDnsCache::RecordAdded, cache_.UpdateDnsRecord(record1.Pass()));
  EXPECT_EQ(MDnsCache::RecordChanged,
            cache_.UpdateDnsRecord(record2.Pass()));
}

// Test that a new record replacing an otherwise identical one already in the
// cache causes the cache to output a "no change" event.
TEST_F(MDnsCacheTest, RecordNoChange) {
  DnsRecordParser parser(kTestResponsesSameAnswers,
                         sizeof(kTestResponsesSameAnswers),
                         0);

  scoped_ptr<const RecordParsed> record1;
  scoped_ptr<const RecordParsed> record2;
  std::vector<const RecordParsed*> results;

  record1 = RecordParsed::CreateFrom(&parser, default_time_);
  record2 = RecordParsed::CreateFrom(&parser, default_time_ +
                                     base::TimeDelta::FromSeconds(1));

  EXPECT_EQ(MDnsCache::RecordAdded, cache_.UpdateDnsRecord(record1.Pass()));
  EXPECT_EQ(MDnsCache::NoChange, cache_.UpdateDnsRecord(record2.Pass()));
}

// Test that the next expiration time of the cache is updated properly on record
// insertion.
TEST_F(MDnsCacheTest, RecordPreemptExpirationTime) {
  DnsRecordParser parser(kTestResponsesSameAnswers,
                         sizeof(kTestResponsesSameAnswers),
                         0);

  scoped_ptr<const RecordParsed> record1;
  scoped_ptr<const RecordParsed> record2;
  std::vector<const RecordParsed*> results;

  record1 = RecordParsed::CreateFrom(&parser, default_time_);
  record2 = RecordParsed::CreateFrom(&parser, default_time_);
  base::TimeDelta ttl1 = base::TimeDelta::FromSeconds(record1->ttl());
  base::TimeDelta ttl2 = base::TimeDelta::FromSeconds(record2->ttl());

  EXPECT_EQ(base::Time(), cache_.next_expiration());
  EXPECT_EQ(MDnsCache::RecordAdded, cache_.UpdateDnsRecord(record2.Pass()));
  EXPECT_EQ(default_time_ + ttl2, cache_.next_expiration());
  EXPECT_EQ(MDnsCache::NoChange, cache_.UpdateDnsRecord(record1.Pass()));
  EXPECT_EQ(default_time_ + ttl1, cache_.next_expiration());
}

} // namespace net

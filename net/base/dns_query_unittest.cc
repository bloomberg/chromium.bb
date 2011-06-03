// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/dns_query.h"
#include "net/base/dns_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

// DNS query consists of a header followed by a question.  Header format
// and question format are described below.  For the meaning of specific
// fields, please see RFC 1035.

// Header format.
//                                  1  1  1  1  1  1
//    0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                      ID                       |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |QR|   Opcode  |AA|TC|RD|RA|   Z    |   RCODE   |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                    QDCOUNT                    |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                    ANCOUNT                    |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                    NSCOUNT                    |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                    ARCOUNT                    |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

// Question format.
//                                  1  1  1  1  1  1
//    0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                                               |
//  /                     QNAME                     /
//  /                                               /
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                     QTYPE                     |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                     QCLASS                    |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

TEST(DnsQueryTest, RandomIdTest) {
  const std::string kHostname = "www.google.com";
  const int kPort = 80;

  DnsQuery q1(kHostname, ADDRESS_FAMILY_IPV4, kPort);
  EXPECT_TRUE(q1.IsValid());
  EXPECT_EQ(kPort, q1.port());
  EXPECT_EQ(kDNS_A, q1.qtype());
  EXPECT_EQ(kHostname, q1.hostname());

  DnsQuery q2(kHostname, ADDRESS_FAMILY_IPV4, kPort);
  EXPECT_TRUE(q2.IsValid());
  EXPECT_EQ(kPort, q2.port());
  EXPECT_EQ(kDNS_A, q2.qtype());
  EXPECT_EQ(kHostname, q2.hostname());

  // This has a 1/2^16 probability of failure.
  EXPECT_FALSE(q1.id() == q2.id());
}

TEST(DnsQueryTest, ConstructorTest) {
  const std::string kHostname = "www.google.com";
  const int kPort = 80;

  DnsQuery q1(kHostname, ADDRESS_FAMILY_IPV4, kPort);
  EXPECT_TRUE(q1.IsValid());
  EXPECT_EQ(kPort, q1.port());
  EXPECT_EQ(kDNS_A, q1.qtype());
  EXPECT_EQ(kHostname, q1.hostname());

  uint8 id_hi = q1.id() >> 8, id_lo = q1.id() & 0xff;

  // See the top of the file for the description of a DNS query.
  const uint8 query_data[] = {
    // Header
    id_hi, id_lo,
    0x01, 0x00,               // Flags -- set RD (recursion desired) bit.
    0x00, 0x01,               // Set QDCOUNT (question count) to 1, all the
                              // rest are 0 for a query.
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,

    // Question
    0x03, 0x77, 0x77, 0x77,   // QNAME: www.google.com in DNS format.
    0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65,
    0x03, 0x63, 0x6f, 0x6d, 0x00,

    0x00, 0x01,               // QTYPE: A query.
    0x00, 0x01,               // QCLASS: IN class.
  };

  int expected_size = arraysize(query_data);
  EXPECT_EQ(expected_size, q1.io_buffer()->size());
  EXPECT_EQ(0, memcmp(q1.io_buffer()->data(), query_data,
                      q1.io_buffer()->size()));

  // Query with a long hostname.
  const char hostname_too_long[] = "123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.1234";

  DnsQuery q2(hostname_too_long, ADDRESS_FAMILY_IPV4, kPort);
  EXPECT_FALSE(q2.IsValid());
}

}  // namespace net

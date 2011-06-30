// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/rand_util.h"
#include "net/base/dns_transaction.h"
#include "net/base/dns_query.h"
#include "net/base/dns_util.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_util.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/socket_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

namespace {

int TestRng1(int /* min */, int /* max */) { return 1; }

std::vector<IPAddressNumber> CStringsToIPAddressList(
    const char* const ip_strings[], size_t size) {
  std::vector<IPAddressNumber> ip_addresses;
  for (size_t i = 0; i < size; ++i) {
    IPAddressNumber ip;
    EXPECT_TRUE(ParseIPLiteralToNumber(ip_strings[i], &ip));
    ip_addresses.push_back(ip);
  }
  return ip_addresses;
}

IPEndPoint CreateDnsAddress(const char* ip_string, uint16 port) {
  IPAddressNumber ip_address;
  DCHECK(ParseIPLiteralToNumber(ip_string, &ip_address));
  return IPEndPoint(ip_address, port);
}

static const char* kDnsIp = "192.168.1.1";
static const uint16 kDnsPort = 53;
static const base::TimeDelta kTimeoutsMs[] = {
  base::TimeDelta::FromMilliseconds(20),
  base::TimeDelta::FromMilliseconds(20),
  base::TimeDelta::FromMilliseconds(20),
};

//-----------------------------------------------------------------------------
// Query/response set for www.google.com, ID is fixed to 1.

static const uint16 kT1Qtype = kDNS_A;

static const char kT1DnsName[] = {
  0x03, 'w', 'w', 'w',
  0x06, 'g', 'o', 'o', 'g', 'l', 'e',
  0x03, 'c', 'o', 'm',
  0x00
};

static const uint8 kT1QueryDatagram[] = {
  // query for www.google.com, type A.
  0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x03, 0x77, 0x77, 0x77,
  0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x03,
  0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00, 0x01
};

static const uint8 kT1ResponseDatagram[] = {
  // response contains one CNAME for www.l.google.com and the following
  // IP addresses: 74.125.226.{179,180,176,177,178}
  0x00, 0x01, 0x81, 0x80, 0x00, 0x01, 0x00, 0x06,
  0x00, 0x00, 0x00, 0x00, 0x03, 0x77, 0x77, 0x77,
  0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x03,
  0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00, 0x01,
  0xc0, 0x0c, 0x00, 0x05, 0x00, 0x01, 0x00, 0x01,
  0x4d, 0x13, 0x00, 0x08, 0x03, 0x77, 0x77, 0x77,
  0x01, 0x6c, 0xc0, 0x10, 0xc0, 0x2c, 0x00, 0x01,
  0x00, 0x01, 0x00, 0x00, 0x00, 0xe4, 0x00, 0x04,
  0x4a, 0x7d, 0xe2, 0xb3, 0xc0, 0x2c, 0x00, 0x01,
  0x00, 0x01, 0x00, 0x00, 0x00, 0xe4, 0x00, 0x04,
  0x4a, 0x7d, 0xe2, 0xb4, 0xc0, 0x2c, 0x00, 0x01,
  0x00, 0x01, 0x00, 0x00, 0x00, 0xe4, 0x00, 0x04,
  0x4a, 0x7d, 0xe2, 0xb0, 0xc0, 0x2c, 0x00, 0x01,
  0x00, 0x01, 0x00, 0x00, 0x00, 0xe4, 0x00, 0x04,
  0x4a, 0x7d, 0xe2, 0xb1, 0xc0, 0x2c, 0x00, 0x01,
  0x00, 0x01, 0x00, 0x00, 0x00, 0xe4, 0x00, 0x04,
  0x4a, 0x7d, 0xe2, 0xb2
};

static const char* const kT1IpAddresses[] = {
  "74.125.226.179", "74.125.226.180", "74.125.226.176",
  "74.125.226.177", "74.125.226.178"
};

//-----------------------------------------------------------------------------
// Query/response set for codereview.chromium.org, ID is fixed to 2.
static const uint16 kT2Qtype = kDNS_A;

static const char kT2DnsName[] = {
  0x12, 'c', 'o', 'd', 'e', 'r', 'e', 'v', 'i', 'e', 'w',
  0x10, 'c', 'h', 'r', 'o', 'm', 'i', 'u', 'm',
  0x03, 'o', 'r', 'g',
  0x00
};

static const uint8 kT2QueryDatagram[] = {
  // query for codereview.chromium.org, type A.
  0x00, 0x02, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x0a, 0x63, 0x6f, 0x64,
  0x65, 0x72, 0x65, 0x76, 0x69, 0x65, 0x77, 0x08,
  0x63, 0x68, 0x72, 0x6f, 0x6d, 0x69, 0x75, 0x6d,
  0x03, 0x6f, 0x72, 0x67, 0x00, 0x00, 0x01, 0x00,
  0x01
};

static const uint8 kT2ResponseDatagram[] = {
  // response contains one CNAME for ghs.l.google.com and the following
  // IP address: 64.233.169.121
  0x00, 0x02, 0x81, 0x80, 0x00, 0x01, 0x00, 0x02,
  0x00, 0x00, 0x00, 0x00, 0x0a, 0x63, 0x6f, 0x64,
  0x65, 0x72, 0x65, 0x76, 0x69, 0x65, 0x77, 0x08,
  0x63, 0x68, 0x72, 0x6f, 0x6d, 0x69, 0x75, 0x6d,
  0x03, 0x6f, 0x72, 0x67, 0x00, 0x00, 0x01, 0x00,
  0x01, 0xc0, 0x0c, 0x00, 0x05, 0x00, 0x01, 0x00,
  0x01, 0x41, 0x75, 0x00, 0x12, 0x03, 0x67, 0x68,
  0x73, 0x01, 0x6c, 0x06, 0x67, 0x6f, 0x6f, 0x67,
  0x6c, 0x65, 0x03, 0x63, 0x6f, 0x6d, 0x00, 0xc0,
  0x35, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x01,
  0x0b, 0x00, 0x04, 0x40, 0xe9, 0xa9, 0x79
};

//-----------------------------------------------------------------------------

}  // namespace

TEST(DnsTransactionTest, NormalQueryResponseTest1) {
  MockWrite writes1[] = {
    MockWrite(true, reinterpret_cast<const char*>(kT1QueryDatagram),
              arraysize(kT1QueryDatagram))
  };

  MockRead reads1[] = {
    MockRead(true, reinterpret_cast<const char*>(kT1ResponseDatagram),
             arraysize(kT1ResponseDatagram))
  };

  StaticSocketDataProvider data(reads1, arraysize(reads1),
                                writes1, arraysize(writes1));
  MockClientSocketFactory factory;
  factory.AddSocketDataProvider(&data);

  std::vector<IPAddressNumber> actual_ip_addresses;
  std::vector<IPAddressNumber> expected_ip_addresses =
      CStringsToIPAddressList(kT1IpAddresses, arraysize(kT1IpAddresses));

  DnsTransaction t(CreateDnsAddress(kDnsIp, kDnsPort),
                   std::string(kT1DnsName, arraysize(kT1DnsName)),
                   kT1Qtype,
                   &actual_ip_addresses,
                   base::Bind(&TestRng1),
                   &factory);
  TestCompletionCallback callback;
  int rv = t.Start(&callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_TRUE(data.at_read_eof());
  EXPECT_TRUE(data.at_write_eof());

  EXPECT_TRUE(actual_ip_addresses == expected_ip_addresses);
}

TEST(DnsTransactionTest, MismatchedQueryResponseTest) {
  MockWrite writes1[] = {
    MockWrite(true, reinterpret_cast<const char*>(kT1QueryDatagram),
              arraysize(kT1QueryDatagram))
  };

  MockRead reads2[] = {
    MockRead(true, reinterpret_cast<const char*>(kT2ResponseDatagram),
             arraysize(kT2ResponseDatagram))
  };

  StaticSocketDataProvider data(reads2, arraysize(reads2),
                                writes1, arraysize(writes1));
  MockClientSocketFactory factory;
  factory.AddSocketDataProvider(&data);

  std::vector<IPAddressNumber> ip_addresses;
  DnsTransaction t(CreateDnsAddress(kDnsIp, kDnsPort),
                   std::string(kT1DnsName, arraysize(kT1DnsName)),
                   kT1Qtype,
                   &ip_addresses,
                   base::Bind(&TestRng1),
                   &factory);
  TestCompletionCallback callback;
  int rv = t.Start(&callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_TRUE(data.at_read_eof());
  EXPECT_TRUE(data.at_write_eof());

  EXPECT_EQ(ERR_DNS_MALFORMED_RESPONSE, rv);
}

// Test that after the first timeout we do a fresh connection and if we get
// a response on the new connection, we return it.
TEST(DnsTransactionTest, FirstTimeoutTest) {
  MockWrite writes1[] = {
    MockWrite(true, reinterpret_cast<const char*>(kT1QueryDatagram),
              arraysize(kT1QueryDatagram))
  };

  MockRead reads1[] = {
    MockRead(true, reinterpret_cast<const char*>(kT1ResponseDatagram),
             arraysize(kT1ResponseDatagram))
  };

  scoped_refptr<DelayedSocketData> socket1_data(
      new DelayedSocketData(2, NULL, 0, writes1, arraysize(writes1)));
  scoped_refptr<DelayedSocketData> socket2_data(
      new DelayedSocketData(0, reads1, arraysize(reads1),
                            writes1, arraysize(writes1)));
  MockClientSocketFactory factory;
  factory.AddSocketDataProvider(socket1_data.get());
  factory.AddSocketDataProvider(socket2_data.get());

  std::vector<IPAddressNumber> expected_ip_addresses =
      CStringsToIPAddressList(kT1IpAddresses, arraysize(kT1IpAddresses));

  std::vector<IPAddressNumber> actual_ip_addresses;
  DnsTransaction transaction(CreateDnsAddress(kDnsIp, kDnsPort),
                             std::string(kT1DnsName, arraysize(kT1DnsName)),
                             kT1Qtype,
                             &actual_ip_addresses,
                             base::Bind(&TestRng1),
                             &factory);
  transaction.set_timeouts_ms(
      std::vector<base::TimeDelta>(kTimeoutsMs,
                                   kTimeoutsMs + arraysize(kTimeoutsMs)));

  TestCompletionCallback callback;
  int rv = transaction.Start(&callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();

  EXPECT_TRUE(socket1_data->at_read_eof());
  EXPECT_TRUE(socket1_data->at_write_eof());
  EXPECT_TRUE(socket2_data->at_read_eof());
  EXPECT_TRUE(socket2_data->at_write_eof());
  EXPECT_EQ(2u, factory.udp_client_sockets().size());

  EXPECT_TRUE(actual_ip_addresses == expected_ip_addresses);
}

// Test that after the first timeout we do a fresh connection, and after
// the second timeout we do another fresh connection, and if we get a
// response on the second connection, we return it.
TEST(DnsTransactionTest, SecondTimeoutTest) {
  MockWrite writes1[] = {
    MockWrite(true, reinterpret_cast<const char*>(kT1QueryDatagram),
              arraysize(kT1QueryDatagram))
  };

  MockRead reads1[] = {
    MockRead(true, reinterpret_cast<const char*>(kT1ResponseDatagram),
             arraysize(kT1ResponseDatagram))
  };

  scoped_refptr<DelayedSocketData> socket1_data(
      new DelayedSocketData(2, NULL, 0, writes1, arraysize(writes1)));
  scoped_refptr<DelayedSocketData> socket2_data(
      new DelayedSocketData(2, NULL, 0, writes1, arraysize(writes1)));
  scoped_refptr<DelayedSocketData> socket3_data(
      new DelayedSocketData(0, reads1, arraysize(reads1),
                            writes1, arraysize(writes1)));
  MockClientSocketFactory factory;
  factory.AddSocketDataProvider(socket1_data.get());
  factory.AddSocketDataProvider(socket2_data.get());
  factory.AddSocketDataProvider(socket3_data.get());

  std::vector<IPAddressNumber> expected_ip_addresses =
      CStringsToIPAddressList(kT1IpAddresses, arraysize(kT1IpAddresses));

  std::vector<IPAddressNumber> actual_ip_addresses;
  DnsTransaction transaction(CreateDnsAddress(kDnsIp, kDnsPort),
                             std::string(kT1DnsName, arraysize(kT1DnsName)),
                             kT1Qtype,
                             &actual_ip_addresses,
                             base::Bind(&TestRng1),
                             &factory);
  transaction.set_timeouts_ms(
      std::vector<base::TimeDelta>(kTimeoutsMs,
                                   kTimeoutsMs + arraysize(kTimeoutsMs)));

  TestCompletionCallback callback;
  int rv = transaction.Start(&callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();

  EXPECT_TRUE(socket1_data->at_read_eof());
  EXPECT_TRUE(socket1_data->at_write_eof());
  EXPECT_TRUE(socket2_data->at_read_eof());
  EXPECT_TRUE(socket2_data->at_write_eof());
  EXPECT_TRUE(socket3_data->at_read_eof());
  EXPECT_TRUE(socket3_data->at_write_eof());
  EXPECT_EQ(3u, factory.udp_client_sockets().size());

  EXPECT_TRUE(actual_ip_addresses == expected_ip_addresses);
}

// Test that after the first timeout we do a fresh connection, and after
// the second timeout we do another fresh connection and after the third
// timeout we give up and return a timeout error.
TEST(DnsTransactionTest, ThirdTimeoutTest) {
  MockWrite writes1[] = {
    MockWrite(true, reinterpret_cast<const char*>(kT1QueryDatagram),
              arraysize(kT1QueryDatagram))
  };

  scoped_refptr<DelayedSocketData> socket1_data(
      new DelayedSocketData(2, NULL, 0, writes1, arraysize(writes1)));
  scoped_refptr<DelayedSocketData> socket2_data(
      new DelayedSocketData(2, NULL, 0, writes1, arraysize(writes1)));
  scoped_refptr<DelayedSocketData> socket3_data(
      new DelayedSocketData(2, NULL, 0, writes1, arraysize(writes1)));
  MockClientSocketFactory factory;
  factory.AddSocketDataProvider(socket1_data.get());
  factory.AddSocketDataProvider(socket2_data.get());
  factory.AddSocketDataProvider(socket3_data.get());

  std::vector<IPAddressNumber> expected_ip_addresses =
      CStringsToIPAddressList(kT1IpAddresses, arraysize(kT1IpAddresses));

  std::vector<IPAddressNumber> actual_ip_addresses;
  DnsTransaction transaction(CreateDnsAddress(kDnsIp, kDnsPort),
                             std::string(kT1DnsName, arraysize(kT1DnsName)),
                             kT1Qtype,
                             &actual_ip_addresses,
                             base::Bind(&TestRng1),
                             &factory);
  transaction.set_timeouts_ms(
      std::vector<base::TimeDelta>(kTimeoutsMs,
                                   kTimeoutsMs + arraysize(kTimeoutsMs)));

  TestCompletionCallback callback;
  int rv = transaction.Start(&callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();

  EXPECT_TRUE(socket1_data->at_read_eof());
  EXPECT_TRUE(socket1_data->at_write_eof());
  EXPECT_TRUE(socket2_data->at_read_eof());
  EXPECT_TRUE(socket2_data->at_write_eof());
  EXPECT_TRUE(socket3_data->at_read_eof());
  EXPECT_TRUE(socket3_data->at_write_eof());
  EXPECT_EQ(3u, factory.udp_client_sockets().size());

  EXPECT_EQ(ERR_DNS_TIMED_OUT, rv);
}

}  // namespace net

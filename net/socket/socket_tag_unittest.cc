// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socket_tag.h"

#include "build/build_config.h"

#if defined(OS_ANDROID)
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

#include <inttypes.h>  // For SCNx64
#include <stdint.h>
#include <stdio.h>

#include <string>

#include "base/files/file_util.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/sockaddr_storage.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

#if defined(OS_ANDROID)
// Query the system to find out how many bytes were received with tag
// |expected_tag| for our UID.  Return the count of recieved bytes.
uint64_t GetTaggedBytes(int32_t expected_tag) {
  // To determine how many bytes the system saw with a particular tag read
  // the /proc/net/xt_qtaguid/stats file which contains the kernel's
  // dump of all the UIDs and their tags sent and received bytes.
  uint64_t bytes = 0;
  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(
      base::FilePath::FromUTF8Unsafe("/proc/net/xt_qtaguid/stats"), &contents));
  for (size_t i = contents.find('\n');  // Skip first line which is headers.
       i != std::string::npos && i < contents.length();) {
    uint64_t tag, rx_bytes;
    uid_t uid;
    int n;
    // Parse out the numbers we care about. For reference here's the column
    // headers:
    // idx iface acct_tag_hex uid_tag_int cnt_set rx_bytes rx_packets tx_bytes
    // tx_packets rx_tcp_bytes rx_tcp_packets rx_udp_bytes rx_udp_packets
    // rx_other_bytes rx_other_packets tx_tcp_bytes tx_tcp_packets tx_udp_bytes
    // tx_udp_packets tx_other_bytes tx_other_packets
    EXPECT_EQ(sscanf(contents.c_str() + i,
                     "%*d %*s 0x%" SCNx64 " %d %*d %" SCNu64
                     " %*d %*d %*d %*d %*d %*d %*d %*d "
                     "%*d %*d %*d %*d %*d %*d %*d%n",
                     &tag, &uid, &rx_bytes, &n),
              3);
    // If this line matches our UID and |expected_tag| then add it to the total.
    if (uid == getuid() && (int32_t)(tag >> 32) == expected_tag) {
      bytes += rx_bytes;
    }
    // Move |i| to the next line.
    i += n + 1;
  }
  return bytes;
}
#endif

}  // namespace

// Test that SocketTag's comparison function work.
TEST(SocketTagTest, Compares) {
  SocketTag unset1;
  SocketTag unset2;

  EXPECT_TRUE(unset1 == unset2);
  EXPECT_FALSE(unset1 != unset2);
  EXPECT_FALSE(unset1 < unset2);

#if defined(OS_ANDROID)
  SocketTag s00(0, 0), s01(0, 1), s11(1, 1);

  EXPECT_FALSE(s00 == unset1);
  EXPECT_TRUE(s01 != unset2);
  EXPECT_FALSE(unset1 < s00);
  EXPECT_TRUE(s00 < unset2);

  EXPECT_FALSE(s00 == s01);
  EXPECT_FALSE(s01 == s11);
  EXPECT_FALSE(s00 == s11);
  EXPECT_TRUE(s00 < s01);
  EXPECT_TRUE(s01 < s11);
  EXPECT_TRUE(s00 < s11);
  EXPECT_FALSE(s01 < s00);
  EXPECT_FALSE(s11 < s01);
  EXPECT_FALSE(s11 < s00);
#endif
}

// On Android, where socket tagging is supported, verify that SocketTag::Apply
// works as expected.
#if defined(OS_ANDROID)
TEST(SocketTagTest, Apply) {
  // Start test server.
  EmbeddedTestServer test_server;
  test_server.AddDefaultHandlers(base::FilePath());
  ASSERT_TRUE(test_server.Start());

  // Calculate sockaddr of test server.
  AddressList addr_list;
  ASSERT_TRUE(test_server.GetAddressList(&addr_list));
  SockaddrStorage addr;
  ASSERT_TRUE(addr_list[0].ToSockAddr(addr.addr, &addr.addr_len));

  // Create socket.
  int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  ASSERT_NE(s, -1);

  // Verify TCP connect packets are tagged and counted properly.
  int32_t tag_val1 = 0x12345678;
  uint64_t old_traffic = GetTaggedBytes(tag_val1);
  SocketTag tag1(SocketTag::UNSET_UID, tag_val1);
  tag1.Apply(s);
  ASSERT_EQ(connect(s, addr.addr, addr.addr_len), 0);
  EXPECT_GT(GetTaggedBytes(tag_val1), old_traffic);

  // Verify socket can be retagged and with our UID.
  int32_t tag_val2 = 0x87654321;
  old_traffic = GetTaggedBytes(tag_val2);
  SocketTag tag2(getuid(), tag_val2);
  tag2.Apply(s);
  const char kRequest1[] = "GET /";
  ASSERT_EQ(send(s, kRequest1, strlen(kRequest1), 0), (int)strlen(kRequest1));
  EXPECT_GT(GetTaggedBytes(tag_val2), old_traffic);

  // Verify socket can be retagged and with our UID.
  old_traffic = GetTaggedBytes(tag_val1);
  tag1.Apply(s);
  const char kRequest2[] = "\n\n";
  ASSERT_EQ(send(s, kRequest2, strlen(kRequest2), 0), (int)strlen(kRequest2));
  EXPECT_GT(GetTaggedBytes(tag_val1), old_traffic);

  ASSERT_EQ(close(s), 0);
}
#endif

}  // namespace net
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "fake_ppapi/fake_pepper_interface.h"
#include "gtest/gtest.h"
#include "nacl_io/kernel_intercept.h"

using namespace nacl_io;
using namespace sdk_util;

namespace {

class HostResolverTest : public ::testing::Test {
 public:
  HostResolverTest() : pepper_(NULL) {}

  void SetUp() {
    pepper_ = new FakePepperInterface();
    ki_init_interface(NULL, pepper_);
  }

  void TearDown() {
    ki_uninit();
    pepper_ = NULL;
  }

 protected:
  FakePepperInterface* pepper_;
};

}  // namespace

#define NULL_HOST (static_cast<hostent*>(NULL))

TEST_F(HostResolverTest, GethostbynameNumeric) {
  hostent* host = ki_gethostbyname(FAKE_HOSTNAME);

  // Verify the returned hostent structure
  ASSERT_NE(NULL_HOST, host);
  ASSERT_EQ(AF_INET, host->h_addrtype);
  ASSERT_EQ(sizeof(in_addr_t), host->h_length);
  ASSERT_STREQ(FAKE_HOSTNAME, host->h_name);

  in_addr_t** addr_list = reinterpret_cast<in_addr_t**>(host->h_addr_list);
  ASSERT_NE(reinterpret_cast<in_addr_t**>(NULL), addr_list);
  ASSERT_EQ(NULL, addr_list[1]);
  in_addr_t exptected_addr = htonl(FAKE_IP);
  ASSERT_EQ(exptected_addr, *addr_list[0]);
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_pool.h"

#include <string>
#include <vector>

#include "net/base/host_port_pair.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(ClientSocketPool, GroupIdOperators) {
  // Each of these lists is in "<" order, as defined by Group::operator< on the
  // corresponding field.

  // HostPortPair::operator< compares port before host.
  const HostPortPair kHostPortPairs[] = {
      {"b", 79}, {"a", 80}, {"b", 80}, {"c", 81}, {"a", 443}, {"c", 443},
  };

  const ClientSocketPool::SocketType kSocketTypes[] = {
      ClientSocketPool::SocketType::kHttp,
      ClientSocketPool::SocketType::kSsl,
      ClientSocketPool::SocketType::kSslVersionInterferenceProbe,
      ClientSocketPool::SocketType::kFtp,
  };

  const bool kPrivacyModes[] = {false, true};

  // All previously created |group_ids|. They should all be less than the
  // current group under consideration.
  std::vector<ClientSocketPool::GroupId> group_ids;

  // Iterate through all sets of group ids, from least to greatest.
  for (const auto& host_port_pair : kHostPortPairs) {
    SCOPED_TRACE(host_port_pair.ToString());
    for (const auto& socket_type : kSocketTypes) {
      SCOPED_TRACE(static_cast<int>(socket_type));
      for (const auto& privacy_mode : kPrivacyModes) {
        SCOPED_TRACE(privacy_mode);

        ClientSocketPool::GroupId group_id(host_port_pair, socket_type,
                                           privacy_mode);
        for (const auto& lower_group_id : group_ids) {
          EXPECT_FALSE(lower_group_id == group_id);
          EXPECT_TRUE(lower_group_id < group_id);
          EXPECT_FALSE(group_id < lower_group_id);
        }

        group_ids.push_back(group_id);

        // Compare |group_id| to itself. Use two different copies of
        // |group_id|'s value, since to protect against bugs where an object
        // only equals itself.
        EXPECT_TRUE(group_ids.back() == group_id);
        EXPECT_FALSE(group_ids.back() < group_id);
        EXPECT_FALSE(group_id < group_ids.back());
      }
    }
  }
}

TEST(ClientSocketPool, GroupIdToString) {
  EXPECT_EQ("foo:80",
            ClientSocketPool::GroupId(HostPortPair("foo", 80),
                                      ClientSocketPool::SocketType::kHttp,
                                      false /* privacy_mode */)
                .ToString());
  EXPECT_EQ("bar:443",
            ClientSocketPool::GroupId(HostPortPair("bar", 443),
                                      ClientSocketPool::SocketType::kHttp,
                                      false /* privacy_mode */)
                .ToString());
  EXPECT_EQ("pm/bar:80",
            ClientSocketPool::GroupId(HostPortPair("bar", 80),
                                      ClientSocketPool::SocketType::kHttp,
                                      true /* privacy_mode */)
                .ToString());

  EXPECT_EQ("ssl/foo:80",
            ClientSocketPool::GroupId(HostPortPair("foo", 80),
                                      ClientSocketPool::SocketType::kSsl,
                                      false /* privacy_mode */)
                .ToString());
  EXPECT_EQ("ssl/bar:443",
            ClientSocketPool::GroupId(HostPortPair("bar", 443),
                                      ClientSocketPool::SocketType::kSsl,
                                      false /* privacy_mode */)
                .ToString());
  EXPECT_EQ("pm/ssl/bar:80",
            ClientSocketPool::GroupId(HostPortPair("bar", 80),
                                      ClientSocketPool::SocketType::kSsl,
                                      true /* privacy_mode */)
                .ToString());

  EXPECT_EQ("version-interference-probe/ssl/foo:443",
            ClientSocketPool::GroupId(
                HostPortPair("foo", 443),
                ClientSocketPool::SocketType::kSslVersionInterferenceProbe,
                false /* privacy_mode */)
                .ToString());
  EXPECT_EQ("pm/version-interference-probe/ssl/bar:444",
            ClientSocketPool::GroupId(
                HostPortPair("bar", 444),
                ClientSocketPool::SocketType::kSslVersionInterferenceProbe,
                true /* privacy_mode */)
                .ToString());

  EXPECT_EQ("ftp/foo:80",
            ClientSocketPool::GroupId(HostPortPair("foo", 80),
                                      ClientSocketPool::SocketType::kFtp,
                                      false /* privacy_mode */)
                .ToString());
  EXPECT_EQ("pm/ftp/bar:81",
            ClientSocketPool::GroupId(HostPortPair("bar", 81),
                                      ClientSocketPool::SocketType::kFtp,
                                      true /* privacy_mode */)
                .ToString());
}

}  // namespace

}  // namespace net

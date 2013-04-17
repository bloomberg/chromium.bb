// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_mint_queue.h"

#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::IdentityMintRequestQueue;

namespace {

class MockRequest : public extensions::IdentityMintRequestQueue::Request {
 public:
  MOCK_METHOD1(StartMintToken, void(IdentityMintRequestQueue::MintType));
};

}  // namespace

TEST(IdentityMintQueueTest, SerialRequests) {
  IdentityMintRequestQueue::MintType type =
      IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE;
  IdentityMintRequestQueue queue;
  std::string extension_id("ext_id");
  MockRequest request1;
  MockRequest request2;

  EXPECT_CALL(request1, StartMintToken(type)).Times(1);
  queue.RequestStart(type, extension_id, std::set<std::string>(), &request1);
  queue.RequestComplete(type, extension_id, std::set<std::string>(), &request1);

  EXPECT_CALL(request2, StartMintToken(type)).Times(1);
  queue.RequestStart(type, extension_id, std::set<std::string>(), &request2);
  queue.RequestComplete(type, extension_id, std::set<std::string>(), &request2);
}

TEST(IdentityMintQueueTest, InteractiveType) {
  IdentityMintRequestQueue::MintType type =
      IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE;
  IdentityMintRequestQueue queue;
  std::string extension_id("ext_id");
  MockRequest request1;

  EXPECT_CALL(request1, StartMintToken(type)).Times(1);
  queue.RequestStart(type, extension_id, std::set<std::string>(), &request1);
  queue.RequestComplete(type, extension_id, std::set<std::string>(), &request1);
}

TEST(IdentityMintQueueTest, ParallelRequests) {
  IdentityMintRequestQueue::MintType type =
      IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE;
  IdentityMintRequestQueue queue;
  std::string extension_id("ext_id");
  MockRequest request1;
  MockRequest request2;
  MockRequest request3;

  EXPECT_CALL(request1, StartMintToken(type)).Times(1);
  queue.RequestStart(type, extension_id, std::set<std::string>(), &request1);
  queue.RequestStart(type, extension_id, std::set<std::string>(), &request2);
  queue.RequestStart(type, extension_id, std::set<std::string>(), &request3);

  EXPECT_CALL(request2, StartMintToken(type)).Times(1);
  queue.RequestComplete(type, extension_id, std::set<std::string>(), &request1);

  EXPECT_CALL(request3, StartMintToken(type)).Times(1);
  queue.RequestComplete(type, extension_id, std::set<std::string>(), &request2);

  queue.RequestComplete(type, extension_id, std::set<std::string>(), &request3);
}

TEST(IdentityMintQueueTest, ParallelRequestsFromTwoExtensions) {
  IdentityMintRequestQueue::MintType type =
      IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE;
  IdentityMintRequestQueue queue;
  std::string extension_id1("ext_id_1");
  std::string extension_id2("ext_id_2");
  MockRequest request1;
  MockRequest request2;

  EXPECT_CALL(request1, StartMintToken(type)).Times(1);
  EXPECT_CALL(request2, StartMintToken(type)).Times(1);
  queue.RequestStart(type, extension_id1, std::set<std::string>(), &request1);
  queue.RequestStart(type, extension_id2, std::set<std::string>(), &request2);

  queue.RequestComplete(type, extension_id1,
                        std::set<std::string>(), &request1);
  queue.RequestComplete(type, extension_id2,
                        std::set<std::string>(), &request2);
}

TEST(IdentityMintQueueTest, ParallelRequestsForDifferentScopes) {
  IdentityMintRequestQueue::MintType type =
      IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE;
  IdentityMintRequestQueue queue;
  std::string extension_id("ext_id");
  MockRequest request1;
  MockRequest request2;
  std::set<std::string> scopes1;
  std::set<std::string> scopes2;

  scopes1.insert("a");
  scopes1.insert("b");
  scopes2.insert("a");

  EXPECT_CALL(request1, StartMintToken(type)).Times(1);
  EXPECT_CALL(request2, StartMintToken(type)).Times(1);
  queue.RequestStart(type, extension_id, scopes1, &request1);
  queue.RequestStart(type, extension_id, scopes2, &request2);

  queue.RequestComplete(type, extension_id, scopes1, &request1);
  queue.RequestComplete(type, extension_id, scopes2, &request2);
}

TEST(IdentityMintQueueTest, KeyComparisons) {
  std::string extension_id1("ext_id_1");
  std::string extension_id2("ext_id_2");
  std::set<std::string> scopes1;
  std::set<std::string> scopes2;
  std::set<std::string> scopes3;

  scopes1.insert("a");
  scopes1.insert("b");
  scopes2.insert("a");

  std::vector<std::string> ids;
  ids.push_back(extension_id1);
  ids.push_back(extension_id2);

  std::vector<std::set<std::string> > scopesets;
  scopesets.push_back(scopes1);
  scopesets.push_back(scopes2);
  scopesets.push_back(scopes3);

  std::vector<IdentityMintRequestQueue::RequestKey> keys;
  typedef std::vector<
    IdentityMintRequestQueue::RequestKey>::const_iterator
      RequestKeyIterator;

  std::vector<std::string>::const_iterator id_it;
  std::vector<std::set<std::string> >::const_iterator scope_it;

  for (id_it = ids.begin(); id_it != ids.end(); ++id_it) {
    for (scope_it = scopesets.begin(); scope_it != scopesets.end();
         ++scope_it) {
      keys.push_back(IdentityMintRequestQueue::RequestKey(
          IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE, *id_it,
          *scope_it));
      keys.push_back(IdentityMintRequestQueue::RequestKey(
          IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE, *id_it, *scope_it));
    }
  }

  // keys should not be less than themselves
  for (RequestKeyIterator it = keys.begin(); it != keys.end(); ++it) {
    EXPECT_FALSE(*it < *it);
  }

  // keys should not equal different keys
  for (RequestKeyIterator it1 = keys.begin(); it1 != keys.end(); ++it1) {
    RequestKeyIterator it2 = it1;
    for (++it2; it2 != keys.end(); ++it2) {
      EXPECT_TRUE(*it1 < *it2 || *it2 < *it1);
      EXPECT_FALSE(*it1 < *it2 && *it2 < *it1);
    }
  }
}

TEST(IdentityMintQueueTest, Empty) {
  IdentityMintRequestQueue::MintType type =
      IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE;
  IdentityMintRequestQueue queue;
  std::string extension_id("ext_id");
  MockRequest request1;

  EXPECT_TRUE(queue.empty(type, extension_id, std::set<std::string>()));
  EXPECT_CALL(request1, StartMintToken(type)).Times(1);
  queue.RequestStart(type, extension_id, std::set<std::string>(), &request1);
  EXPECT_FALSE(queue.empty(type, extension_id, std::set<std::string>()));
  queue.RequestComplete(type, extension_id, std::set<std::string>(), &request1);
  EXPECT_TRUE(queue.empty(type, extension_id, std::set<std::string>()));
}

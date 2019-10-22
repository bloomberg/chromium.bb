// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/constants.h"

#include <unordered_map>

#include "absl/hash/hash.h"
#include "gtest/gtest.h"

namespace openscreen {
namespace discovery {

TEST(DnsSdConstantsTest, TestInstanceKeyEquals) {
  InstanceKey key1;
  key1.service_id = "service";
  key1.domain_id = "domain";
  key1.instance_id = "instance";
  InstanceKey key2;
  key2.service_id = "service";
  key2.domain_id = "domain";
  key2.instance_id = "instance";
  EXPECT_TRUE(key1 == key2);
  EXPECT_FALSE(key1 != key2);

  key1.service_id = "service2";
  EXPECT_FALSE(key1 == key2);
  EXPECT_TRUE(key1 != key2);
  key2.service_id = "service2";
  EXPECT_TRUE(key1 == key2);
  EXPECT_FALSE(key1 != key2);

  key1.domain_id = "domain2";
  EXPECT_FALSE(key1 == key2);
  EXPECT_TRUE(key1 != key2);
  key2.domain_id = "domain2";
  EXPECT_TRUE(key1 == key2);
  EXPECT_FALSE(key1 != key2);

  key1.instance_id = "instance2";
  EXPECT_FALSE(key1 == key2);
  EXPECT_TRUE(key1 != key2);
  key2.instance_id = "instance2";
  EXPECT_TRUE(key1 == key2);
  EXPECT_FALSE(key1 != key2);
}

TEST(DnsSdConstantsTest, TestServiceKeyEquals) {
  ServiceKey key1;
  key1.service_id = "service";
  key1.domain_id = "domain";
  ServiceKey key2;
  key2.service_id = "service";
  key2.domain_id = "domain";
  EXPECT_TRUE(key1 == key2);
  EXPECT_FALSE(key1 != key2);

  key1.service_id = "service2";
  EXPECT_FALSE(key1 == key2);
  EXPECT_TRUE(key1 != key2);
  key2.service_id = "service2";
  EXPECT_TRUE(key1 == key2);
  EXPECT_FALSE(key1 != key2);

  key1.domain_id = "domain2";
  EXPECT_FALSE(key1 == key2);
  EXPECT_TRUE(key1 != key2);
  key2.domain_id = "domain2";
  EXPECT_TRUE(key1 == key2);
  EXPECT_FALSE(key1 != key2);
}

TEST(DnsSdConstantsTest, TestIsInstanceOf) {
  ServiceKey ptr;
  ptr.service_id = "service";
  ptr.domain_id = "domain";
  InstanceKey svc;
  svc.service_id = "service";
  svc.domain_id = "domain";
  svc.instance_id = "instance";
  EXPECT_TRUE(IsInstanceOf(ptr, svc));

  svc.instance_id = "other id";
  EXPECT_TRUE(IsInstanceOf(ptr, svc));

  svc.domain_id = "domain2";
  EXPECT_FALSE(IsInstanceOf(ptr, svc));
  ptr.domain_id = "domain2";
  EXPECT_TRUE(IsInstanceOf(ptr, svc));

  svc.service_id = "service2";
  EXPECT_FALSE(IsInstanceOf(ptr, svc));
  ptr.service_id = "service2";
  EXPECT_TRUE(IsInstanceOf(ptr, svc));
}

TEST(DnsSdConstantsTest, ServiceKeyInMap) {
  ServiceKey ptr{"service", "domain"};
  ServiceKey ptr2{"service", "domain"};
  ServiceKey ptr3{"service2", "domain"};
  ServiceKey ptr4{"service", "domain2"};
  std::unordered_map<ServiceKey, int32_t, absl::Hash<ServiceKey>> map;
  map[ptr] = 1;
  map[ptr2] = 2;
  EXPECT_EQ(map[ptr], 2);

  map[ptr3] = 3;
  EXPECT_EQ(map[ptr], 2);
  EXPECT_EQ(map[ptr3], 3);

  map[ptr4] = 4;
  EXPECT_EQ(map[ptr], 2);
  EXPECT_EQ(map[ptr3], 3);
  EXPECT_EQ(map[ptr4], 4);
}

TEST(DnsSdConstantsTest, InstanceKeyInMap) {
  InstanceKey key{"instance", "service", "domain"};
  InstanceKey key2{"instance", "service", "domain"};
  InstanceKey key3{"instance2", "service", "domain"};
  InstanceKey key4{"instance", "service2", "domain"};
  InstanceKey key5{"instance", "service", "domain2"};
  std::unordered_map<InstanceKey, int32_t, absl::Hash<InstanceKey>> map;
  map[key] = 1;
  map[key2] = 2;
  EXPECT_EQ(map[key], 2);

  map[key3] = 3;
  EXPECT_EQ(map[key], 2);
  EXPECT_EQ(map[key3], 3);

  map[key4] = 4;
  EXPECT_EQ(map[key], 2);
  EXPECT_EQ(map[key3], 3);
  EXPECT_EQ(map[key4], 4);

  map[key5] = 5;
  EXPECT_EQ(map[key], 2);
  EXPECT_EQ(map[key3], 3);
  EXPECT_EQ(map[key4], 4);
  EXPECT_EQ(map[key5], 5);
}

}  // namespace discovery
}  // namespace openscreen

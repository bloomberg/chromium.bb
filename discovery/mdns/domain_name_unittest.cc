// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/domain_name.h"

#include <sstream>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace openscreen {
namespace mdns {

namespace {

bool FromLabels(const std::vector<std::string>& labels, DomainName* result) {
  return DomainName::FromLabels(labels.begin(), labels.end(), result);
}

}  // namespace

TEST(DomainNameTest, Constructors) {
  DomainName empty;

  ASSERT_EQ(1u, empty.domain_name().size());
  EXPECT_EQ(0, empty.domain_name()[0]);

  DomainName original({10, 'o', 'p', 'e', 'n', 's', 'c', 'r', 'e', 'e', 'n', 3,
                       'o', 'r', 'g', 0});
  ASSERT_EQ(16u, original.domain_name().size());

  auto data_copy = original.domain_name();
  DomainName direct_ctor(std::move(data_copy));
  EXPECT_EQ(direct_ctor.domain_name(), original.domain_name());

  DomainName copy_ctor(original);
  EXPECT_EQ(copy_ctor.domain_name(), original.domain_name());

  DomainName move_ctor(std::move(copy_ctor));
  EXPECT_EQ(move_ctor.domain_name(), original.domain_name());

  DomainName copy_assign;
  copy_assign = move_ctor;
  EXPECT_EQ(copy_assign.domain_name(), original.domain_name());

  DomainName move_assign;
  move_assign = std::move(move_ctor);
  EXPECT_EQ(move_assign.domain_name(), original.domain_name());
}

TEST(DomainNameTest, FromLabels) {
  const auto typical =
      std::vector<uint8_t>{10,  'o', 'p', 'e', 'n', 's', 'c', 'r',
                           'e', 'e', 'n', 3,   'o', 'r', 'g', 0};
  DomainName result;
  ASSERT_TRUE(FromLabels({"openscreen", "org"}, &result));
  EXPECT_EQ(result.domain_name(), typical);

  const auto includes_dot =
      std::vector<uint8_t>{11,  'o', 'p', 'e', 'n', '.', 's', 'c', 'r',
                           'e', 'e', 'n', 3,   'o', 'r', 'g', 0};
  ASSERT_TRUE(FromLabels({"open.screen", "org"}, &result));
  EXPECT_EQ(result.domain_name(), includes_dot);

  const auto includes_non_ascii =
      std::vector<uint8_t>{11,  'o', 'p', 'e', 'n', 7,   's', 'c', 'r',
                           'e', 'e', 'n', 3,   'o', 'r', 'g', 0};
  ASSERT_TRUE(FromLabels({"open\7screen", "org"}, &result));
  EXPECT_EQ(result.domain_name(), includes_non_ascii);

  ASSERT_FALSE(FromLabels({"extremely-long-label-that-is-actually-too-long-"
                           "for-rfc-1034-and-will-not-generate"},
                          &result));

  ASSERT_FALSE(FromLabels(
      {
          "extremely-long-domain-name-that-is-made-of",
          "valid-labels",
          "however-overall-it-is-too-long-for-rfc-1034",
          "so-it-should-fail-to-generate",
          "filler-filler-filler-filler-filler",
          "filler-filler-filler-filler-filler",
          "filler-filler-filler-filler-filler",
          "filler-filler-filler-filler-filler",
      },
      &result));
}

TEST(DomainNameTest, Equality) {
  DomainName alpha;
  DomainName beta;
  DomainName alpha_copy;

  ASSERT_TRUE(FromLabels({"alpha", "openscreen", "org"}, &alpha));
  ASSERT_TRUE(FromLabels({"beta", "openscreen", "org"}, &beta));
  alpha_copy = alpha;

  EXPECT_TRUE(alpha == alpha);
  EXPECT_FALSE(alpha != alpha);
  EXPECT_TRUE(alpha == alpha_copy);
  EXPECT_FALSE(alpha != alpha_copy);
  EXPECT_FALSE(alpha == beta);
  EXPECT_TRUE(alpha != beta);
}

TEST(DomainNameTest, EndsWithLocalDomain) {
  DomainName alpha;
  DomainName beta;

  EXPECT_FALSE(alpha.EndsWithLocalDomain());

  ASSERT_TRUE(FromLabels({"alpha", "openscreen", "org"}, &alpha));
  ASSERT_TRUE(FromLabels({"beta", "local"}, &beta));

  EXPECT_FALSE(alpha.EndsWithLocalDomain());
  EXPECT_TRUE(beta.EndsWithLocalDomain());
}

TEST(DomainNameTest, IsEmpty) {
  DomainName alpha;
  DomainName beta(std::vector<uint8_t>{0});

  EXPECT_TRUE(alpha.IsEmpty());
  EXPECT_TRUE(beta.IsEmpty());

  ASSERT_TRUE(FromLabels({"alpha", "openscreen", "org"}, &alpha));
  EXPECT_FALSE(alpha.IsEmpty());
}

TEST(DomainNameTest, Append) {
  const auto expected_service_type =
      std::vector<uint8_t>{11,  '_', 'o', 'p', 'e', 'n', 's', 'c', 'r', 'e',
                           'e', 'n', 5,   '_', 'q', 'u', 'i', 'c', 0};
  const auto total_expected = std::vector<uint8_t>{
      5,   'a', 'l', 'p', 'h', 'a', 11,  '_', 'o', 'p', 'e', 'n', 's',
      'c', 'r', 'e', 'e', 'n', 5,   '_', 'q', 'u', 'i', 'c', 0};
  DomainName service_name;
  DomainName service_type;
  DomainName protocol;
  ASSERT_TRUE(FromLabels({"alpha"}, &service_name));
  ASSERT_TRUE(FromLabels({"_openscreen"}, &service_type));
  ASSERT_TRUE(FromLabels({"_quic"}, &protocol));

  EXPECT_TRUE(service_type.Append(protocol));
  EXPECT_EQ(service_type.domain_name(), expected_service_type);

  DomainName result;
  EXPECT_TRUE(DomainName::Append(service_name, service_type, &result));
  EXPECT_EQ(result.domain_name(), total_expected);
}

TEST(DomainNameTest, GetLabels) {
  const auto labels = std::vector<std::string>{"alpha", "beta", "gamma", "org"};
  DomainName d;
  ASSERT_TRUE(FromLabels(labels, &d));
  EXPECT_EQ(d.GetLabels(), labels);
}

TEST(DomainNameTest, StreamEscaping) {
  {
    std::stringstream ss;
    ss << DomainName(std::vector<uint8_t>{1, 0, 0});
    EXPECT_EQ(ss.str(), "\\x00.");
  }
  {
    std::stringstream ss;
    ss << DomainName(std::vector<uint8_t>{1, 1, 0});
    EXPECT_EQ(ss.str(), "\\x01.");
  }
  {
    std::stringstream ss;
    ss << DomainName(std::vector<uint8_t>{1, 18, 0});
    EXPECT_EQ(ss.str(), "\\x12.");
  }
  {
    std::stringstream ss;
    ss << DomainName(std::vector<uint8_t>{1, 255, 0});
    EXPECT_EQ(ss.str(), "\\xff.");
  }
}

}  // namespace mdns
}  // namespace openscreen

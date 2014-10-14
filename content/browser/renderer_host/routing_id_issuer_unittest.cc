// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/routing_id_issuer.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

const int kMangler = 5;

class RoutingIDIssuerTest : public testing::Test {};

TEST_F(RoutingIDIssuerTest, IssueNext) {
  scoped_ptr<RoutingIDIssuer> target =
      RoutingIDIssuer::CreateWithMangler(kMangler);
  int id0 = target->IssueNext();
  int id1 = target->IssueNext();
  EXPECT_EQ(id0 + 1, id1);
}

TEST_F(RoutingIDIssuerTest, IsProbablyValid) {
  scoped_ptr<RoutingIDIssuer> proper =
      RoutingIDIssuer::CreateWithMangler(kMangler);
  scoped_ptr<RoutingIDIssuer> foreign =
      RoutingIDIssuer::CreateWithMangler(kMangler + 1);

  EXPECT_FALSE(proper->IsProbablyValid(kMangler));

  int proper_id = proper->IssueNext();
  int foreign_id = foreign->IssueNext();
  EXPECT_TRUE(proper->IsProbablyValid(proper_id));
  EXPECT_TRUE(foreign->IsProbablyValid(foreign_id));
  EXPECT_FALSE(proper->IsProbablyValid(foreign_id));
  EXPECT_FALSE(foreign->IsProbablyValid(proper_id));
}

}  // namespace
}  // namespace contnet

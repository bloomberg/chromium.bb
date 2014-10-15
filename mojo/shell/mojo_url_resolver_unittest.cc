// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/mojo_url_resolver.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace shell {
namespace test {
namespace {

typedef testing::Test MojoURLResolverTest;

TEST_F(MojoURLResolverTest, MojoURLsFallThrough) {
  MojoURLResolver resolver;
  resolver.AddCustomMapping(GURL("mojo:test"), GURL("mojo:foo"));
  const GURL base_url("file:/base");
  resolver.SetBaseURL(base_url);
  const std::string resolved(resolver.Resolve(GURL("mojo:test")).spec());
  // Resolved must start with |base_url|.
  EXPECT_EQ(base_url.spec(), resolved.substr(0, base_url.spec().size()));
  // And must contain foo (which is what test mapped to.
  EXPECT_NE(std::string::npos, resolved.find("foo"));
}

}  // namespace
}  // namespace test
}  // namespace shell
}  // namespace mojo

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/fetcher/url_resolver.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "mojo/util/filename_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace shell {
namespace test {
namespace {

typedef testing::Test URLResolverTest;

TEST_F(URLResolverTest, TestQueryForBaseFileURL) {
  URLResolver resolver(GURL("file:///base"));
  GURL mapped_url = resolver.ResolveMojoURL(GURL("mojo:foo?a=b"));
  EXPECT_EQ("file:///base/foo/foo.mojo?a=b", mapped_url.spec());
}

TEST_F(URLResolverTest, TestQueryForBaseHttpURL) {
  URLResolver resolver(GURL("http://127.0.0.1:1234"));
  GURL mapped_url = resolver.ResolveMojoURL(GURL("mojo:foo?a=b"));
  EXPECT_EQ("http://127.0.0.1:1234/foo/foo.mojo?a=b", mapped_url.spec());
}

TEST_F(URLResolverTest, TestManifest) {
  URLResolver resolver(GURL("file:///base"));
  {
    GURL mapped_url = resolver.ResolveMojoManifest(GURL("mojo:foo"));
    EXPECT_EQ("file:///base/foo/manifest.json", mapped_url.spec());
  }
  {
    GURL mapped_url = resolver.ResolveMojoManifest(GURL("exe:foo"));
    EXPECT_EQ("file:///base/foo_manifest.json", mapped_url.spec());
  }
  {
    GURL mapped_url = resolver.ResolveMojoManifest(GURL("http://localhost/"));
    EXPECT_TRUE(mapped_url.is_empty());
  }
}

}  // namespace
}  // namespace test
}  // namespace shell
}  // namespace mojo

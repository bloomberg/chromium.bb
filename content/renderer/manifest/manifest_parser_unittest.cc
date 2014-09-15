// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/manifest/manifest_parser.h"

#include "base/strings/string_util.h"
#include "content/public/common/manifest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(ManifestParserTest, EmptyStringNull) {
  Manifest manifest = ManifestParser::Parse("");

  // A parsing error is equivalent to an empty manifest.
  ASSERT_TRUE(manifest.IsEmpty());
  ASSERT_TRUE(manifest.name.is_null());
  ASSERT_TRUE(manifest.short_name.is_null());
}

TEST(ManifestParserTest, ValidNoContentParses) {
  Manifest manifest = ManifestParser::Parse("{}");

  // Check that all the fields are null in that case.
  ASSERT_TRUE(manifest.IsEmpty());
  ASSERT_TRUE(manifest.name.is_null());
  ASSERT_TRUE(manifest.short_name.is_null());
}

TEST(ManifestParserTest, NameParseRules) {
  // Smoke test.
  {
    Manifest manifest = ManifestParser::Parse("{ \"name\": \"foo\" }");
    ASSERT_TRUE(EqualsASCII(manifest.name.string(), "foo"));
  }

  // Trim whitespaces.
  {
    Manifest manifest = ManifestParser::Parse("{ \"name\": \"  foo  \" }");
    ASSERT_TRUE(EqualsASCII(manifest.name.string(), "foo"));
  }

  // Don't parse if name isn't a string.
  {
    Manifest manifest = ManifestParser::Parse("{ \"name\": {} }");
    ASSERT_TRUE(manifest.name.is_null());
  }

  // Don't parse if name isn't a string.
  {
    Manifest manifest = ManifestParser::Parse("{ \"name\": 42 }");
    ASSERT_TRUE(manifest.name.is_null());
  }
}

TEST(ManifestParserTest, ShortNameParseRules) {
  // Smoke test.
  {
    Manifest manifest = ManifestParser::Parse("{ \"short_name\": \"foo\" }");
    ASSERT_TRUE(EqualsASCII(manifest.short_name.string(), "foo"));
  }

  // Trim whitespaces.
  {
    Manifest manifest =
        ManifestParser::Parse("{ \"short_name\": \"  foo  \" }");
    ASSERT_TRUE(EqualsASCII(manifest.short_name.string(), "foo"));
  }

  // Don't parse if name isn't a string.
  {
    Manifest manifest = ManifestParser::Parse("{ \"short_name\": {} }");
    ASSERT_TRUE(manifest.short_name.is_null());
  }

  // Don't parse if name isn't a string.
  {
    Manifest manifest = ManifestParser::Parse("{ \"short_name\": 42 }");
    ASSERT_TRUE(manifest.short_name.is_null());
  }
}

} // namespace content

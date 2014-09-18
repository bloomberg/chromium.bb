// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/manifest/manifest_parser.h"

#include "base/strings/string_util.h"
#include "content/public/common/manifest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class ManifestParserTest : public testing::Test  {
 protected:
  ManifestParserTest() {}
  virtual ~ManifestParserTest() {}

  Manifest ParseManifest(const base::StringPiece& json,
                         const GURL& document_url = default_document_url,
                         const GURL& manifest_url = default_manifest_url) {
    return ManifestParser::Parse(json, document_url, manifest_url);
  }

  static const GURL default_document_url;
  static const GURL default_manifest_url;

 private:
  DISALLOW_COPY_AND_ASSIGN(ManifestParserTest);
};

const GURL ManifestParserTest::default_document_url(
    "http://foo.com/index.html");
const GURL ManifestParserTest::default_manifest_url(
    "http://foo.com/manifest.json");

TEST_F(ManifestParserTest, EmptyStringNull) {
  Manifest manifest = ParseManifest("");

  // A parsing error is equivalent to an empty manifest.
  ASSERT_TRUE(manifest.IsEmpty());
  ASSERT_TRUE(manifest.name.is_null());
  ASSERT_TRUE(manifest.short_name.is_null());
  ASSERT_TRUE(manifest.start_url.is_empty());
  ASSERT_EQ(manifest.display, Manifest::DISPLAY_MODE_UNSPECIFIED);
  ASSERT_EQ(manifest.orientation, blink::WebScreenOrientationLockDefault);
}

TEST_F(ManifestParserTest, ValidNoContentParses) {
  Manifest manifest = ParseManifest("{}");

  // Check that all the fields are null in that case.
  ASSERT_TRUE(manifest.IsEmpty());
  ASSERT_TRUE(manifest.name.is_null());
  ASSERT_TRUE(manifest.short_name.is_null());
  ASSERT_TRUE(manifest.start_url.is_empty());
  ASSERT_EQ(manifest.display, Manifest::DISPLAY_MODE_UNSPECIFIED);
  ASSERT_EQ(manifest.orientation, blink::WebScreenOrientationLockDefault);
}

TEST_F(ManifestParserTest, NameParseRules) {
  // Smoke test.
  {
    Manifest manifest = ParseManifest("{ \"name\": \"foo\" }");
    ASSERT_TRUE(EqualsASCII(manifest.name.string(), "foo"));
    ASSERT_FALSE(manifest.IsEmpty());
  }

  // Trim whitespaces.
  {
    Manifest manifest = ParseManifest("{ \"name\": \"  foo  \" }");
    ASSERT_TRUE(EqualsASCII(manifest.name.string(), "foo"));
  }

  // Don't parse if name isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"name\": {} }");
    ASSERT_TRUE(manifest.name.is_null());
  }

  // Don't parse if name isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"name\": 42 }");
    ASSERT_TRUE(manifest.name.is_null());
  }
}

TEST_F(ManifestParserTest, ShortNameParseRules) {
  // Smoke test.
  {
    Manifest manifest = ParseManifest("{ \"short_name\": \"foo\" }");
    ASSERT_TRUE(EqualsASCII(manifest.short_name.string(), "foo"));
    ASSERT_FALSE(manifest.IsEmpty());
  }

  // Trim whitespaces.
  {
    Manifest manifest = ParseManifest("{ \"short_name\": \"  foo  \" }");
    ASSERT_TRUE(EqualsASCII(manifest.short_name.string(), "foo"));
  }

  // Don't parse if name isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"short_name\": {} }");
    ASSERT_TRUE(manifest.short_name.is_null());
  }

  // Don't parse if name isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"short_name\": 42 }");
    ASSERT_TRUE(manifest.short_name.is_null());
  }
}

TEST_F(ManifestParserTest, StartURLParseRules) {
  // Smoke test.
  {
    Manifest manifest = ParseManifest("{ \"start_url\": \"land.html\" }");
    ASSERT_EQ(manifest.start_url.spec(),
              default_document_url.Resolve("land.html").spec());
    ASSERT_FALSE(manifest.IsEmpty());
  }

  // Whitespaces.
  {
    Manifest manifest = ParseManifest("{ \"start_url\": \"  land.html  \" }");
    ASSERT_EQ(manifest.start_url.spec(),
              default_document_url.Resolve("land.html").spec());
  }

  // Don't parse if property isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"start_url\": {} }");
    ASSERT_TRUE(manifest.start_url.is_empty());
  }

  // Don't parse if property isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"start_url\": 42 }");
    ASSERT_TRUE(manifest.start_url.is_empty());
  }

  // Absolute start_url, same origin with document.
  {
    Manifest manifest =
        ParseManifest("{ \"start_url\": \"http://foo.com/land.html\" }",
                      GURL("http://foo.com/manifest.json"),
                      GURL("http://foo.com/index.html"));
    ASSERT_EQ(manifest.start_url.spec(), "http://foo.com/land.html");
  }

  // Absolute start_url, cross origin with document.
  {
    Manifest manifest =
        ParseManifest("{ \"start_url\": \"http://bar.com/land.html\" }",
                      GURL("http://foo.com/manifest.json"),
                      GURL("http://foo.com/index.html"));
    ASSERT_TRUE(manifest.start_url.is_empty());
  }

  // Resolving has to happen based on the manifest_url.
  {
    Manifest manifest =
        ParseManifest("{ \"start_url\": \"land.html\" }",
                      GURL("http://foo.com/landing/manifest.json"),
                      GURL("http://foo.com/index.html"));
    ASSERT_EQ(manifest.start_url.spec(), "http://foo.com/landing/land.html");
  }
}

TEST_F(ManifestParserTest, DisplayParserRules) {
  // Smoke test.
  {
    Manifest manifest = ParseManifest("{ \"display\": \"browser\" }");
    EXPECT_EQ(manifest.display, Manifest::DISPLAY_MODE_BROWSER);
    EXPECT_FALSE(manifest.IsEmpty());
  }

  // Trim whitespaces.
  {
    Manifest manifest = ParseManifest("{ \"display\": \"  browser  \" }");
    EXPECT_EQ(manifest.display, Manifest::DISPLAY_MODE_BROWSER);
  }

  // Don't parse if name isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"display\": {} }");
    EXPECT_EQ(manifest.display, Manifest::DISPLAY_MODE_UNSPECIFIED);
  }

  // Don't parse if name isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"display\": 42 }");
    EXPECT_EQ(manifest.display, Manifest::DISPLAY_MODE_UNSPECIFIED);
  }

  // Parse fails if string isn't known.
  {
    Manifest manifest = ParseManifest("{ \"display\": \"browser_something\" }");
    EXPECT_EQ(manifest.display, Manifest::DISPLAY_MODE_UNSPECIFIED);
  }

  // Accept 'fullscreen'.
  {
    Manifest manifest = ParseManifest("{ \"display\": \"fullscreen\" }");
    EXPECT_EQ(manifest.display, Manifest::DISPLAY_MODE_FULLSCREEN);
  }

  // Accept 'fullscreen'.
  {
    Manifest manifest = ParseManifest("{ \"display\": \"standalone\" }");
    EXPECT_EQ(manifest.display, Manifest::DISPLAY_MODE_STANDALONE);
  }

  // Accept 'minimal-ui'.
  {
    Manifest manifest = ParseManifest("{ \"display\": \"minimal-ui\" }");
    EXPECT_EQ(manifest.display, Manifest::DISPLAY_MODE_MINIMAL_UI);
  }

  // Accept 'browser'.
  {
    Manifest manifest = ParseManifest("{ \"display\": \"browser\" }");
    EXPECT_EQ(manifest.display, Manifest::DISPLAY_MODE_BROWSER);
  }

  // Case insensitive.
  {
    Manifest manifest = ParseManifest("{ \"display\": \"BROWSER\" }");
    EXPECT_EQ(manifest.display, Manifest::DISPLAY_MODE_BROWSER);
  }
}

TEST_F(ManifestParserTest, OrientationParserRules) {
  // Smoke test.
  {
    Manifest manifest = ParseManifest("{ \"orientation\": \"natural\" }");
    EXPECT_EQ(manifest.orientation, blink::WebScreenOrientationLockNatural);
    EXPECT_FALSE(manifest.IsEmpty());
  }

  // Trim whitespaces.
  {
    Manifest manifest = ParseManifest("{ \"orientation\": \"natural\" }");
    EXPECT_EQ(manifest.orientation, blink::WebScreenOrientationLockNatural);
  }

  // Don't parse if name isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"orientation\": {} }");
    EXPECT_EQ(manifest.orientation, blink::WebScreenOrientationLockDefault);
  }

  // Don't parse if name isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"orientation\": 42 }");
    EXPECT_EQ(manifest.orientation, blink::WebScreenOrientationLockDefault);
  }

  // Parse fails if string isn't known.
  {
    Manifest manifest = ParseManifest("{ \"orientation\": \"naturalish\" }");
    EXPECT_EQ(manifest.orientation, blink::WebScreenOrientationLockDefault);
  }

  // Accept 'any'.
  {
    Manifest manifest = ParseManifest("{ \"orientation\": \"any\" }");
    EXPECT_EQ(manifest.orientation, blink::WebScreenOrientationLockAny);
  }

  // Accept 'natural'.
  {
    Manifest manifest = ParseManifest("{ \"orientation\": \"natural\" }");
    EXPECT_EQ(manifest.orientation, blink::WebScreenOrientationLockNatural);
  }

  // Accept 'landscape'.
  {
    Manifest manifest = ParseManifest("{ \"orientation\": \"landscape\" }");
    EXPECT_EQ(manifest.orientation, blink::WebScreenOrientationLockLandscape);
  }

  // Accept 'landscape-primary'.
  {
    Manifest manifest =
        ParseManifest("{ \"orientation\": \"landscape-primary\" }");
    EXPECT_EQ(manifest.orientation,
              blink::WebScreenOrientationLockLandscapePrimary);
  }

  // Accept 'landscape-secondary'.
  {
    Manifest manifest =
        ParseManifest("{ \"orientation\": \"landscape-secondary\" }");
    EXPECT_EQ(manifest.orientation,
              blink::WebScreenOrientationLockLandscapeSecondary);
  }

  // Accept 'portrait'.
  {
    Manifest manifest = ParseManifest("{ \"orientation\": \"portrait\" }");
    EXPECT_EQ(manifest.orientation, blink::WebScreenOrientationLockPortrait);
  }

  // Accept 'portrait-primary'.
  {
    Manifest manifest =
      ParseManifest("{ \"orientation\": \"portrait-primary\" }");
    EXPECT_EQ(manifest.orientation,
              blink::WebScreenOrientationLockPortraitPrimary);
  }

  // Accept 'portrait-secondary'.
  {
    Manifest manifest =
        ParseManifest("{ \"orientation\": \"portrait-secondary\" }");
    EXPECT_EQ(manifest.orientation,
              blink::WebScreenOrientationLockPortraitSecondary);
  }

  // Case insensitive.
  {
    Manifest manifest = ParseManifest("{ \"orientation\": \"LANDSCAPE\" }");
    EXPECT_EQ(manifest.orientation, blink::WebScreenOrientationLockLandscape);
  }
}

} // namespace content

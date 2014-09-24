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

TEST_F(ManifestParserTest, IconsParseRules) {
  // Smoke test: if no icon, empty list.
  {
    Manifest manifest = ParseManifest("{ \"icons\": [] }");
    EXPECT_EQ(manifest.icons.size(), 0u);
    EXPECT_TRUE(manifest.IsEmpty());
  }

  // Smoke test: if empty icon, empty list.
  {
    Manifest manifest = ParseManifest("{ \"icons\": [ {} ] }");
    EXPECT_EQ(manifest.icons.size(), 0u);
    EXPECT_TRUE(manifest.IsEmpty());
  }

  // Smoke test: icon with invalid src, empty list.
  {
    Manifest manifest = ParseManifest("{ \"icons\": [ { \"icons\": [] } ] }");
    EXPECT_EQ(manifest.icons.size(), 0u);
    EXPECT_TRUE(manifest.IsEmpty());
  }

  // Smoke test: if icon with empty src, it will be present in the list.
  {
    Manifest manifest = ParseManifest("{ \"icons\": [ { \"src\": \"\" } ] }");
    EXPECT_EQ(manifest.icons.size(), 1u);
    EXPECT_EQ(manifest.icons[0].src.spec(), "http://foo.com/index.html");
    EXPECT_FALSE(manifest.IsEmpty());
  }

  // Smoke test: if one icons with valid src, it will be present in the list.
  {
    Manifest manifest =
        ParseManifest("{ \"icons\": [{ \"src\": \"foo.jpg\" }] }");
    EXPECT_EQ(manifest.icons.size(), 1u);
    EXPECT_EQ(manifest.icons[0].src.spec(), "http://foo.com/foo.jpg");
    EXPECT_FALSE(manifest.IsEmpty());
  }
}

TEST_F(ManifestParserTest, IconSrcParseRules) {
  // Smoke test.
  {
    Manifest manifest =
        ParseManifest("{ \"icons\": [ {\"src\": \"foo.png\" } ] }");
    EXPECT_EQ(manifest.icons[0].src.spec(),
              default_document_url.Resolve("foo.png").spec());
  }

  // Whitespaces.
  {
    Manifest manifest =
        ParseManifest("{ \"icons\": [ {\"src\": \"   foo.png   \" } ] }");
    EXPECT_EQ(manifest.icons[0].src.spec(),
              default_document_url.Resolve("foo.png").spec());
  }

  // Don't parse if property isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"icons\": [ {\"src\": {} } ] }");
    EXPECT_TRUE(manifest.icons.empty());
  }

  // Don't parse if property isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"icons\": [ {\"src\": 42 } ] }");
    EXPECT_TRUE(manifest.icons.empty());
  }

  // Resolving has to happen based on the document_url.
  {
    Manifest manifest =
        ParseManifest("{ \"icons\": [ {\"src\": \"icons/foo.png\" } ] }",
                      GURL("http://foo.com/landing/index.html"));
    EXPECT_EQ(manifest.icons[0].src.spec(),
              "http://foo.com/landing/icons/foo.png");
  }
}

TEST_F(ManifestParserTest, IconTypeParseRules) {
  // Smoke test.
  {
    Manifest manifest =
        ParseManifest("{ \"icons\": [ {\"src\": \"\", \"type\": \"foo\" } ] }");
    EXPECT_TRUE(EqualsASCII(manifest.icons[0].type.string(), "foo"));
  }

  // Trim whitespaces.
  {
    Manifest manifest = ParseManifest("{ \"icons\": [ {\"src\": \"\","
                                      " \"type\": \"  foo  \" } ] }");
    EXPECT_TRUE(EqualsASCII(manifest.icons[0].type.string(), "foo"));
  }

  // Don't parse if property isn't a string.
  {
    Manifest manifest =
        ParseManifest("{ \"icons\": [ {\"src\": \"\", \"type\": {} } ] }");
    EXPECT_TRUE(manifest.icons[0].type.is_null());
  }

  // Don't parse if property isn't a string.
  {
    Manifest manifest =
        ParseManifest("{ \"icons\": [ {\"src\": \"\", \"type\": 42 } ] }");
    EXPECT_TRUE(manifest.icons[0].type.is_null());
  }
}

TEST_F(ManifestParserTest, IconDensityParseRules) {
  // Smoke test.
  {
    Manifest manifest =
        ParseManifest("{ \"icons\": [ {\"src\": \"\", \"density\": 42 } ] }");
    EXPECT_EQ(manifest.icons[0].density, 42);
  }

  // Decimal value.
  {
    Manifest manifest =
        ParseManifest("{ \"icons\": [ {\"src\": \"\", \"density\": 2.5 } ] }");
    EXPECT_EQ(manifest.icons[0].density, 2.5);
  }

  // Parse fail if it isn't a float.
  {
    Manifest manifest =
        ParseManifest("{ \"icons\": [ {\"src\": \"\", \"density\": {} } ] }");
    EXPECT_EQ(manifest.icons[0].density, Manifest::Icon::kDefaultDensity);
  }

  // Parse fail if it isn't a float.
  {
    Manifest manifest =
        ParseManifest("{ \"icons\": [ {\"src\": \"\", \"density\":\"2\" } ] }");
    EXPECT_EQ(manifest.icons[0].density, Manifest::Icon::kDefaultDensity);
  }

  // Edge case: 1.0.
  {
    Manifest manifest =
        ParseManifest("{ \"icons\": [ {\"src\": \"\", \"density\": 1.00 } ] }");
    EXPECT_EQ(manifest.icons[0].density, 1);
  }

  // Edge case: values between 0.0 and 1.0 are allowed.
  {
    Manifest manifest =
        ParseManifest("{ \"icons\": [ {\"src\": \"\", \"density\": 0.42 } ] }");
    EXPECT_EQ(manifest.icons[0].density, 0.42);
  }

  // 0 is an invalid value.
  {
    Manifest manifest =
        ParseManifest("{ \"icons\": [ {\"src\": \"\", \"density\": 0.0 } ] }");
    EXPECT_EQ(manifest.icons[0].density, Manifest::Icon::kDefaultDensity);
  }

  // Negative values are invalid.
  {
    Manifest manifest =
        ParseManifest("{ \"icons\": [ {\"src\": \"\", \"density\": -2.5 } ] }");
    EXPECT_EQ(manifest.icons[0].density, Manifest::Icon::kDefaultDensity);
  }
}

TEST_F(ManifestParserTest, IconSizesParseRules) {
  // Smoke test.
  {
    Manifest manifest = ParseManifest("{ \"icons\": [ {\"src\": \"\","
        "\"sizes\": \"42x42\" } ] }");
    EXPECT_EQ(manifest.icons[0].sizes.size(), 1u);
  }

  // Trim whitespaces.
  {
    Manifest manifest = ParseManifest("{ \"icons\": [ {\"src\": \"\","
        "\"sizes\": \"  42x42  \" } ] }");
    EXPECT_EQ(manifest.icons[0].sizes.size(), 1u);
  }

  // Don't parse if name isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"icons\": [ {\"src\": \"\","
        "\"sizes\": {} } ] }");
    EXPECT_EQ(manifest.icons[0].sizes.size(), 0u);
  }

  // Don't parse if name isn't a string.
  {
    Manifest manifest = ParseManifest("{ \"icons\": [ {\"src\": \"\","
        "\"sizes\": 42 } ] }");
    EXPECT_EQ(manifest.icons[0].sizes.size(), 0u);
  }

  // Smoke test: value correctly parsed.
  {
    Manifest manifest = ParseManifest("{ \"icons\": [ {\"src\": \"\","
        "\"sizes\": \"42x42  48x48\" } ] }");
    EXPECT_EQ(manifest.icons[0].sizes[0], gfx::Size(42, 42));
    EXPECT_EQ(manifest.icons[0].sizes[1], gfx::Size(48, 48));
  }

  // <WIDTH>'x'<HEIGHT> and <WIDTH>'X'<HEIGHT> are equivalent.
  {
    Manifest manifest = ParseManifest("{ \"icons\": [ {\"src\": \"\","
        "\"sizes\": \"42X42  48X48\" } ] }");
    EXPECT_EQ(manifest.icons[0].sizes[0], gfx::Size(42, 42));
    EXPECT_EQ(manifest.icons[0].sizes[1], gfx::Size(48, 48));
  }

  // Twice the same value is parsed twice.
  {
    Manifest manifest = ParseManifest("{ \"icons\": [ {\"src\": \"\","
        "\"sizes\": \"42X42  42x42\" } ] }");
    EXPECT_EQ(manifest.icons[0].sizes[0], gfx::Size(42, 42));
    EXPECT_EQ(manifest.icons[0].sizes[1], gfx::Size(42, 42));
  }

  // Width or height can't start with 0.
  {
    Manifest manifest = ParseManifest("{ \"icons\": [ {\"src\": \"\","
        "\"sizes\": \"004X007  042x00\" } ] }");
    EXPECT_EQ(manifest.icons[0].sizes.size(), 0u);
  }

  // Width and height MUST contain digits.
  {
    Manifest manifest = ParseManifest("{ \"icons\": [ {\"src\": \"\","
        "\"sizes\": \"e4X1.0  55ax1e10\" } ] }");
    EXPECT_EQ(manifest.icons[0].sizes.size(), 0u);
  }

  // 'any' is correctly parsed and transformed to gfx::Size(0,0).
  {
    Manifest manifest = ParseManifest("{ \"icons\": [ {\"src\": \"\","
        "\"sizes\": \"any AnY ANY aNy\" } ] }");
    gfx::Size any = gfx::Size(0, 0);
    EXPECT_EQ(manifest.icons[0].sizes.size(), 4u);
    EXPECT_EQ(manifest.icons[0].sizes[0], any);
    EXPECT_EQ(manifest.icons[0].sizes[1], any);
    EXPECT_EQ(manifest.icons[0].sizes[2], any);
    EXPECT_EQ(manifest.icons[0].sizes[3], any);
  }

  // Some invalid width/height combinations.
  {
    Manifest manifest = ParseManifest("{ \"icons\": [ {\"src\": \"\","
        "\"sizes\": \"x 40xx 1x2x3 x42 42xx42\" } ] }");
    gfx::Size any = gfx::Size(0, 0);
    EXPECT_EQ(manifest.icons[0].sizes.size(), 0u);
  }
}

} // namespace content

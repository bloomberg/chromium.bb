// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_vector.h"
#include "base/version.h"
#include "chrome/common/extensions/update_manifest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "libxml/globals.h"

static const char* kValidXml =
"<?xml version='1.0' encoding='UTF-8'?>"
"<gupdate xmlns='http://www.google.com/update2/response' protocol='2.0'>"
" <app appid='12345'>"
"  <updatecheck codebase='http://example.com/extension_1.2.3.4.crx'"
"               version='1.2.3.4' prodversionmin='2.0.143.0' />"
" </app>"
"</gupdate>";

const char *valid_xml_with_hash =
"<?xml version='1.0' encoding='UTF-8'?>"
"<gupdate xmlns='http://www.google.com/update2/response' protocol='2.0'>"
" <app appid='12345'>"
"  <updatecheck codebase='http://example.com/extension_1.2.3.4.crx'"
"               version='1.2.3.4' prodversionmin='2.0.143.0' "
"               hash='1234'/>"
" </app>"
"</gupdate>";

static const char*  kMissingAppId =
"<?xml version='1.0'?>"
"<gupdate xmlns='http://www.google.com/update2/response' protocol='2.0'>"
" <app>"
"  <updatecheck codebase='http://example.com/extension_1.2.3.4.crx'"
"               version='1.2.3.4' />"
" </app>"
"</gupdate>";

static const char*  kInvalidCodebase =
"<?xml version='1.0'?>"
"<gupdate xmlns='http://www.google.com/update2/response' protocol='2.0'>"
" <app appid='12345' status='ok'>"
"  <updatecheck codebase='example.com/extension_1.2.3.4.crx'"
"               version='1.2.3.4' />"
" </app>"
"</gupdate>";

static const char*  kMissingVersion =
"<?xml version='1.0'?>"
"<gupdate xmlns='http://www.google.com/update2/response' protocol='2.0'>"
" <app appid='12345' status='ok'>"
"  <updatecheck codebase='http://example.com/extension_1.2.3.4.crx' />"
" </app>"
"</gupdate>";

static const char*  kInvalidVersion =
"<?xml version='1.0'?>"
"<gupdate xmlns='http://www.google.com/update2/response' protocol='2.0'>"
" <app appid='12345' status='ok'>"
"  <updatecheck codebase='http://example.com/extension_1.2.3.4.crx' "
"               version='1.2.3.a'/>"
" </app>"
"</gupdate>";

static const char* kUsesNamespacePrefix =
"<?xml version='1.0' encoding='UTF-8'?>"
"<g:gupdate xmlns:g='http://www.google.com/update2/response' protocol='2.0'>"
" <g:app appid='12345'>"
"  <g:updatecheck codebase='http://example.com/extension_1.2.3.4.crx'"
"               version='1.2.3.4' prodversionmin='2.0.143.0' />"
" </g:app>"
"</g:gupdate>";

// Includes unrelated <app> tags from other xml namespaces - this should
// not cause problems.
static const char* kSimilarTagnames =
"<?xml version='1.0' encoding='UTF-8'?>"
"<gupdate xmlns='http://www.google.com/update2/response'"
"         xmlns:a='http://a' protocol='2.0'>"
" <a:app/>"
" <b:app xmlns:b='http://b' />"
" <app appid='12345'>"
"  <updatecheck codebase='http://example.com/extension_1.2.3.4.crx'"
"               version='1.2.3.4' prodversionmin='2.0.143.0' />"
" </app>"
"</gupdate>";

// Includes a <daystart> tag.
static const char* kWithDaystart =
"<?xml version='1.0' encoding='UTF-8'?>"
"<gupdate xmlns='http://www.google.com/update2/response' protocol='2.0'>"
" <daystart elapsed_seconds='456' />"
" <app appid='12345'>"
"  <updatecheck codebase='http://example.com/extension_1.2.3.4.crx'"
"               version='1.2.3.4' prodversionmin='2.0.143.0' />"
" </app>"
"</gupdate>";

// Indicates no updates available - this should not be a parse error.
static const char* kNoUpdate =
"<?xml version='1.0' encoding='UTF-8'?>"
"<gupdate xmlns='http://www.google.com/update2/response' protocol='2.0'>"
" <app appid='12345'>"
"  <updatecheck status='noupdate' />"
" </app>"
"</gupdate>";

TEST(ExtensionUpdateManifestTest, TestUpdateManifest) {
  UpdateManifest parser;

  // Test parsing of a number of invalid xml cases
  EXPECT_FALSE(parser.Parse(""));
  EXPECT_FALSE(parser.Parse(kMissingAppId));
  EXPECT_FALSE(parser.Parse(kInvalidCodebase));
  EXPECT_FALSE(parser.Parse(kMissingVersion));
  EXPECT_FALSE(parser.Parse(kInvalidVersion));

  // Parse some valid XML, and check that all params came out as expected
  EXPECT_TRUE(parser.Parse(kValidXml));
  EXPECT_FALSE(parser.results().list.empty());
  const UpdateManifest::Result* firstResult = &parser.results().list.at(0);
  EXPECT_EQ(GURL("http://example.com/extension_1.2.3.4.crx"),
            firstResult->crx_url);

  EXPECT_EQ("1.2.3.4", firstResult->version);
  EXPECT_EQ("2.0.143.0", firstResult->browser_min_version);

  // Parse some xml that uses namespace prefixes.
  EXPECT_TRUE(parser.Parse(kUsesNamespacePrefix));
  EXPECT_TRUE(parser.Parse(kSimilarTagnames));
  xmlCleanupGlobals();

  // Parse xml with hash value
  EXPECT_TRUE(parser.Parse(valid_xml_with_hash));
  EXPECT_FALSE(parser.results().list.empty());
  firstResult = &parser.results().list.at(0);
  EXPECT_EQ("1234", firstResult->package_hash);

  EXPECT_TRUE(parser.Parse(kWithDaystart));
  EXPECT_FALSE(parser.results().list.empty());
  EXPECT_EQ(parser.results().daystart_elapsed_seconds, 456);

  // Parse a no-update response.
  EXPECT_TRUE(parser.Parse(kNoUpdate));
  EXPECT_FALSE(parser.results().list.empty());
  firstResult = &parser.results().list.at(0);
  EXPECT_EQ(firstResult->extension_id, "12345");
  EXPECT_EQ(firstResult->version, "");
}

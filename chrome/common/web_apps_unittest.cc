// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/web_apps.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/json_schema_validator.h"
#include "content/common/json_value_serializer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

DictionaryValue* LoadDefinitionFile(const std::string& name) {
  FilePath path;
  if (!PathService::Get(chrome::DIR_TEST_DATA, &path)) {
    ADD_FAILURE() << "Could not get test data dir.";
    return NULL;
  }

  path = path.AppendASCII("web_app_info").AppendASCII(name.c_str());
  if (!file_util::PathExists(path)) {
    ADD_FAILURE() << "Path does not exist: " << path.value();
    return NULL;
  }

  std::string error;
  JSONFileValueSerializer serializer(path);
  DictionaryValue* result = static_cast<DictionaryValue*>(
      serializer.Deserialize(NULL, &error));
  if (!result) {
    ADD_FAILURE() << "Error parsing " << name << ": " << error;
    return NULL;
  }

  return result;
}

WebApplicationInfo* ParseFromDefinitionAndExpectSuccess(
    const std::string& name) {
  scoped_ptr<DictionaryValue> defintion(LoadDefinitionFile(name));
  if (!defintion.get())
    return NULL;

  scoped_ptr<WebApplicationInfo> web_app(new WebApplicationInfo());
  web_app->manifest_url = GURL("http://example.com/");

  string16 error;
  if (!web_apps::ParseWebAppFromDefinitionFile(defintion.get(), web_app.get(),
                                               &error)) {
    ADD_FAILURE() << "Error parsing " << name << ": " << UTF16ToUTF8(error);
    return NULL;
  }

  return web_app.release();
}

void ParseFromDefinitionAndExpectFailure(const std::string& name,
                                         const string16& expected_error) {
  scoped_ptr<DictionaryValue> definition(LoadDefinitionFile(name));
  if (!definition.get())
    return;

  WebApplicationInfo web_app;
  web_app.manifest_url = GURL("http://example.com/");

  string16 error;
  if (web_apps::ParseWebAppFromDefinitionFile(definition.get(), &web_app,
                                              &error)) {
    ADD_FAILURE() << "Expected error parsing " << name
                  << " but parse succeeded.";
    return;
  }

  EXPECT_EQ(UTF16ToUTF8(expected_error), UTF16ToUTF8(error)) << name;
}

}

TEST(WebAppInfo, ParseFromDefinitionFileErrors) {
  // Test one definition file with a JSON schema error, just to make sure we're
  // correctly propagating those. We don't extensively test all the properties
  // covered by the schema, since we assume JSON schema is working correctly.
  ParseFromDefinitionAndExpectFailure(
      "missing_name.json",
      UTF8ToUTF16(std::string("name: ") +
                      JSONSchemaValidator::kObjectPropertyIsRequired));

  ParseFromDefinitionAndExpectFailure(
      "invalid_launch_url.json",
      UTF8ToUTF16(WebApplicationInfo::kInvalidLaunchURL));

  ParseFromDefinitionAndExpectFailure(
      "invalid_urls.json",
      UTF8ToUTF16(
          JSONSchemaValidator::FormatErrorMessage(
              WebApplicationInfo::kInvalidURL, "2")));
}

TEST(WebAppInfo, Minimal) {
  scoped_ptr<WebApplicationInfo> web_app(
      ParseFromDefinitionAndExpectSuccess("minimal.json"));

  EXPECT_EQ(UTF8ToUTF16("hello"), web_app->title);
  EXPECT_EQ(UTF8ToUTF16(""), web_app->description);
  EXPECT_EQ(GURL("http://example.com/launch_url"), web_app->app_url);
  EXPECT_EQ(0u, web_app->icons.size());
  EXPECT_EQ(0u, web_app->urls.size());
  EXPECT_EQ(0u, web_app->permissions.size());
  EXPECT_EQ("", web_app->launch_container);
}

TEST(WebAppInfo, Full) {
  scoped_ptr<WebApplicationInfo> web_app(
      ParseFromDefinitionAndExpectSuccess("full.json"));

  EXPECT_EQ(UTF8ToUTF16("hello"), web_app->title);
  EXPECT_EQ(UTF8ToUTF16("This app is super awesome"), web_app->description);
  EXPECT_EQ(GURL("http://example.com/launch_url"), web_app->app_url);
  ASSERT_EQ(1u, web_app->icons.size());
  EXPECT_EQ("http://example.com/16.png", web_app->icons[0].url.spec());
  EXPECT_EQ(16, web_app->icons[0].width);
  EXPECT_EQ(16, web_app->icons[0].height);
  ASSERT_EQ(2u, web_app->urls.size());
  EXPECT_EQ("http://example.com/foobar", web_app->urls[0].spec());
  EXPECT_EQ("http://example.com/baz", web_app->urls[1].spec());
  ASSERT_EQ(2u, web_app->permissions.size());
  EXPECT_EQ("geolocation", web_app->permissions[0]);
  EXPECT_EQ("notifications", web_app->permissions[1]);
  EXPECT_EQ("panel", web_app->launch_container);
}

// Tests ParseIconSizes with various input.
TEST(WebAppInfo, ParseIconSizes) {
  struct TestData {
    const char* input;
    const bool expected_result;
    const bool is_any;
    const size_t expected_size_count;
    const int width1;
    const int height1;
    const int width2;
    const int height2;
  } data[] = {
    // Bogus input cases.
    { "10",         false, false, 0, 0, 0, 0, 0 },
    { "10 10",      false, false, 0, 0, 0, 0, 0 },
    { "010",        false, false, 0, 0, 0, 0, 0 },
    { " 010 ",      false, false, 0, 0, 0, 0, 0 },
    { " 10x ",      false, false, 0, 0, 0, 0, 0 },
    { " x10 ",      false, false, 0, 0, 0, 0, 0 },
    { "any 10x10",  false, false, 0, 0, 0, 0, 0 },
    { "",           false, false, 0, 0, 0, 0, 0 },
    { "10ax11",     false, false, 0, 0, 0, 0, 0 },

    // Any.
    { "any",        true, true, 0, 0, 0, 0, 0 },
    { " any",       true, true, 0, 0, 0, 0, 0 },
    { " any ",      true, true, 0, 0, 0, 0, 0 },

    // Sizes.
    { "10x11",      true, false, 1, 10, 11, 0, 0 },
    { " 10x11 ",    true, false, 1, 10, 11, 0, 0 },
    { " 10x11 1x2", true, false, 2, 10, 11, 1, 2 },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    bool is_any;
    std::vector<gfx::Size> sizes;
    bool result = web_apps::ParseIconSizes(ASCIIToUTF16(data[i].input), &sizes,
                                           &is_any);
    ASSERT_EQ(result, data[i].expected_result);
    if (result) {
      ASSERT_EQ(data[i].is_any, is_any);
      ASSERT_EQ(data[i].expected_size_count, sizes.size());
      if (!sizes.empty()) {
        ASSERT_EQ(data[i].width1, sizes[0].width());
        ASSERT_EQ(data[i].height1, sizes[0].height());
      }
      if (sizes.size() > 1) {
        ASSERT_EQ(data[i].width2, sizes[1].width());
        ASSERT_EQ(data[i].height2, sizes[1].height());
      }
    }
  }
}

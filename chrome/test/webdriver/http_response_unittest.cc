// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webdriver {

namespace {

void ExpectHeaderValue(const HttpResponse& response, const std::string& name,
                       const std::string& expected_value) {
  std::string actual_value;
  EXPECT_TRUE(response.GetHeader(name, &actual_value));
  EXPECT_EQ(expected_value, actual_value);
}

}  // namespace

TEST(HttpResponseTest, AddingHeaders) {
  HttpResponse response;

  response.AddHeader("FOO", "a");
  ExpectHeaderValue(response, "foo", "a");

  // Headers should be case insensitive.
  response.AddHeader("fOo", "b,c");
  response.AddHeader("FoO", "d");
  ExpectHeaderValue(response, "foo", "a,b,c,d");
}

TEST(HttpResponseTest, RemovingHeaders) {
  HttpResponse response;

  ASSERT_FALSE(response.RemoveHeader("I-Am-Not-There"));

  ASSERT_FALSE(response.GetHeader("foo", NULL));
  response.AddHeader("foo", "bar");
  ASSERT_TRUE(response.GetHeader("foo", NULL));
  ASSERT_TRUE(response.RemoveHeader("foo"));
  ASSERT_FALSE(response.GetHeader("foo", NULL));
}

TEST(HttpResponseTest, CanClearAllHeaders) {
  HttpResponse response;
  response.AddHeader("food", "cheese");
  response.AddHeader("color", "red");

  ExpectHeaderValue(response, "food", "cheese");
  ExpectHeaderValue(response, "color", "red");

  response.ClearHeaders();
  EXPECT_FALSE(response.GetHeader("food", NULL));
  EXPECT_FALSE(response.GetHeader("color", NULL));
}

TEST(HttpResponseTest, CanSetMimeType) {
  HttpResponse response;

  response.SetMimeType("application/json");
  ExpectHeaderValue(response, "content-type", "application/json");

  response.SetMimeType("text/html");
  ExpectHeaderValue(response, "content-type", "text/html");
}

TEST(HttpResponseTest, SetBody) {
  HttpResponse response;

  std::string body("foo bar");
  response.SetBody(body);
  ASSERT_EQ(body.length(), response.length());
  ASSERT_EQ(body, std::string(response.data(), response.length()));

  // Grow the response size.
  body.append(" baz");
  response.SetBody(body);
  ASSERT_EQ(body.length(), response.length());
  ASSERT_EQ(body, std::string(response.data(), response.length()));

  // Shrink the response size.
  body = "small";
  response.SetBody(body);
  ASSERT_EQ(body.length(), response.length());
  ASSERT_EQ(body, std::string(response.data(), response.length()));
}

}  // namespace webdriver

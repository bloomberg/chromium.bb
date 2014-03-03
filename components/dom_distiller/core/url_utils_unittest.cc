// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/url_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace dom_distiller {

namespace url_utils {

TEST(DomDistillerUrlUtilsTest, TestPathUtil) {
  const std::string single_key = "mypath?foo=bar";
  EXPECT_EQ("bar", GetValueForKeyInUrlPathQuery(single_key, "foo"));
  const std::string two_keys = "mypath?key1=foo&key2=bar";
  EXPECT_EQ("foo", GetValueForKeyInUrlPathQuery(two_keys, "key1"));
  EXPECT_EQ("bar", GetValueForKeyInUrlPathQuery(two_keys, "key2"));
  const std::string multiple_same_key = "mypath?key=foo&key=bar";
  EXPECT_EQ("foo", GetValueForKeyInUrlPathQuery(multiple_same_key, "key"));
}

}  // namespace url_utils

}  // namespace dom_distiller

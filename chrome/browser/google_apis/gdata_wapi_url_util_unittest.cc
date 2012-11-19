// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/gdata_wapi_url_util.h"

#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {
namespace gdata_wapi_url_util {

TEST(GDataWapiUrlUtilTest, AddStandardUrlParams) {
  EXPECT_EQ("http://www.example.com/?v=3&alt=json",
            AddStandardUrlParams(GURL("http://www.example.com")).spec());
}

TEST(GDataWapiUrlUtilTest, AddMetadataUrlParams) {
  EXPECT_EQ("http://www.example.com/?v=3&alt=json&include-installed-apps=true",
            AddMetadataUrlParams(GURL("http://www.example.com")).spec());
}

TEST(GDataWapiUrlUtilTest, AddFeedUrlParams) {
  EXPECT_EQ("http://www.example.com/?v=3&alt=json&showfolders=true"
            "&max-results=100"
            "&include-installed-apps=true",
            AddFeedUrlParams(GURL("http://www.example.com"),
                             100,  // num_items_to_fetch
                             0,  // changestamp
                             ""  // search_string
                             ).spec());
  EXPECT_EQ("http://www.example.com/?v=3&alt=json&showfolders=true"
            "&max-results=100"
            "&include-installed-apps=true"
            "&start-index=123",
            AddFeedUrlParams(GURL("http://www.example.com"),
                             100,  // num_items_to_fetch
                             123,  // changestamp
                             ""  // search_string
                             ).spec());
  EXPECT_EQ("http://www.example.com/?v=3&alt=json&showfolders=true"
            "&max-results=100"
            "&include-installed-apps=true"
            "&start-index=123"
            "&q=%22foo+bar%22",
            AddFeedUrlParams(GURL("http://www.example.com"),
                             100,  // num_items_to_fetch
                             123,  // changestamp
                             "\"foo bar\""  // search_string
                             ).spec());
}

TEST(GDataWapiUrlUtilTest, FormatDocumentListURL) {
  EXPECT_EQ("https://docs.google.com/feeds/default/private/full/-/mine",
            FormatDocumentListURL("").spec());
  EXPECT_EQ("https://docs.google.com/feeds/default/private/full/"
            "RESOURCE-ID/contents/-/mine",
            FormatDocumentListURL("RESOURCE-ID").spec());
}

}  // namespace gdata_wapi_url_util
}  // namespace google_apis

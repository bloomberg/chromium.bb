// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/navigation/wk_based_restore_session_util.h"

#include <memory>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#import "ios/web/navigation/navigation_item_impl.h"
#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace web {

typedef PlatformTest WKBasedRestoreSessionUtilTest;

TEST_F(WKBasedRestoreSessionUtilTest, CreateRestoreSessionUrl) {
  auto item0 = std::make_unique<NavigationItemImpl>();
  item0->SetURL(GURL("http://www.0.com"));
  item0->SetTitle(base::ASCIIToUTF16("Test Website 0"));
  auto item1 = std::make_unique<NavigationItemImpl>();
  item1->SetURL(GURL("http://www.1.com"));

  std::vector<std::unique_ptr<NavigationItem>> items;
  items.push_back(std::move(item0));
  items.push_back(std::move(item1));

  GURL restore_session_url =
      CreateRestoreSessionUrl(0 /* last_committed_item_index */, items);
  ASSERT_TRUE(IsRestoreSessionUrl(restore_session_url));

  std::string session_json;
  net::GetValueForKeyInQuery(restore_session_url,
                             kRestoreSessionSessionQueryKey, &session_json);
  EXPECT_EQ(
      "{\"offset\":-1,\"titles\":[\"Test Website 0\",\"\"],"
      "\"urls\":[\"http://www.0.com/\",\"http://www.1.com/\"]}",
      session_json);
}

TEST_F(WKBasedRestoreSessionUtilTest, IsNotRestoreSessionUrl) {
  EXPECT_FALSE(IsRestoreSessionUrl(GURL()));
  EXPECT_FALSE(IsRestoreSessionUrl(GURL("file://somefile")));
  EXPECT_FALSE(IsRestoreSessionUrl(GURL("http://www.1.com")));
}

TEST_F(WKBasedRestoreSessionUtilTest, ExtractTargetURL) {
  GURL target_url = GURL("http://www.1.com?query=special%26chars");
  GURL url = net::AppendQueryParameter(GetRestoreSessionBaseUrl(),
                                       kRestoreSessionTargetUrlQueryKey,
                                       target_url.spec());
  GURL extracted_url;
  ASSERT_TRUE(ExtractTargetURL(url, &extracted_url));
  EXPECT_EQ(target_url, extracted_url);
}

}  // namespace web

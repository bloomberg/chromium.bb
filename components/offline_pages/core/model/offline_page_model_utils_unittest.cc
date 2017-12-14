// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/offline_page_model_utils.h"

#include "components/offline_pages/core/client_namespace_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

TEST(OfflinePageModelUtilsTest, ToNamespaceEnum) {
  EXPECT_EQ(model_utils::ToNamespaceEnum(kDefaultNamespace),
            OfflinePagesNamespaceEnumeration::DEFAULT);
  EXPECT_EQ(model_utils::ToNamespaceEnum(kBookmarkNamespace),
            OfflinePagesNamespaceEnumeration::BOOKMARK);
  EXPECT_EQ(model_utils::ToNamespaceEnum(kLastNNamespace),
            OfflinePagesNamespaceEnumeration::LAST_N);
  EXPECT_EQ(model_utils::ToNamespaceEnum(kAsyncNamespace),
            OfflinePagesNamespaceEnumeration::ASYNC_LOADING);
  EXPECT_EQ(model_utils::ToNamespaceEnum(kCCTNamespace),
            OfflinePagesNamespaceEnumeration::CUSTOM_TABS);
  EXPECT_EQ(model_utils::ToNamespaceEnum(kDownloadNamespace),
            OfflinePagesNamespaceEnumeration::DOWNLOAD);
  EXPECT_EQ(model_utils::ToNamespaceEnum(kNTPSuggestionsNamespace),
            OfflinePagesNamespaceEnumeration::NTP_SUGGESTION);
  EXPECT_EQ(model_utils::ToNamespaceEnum(kSuggestedArticlesNamespace),
            OfflinePagesNamespaceEnumeration::SUGGESTED_ARTICLES);
  EXPECT_EQ(model_utils::ToNamespaceEnum(kBrowserActionsNamespace),
            OfflinePagesNamespaceEnumeration::BROWSER_ACTIONS);
}

}  // namespace offline_pages

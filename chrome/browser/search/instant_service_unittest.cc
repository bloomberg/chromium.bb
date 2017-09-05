// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/instant_service.h"

#include <vector>

#include "chrome/browser/search/instant_unittest_base.h"
#include "chrome/common/search/instant_types.h"
#include "components/history/core/browser/history_types.h"
#include "url/gurl.h"

using InstantServiceTest = InstantUnitTestBase;

TEST_F(InstantServiceTest, GetSuggestionFromClientSide) {
  history::MostVisitedURLList url_list;
  url_list.push_back(history::MostVisitedURL());

  instant_service_->OnTopSitesReceived(url_list);

  auto items = instant_service_->most_visited_items_;
  ASSERT_EQ(1, (int)items.size());
  ASSERT_EQ(ntp_tiles::TileSource::TOP_SITES, items[0].source);
}

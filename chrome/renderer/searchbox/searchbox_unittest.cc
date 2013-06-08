// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "chrome/common/instant_types.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace internal {

// Defined in searchbox_extension.cc
bool GetInstantRestrictedIDFromURL(int render_view_id,
                                   const GURL& url,
                                   InstantRestrictedID* id);

TEST(SearchBoxUtilTest, GetInstantRestrictedIDFromTransientURL) {
  const int kInvalidRenderViewID = 920;
  const int kValidRenderViewID = 1;

  const struct {
    int render_view_id;
    GURL transient_url;
    InstantRestrictedID expected_rid;
    bool expected_return_val;
  } test_cases[] = {
    // RenderView ID matches the view id specified in the transient url.
    {kValidRenderViewID, GURL("chrome-search://favicon/1/2"), 2, true},
    {kValidRenderViewID, GURL("chrome-search://thumb/1/2"), 2, true},

    // RenderView ID does not match the view id specified in the transient url.
    {kInvalidRenderViewID, GURL("chrome-search://favicon/1/2"), 0, false},
    {kInvalidRenderViewID, GURL("chrome-search://thumb/1/2"), 0, false},

    // Invalid transient urls.
    {kValidRenderViewID, GURL("chrome-search://thumb"), 0, false},
    {kValidRenderViewID, GURL("chrome-search://thumb/"), 0, false},
    {kValidRenderViewID, GURL("chrome-search://thumb/123"), 0, false},
    {kValidRenderViewID, GURL("chrome-search://thumb/xyz"), 0, false},
    {kValidRenderViewID, GURL("chrome-search://thumb/123/"), 0, false},
    {kValidRenderViewID, GURL("chrome-search://thumb/123/xyz"), 0, false},
    {kValidRenderViewID, GURL("chrome-search://favicon"), 0, false},
    {kValidRenderViewID, GURL("chrome-search://favicon/"), 0, false},
    {kValidRenderViewID, GURL("chrome-search://favicon/123"), 0, false},
    {kValidRenderViewID, GURL("chrome-search://favicon/xyz"), 0, false},
    {kValidRenderViewID, GURL("chrome-search://favicon/123/"), 0, false},
    {kValidRenderViewID, GURL("chrome-search://favicon/123/xyz"), 0, false}
  };

  InstantRestrictedID rid = 0;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    bool return_val = GetInstantRestrictedIDFromURL(
        test_cases[i].render_view_id, test_cases[i].transient_url, &rid);
    EXPECT_EQ(test_cases[i].expected_return_val, return_val);
    EXPECT_EQ(test_cases[i].expected_rid, rid);
    rid = 0;
  }
}

}  // namespace internal

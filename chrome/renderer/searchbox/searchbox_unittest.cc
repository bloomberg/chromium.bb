// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "chrome/common/instant_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace internal {

// Defined in searchbox.cc
bool GetRestrictedIDFromThumbnailUrl(int render_view_id,
                                     const GURL& url,
                                     InstantRestrictedID* id);

// Defined in searchbox.cc
bool GetRestrictedIDFromFaviconUrl(int render_view_id,
                                   const GURL& url,
                                   std::string* favicon_params,
                                   InstantRestrictedID* rid);

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
    bool return_val = GetRestrictedIDFromThumbnailUrl(
        test_cases[i].render_view_id, test_cases[i].transient_url, &rid);
    EXPECT_EQ(test_cases[i].expected_return_val, return_val);
    EXPECT_EQ(test_cases[i].expected_rid, rid);
    rid = 0;
  }
}

TEST(SearchBoxUtilTest, ParseRestrictedFaviconTransientUrl) {
  const int kInvalidRenderViewID = 920;
  const int kValidRenderViewID = 1;

  const struct {
    int render_view_id;
    GURL transient_url;
    std::string expected_favicon_params;
    InstantRestrictedID expected_rid;
    bool expected_return_val;
  } test_cases[] = {
    // RenderView ID matches the view id specified in the transient url.
    {
      kValidRenderViewID,
      GURL("chrome-search://favicon/1/2"),
      "",
      2,
      true
    },
    {
      kValidRenderViewID,
      GURL("chrome-search://favicon/size/16@2x/1/2"),
      "size/16@2x/",
      2,
      true
    },
    {
      kValidRenderViewID,
      GURL("chrome-search://favicon/largest/1/2"),
      "largest/",
      2,
      true
    },
    {
      kValidRenderViewID,
      GURL("chrome-search://favicon/origin/1/2"),
      "origin/",
      2,
      true
    },
    {
      kValidRenderViewID,
      GURL("chrome-search://favicon/iconurl/1/2"),
      "iconurl/",
      2,
      true
    },

    // RenderView ID does not match the view id specified in the transient url.
    {
      kInvalidRenderViewID,
      GURL("chrome-search://favicon/1/2"),
      "",
      0,
      true
    },
    {
      kInvalidRenderViewID,
      GURL("chrome-search://favicon/size/16@2x/1/2"),
      "size/16@2x/",
      0,
      true
    },
    {
      kInvalidRenderViewID,
      GURL("chrome-search://favicon/largest/1/2"),
      "largest/",
      0,
      true
    },
    {
      kInvalidRenderViewID,
      GURL("chrome-search://favicon/origin/1/2"),
      "origin/",
      0,
      true
    },
    {
      kInvalidRenderViewID,
      GURL("chrome-search://favicon/iconurl/1/2"),
      "iconurl/",
      0,
      true
    },

    // Invalid transient urls.
    {
      kValidRenderViewID,
      GURL("chrome-search://favicon"),
      "",
      0,
      false
    },
    {
      kValidRenderViewID,
      GURL("chrome-search://favicon/"),
      "",
      0,
      false
    },
    {
      kValidRenderViewID,
      GURL("chrome-search://favicon/size/16@2x"),
      "",
      0,
      false
    },
    {
      kValidRenderViewID,
      GURL("chrome-search://favicon/size"),
      "",
      0,
      true
    },
    {
      kValidRenderViewID,
      GURL("chrome-search://favicon/size/16@2x/123"),
      "size/16@2x/",
      0,
      true
    },
    {
      kValidRenderViewID,
      GURL("chrome-search://favicon/size/16@2x/xyz"),
      "size/16@2x/",
      0,
      true
    },
    {
      kValidRenderViewID,
      GURL("chrome-search://favicon/size/16@2x/123/"),
      "size/16@2x/",
      0,
      true
    },
    {
      kValidRenderViewID,
      GURL("chrome-search://favicon/size/16@2x/123/xyz"),
      "size/16@2x/",
      0,
      true
    },
    {
      kValidRenderViewID,
      GURL("chrome-search://favicon/invalidparameter/16@2x/1/2"),
      "",
      0,
      true
    }
  };

  std::string favicon_params = "";
  InstantRestrictedID rid = 0;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    bool return_val = GetRestrictedIDFromFaviconUrl(
        test_cases[i].render_view_id,
        test_cases[i].transient_url,
        &favicon_params,
        &rid);
    EXPECT_EQ(test_cases[i].expected_return_val, return_val);
    EXPECT_EQ(test_cases[i].expected_favicon_params, favicon_params);
    EXPECT_EQ(test_cases[i].expected_rid, rid);
    favicon_params = "";
    rid = 0;
  }
}

}  // namespace internal

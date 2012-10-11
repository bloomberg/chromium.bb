// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_TYPES_TEST_HELPER_H_
#define CHROME_BROWSER_SESSIONS_SESSION_TYPES_TEST_HELPER_H_

#include <string>

#include "base/time.h"
#include "content/public/common/page_transition_types.h"

class GURL;
class TabNavigation;

namespace content {
struct Referrer;
}

struct SessionTypesTestHelper {
  // Compares everything except index, unique ID, post ID, and
  // timestamp.
  static void ExpectNavigationEquals(const TabNavigation& expected,
                                     const TabNavigation& actual);

  // Create a TabNavigation with the given URL and title and some
  // common values for the other fields.
  static TabNavigation CreateNavigation(const std::string& virtual_url,
                                        const std::string& title);

  // Getters.

  static const content::Referrer& GetReferrer(const TabNavigation& navigation);

  static content::PageTransition GetTransitionType(
      const TabNavigation& navigation);

  static bool GetHasPostData(const TabNavigation& navigation);

  static int64 GetPostID(const TabNavigation& navigation);

  static const GURL& GetOriginalRequestURL(const TabNavigation& navigation);

  static bool GetIsOverridingUserAgent(const TabNavigation& navigation);

  static base::Time GetTimestamp(const TabNavigation& navigation);

  // Setters.

  static void SetContentState(TabNavigation* navigation,
                              const std::string& content_state);

  static void SetHasPostData(TabNavigation* navigation,
                             bool has_post_data);

  static void SetOriginalRequestURL(TabNavigation* navigation,
                                    const GURL& original_request_url);

  static void SetIsOverridingUserAgent(TabNavigation* navigation,
                                       bool is_overriding_user_agent);

  static void SetTimestamp(TabNavigation* navigation, base::Time timestamp);
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_TYPES_TEST_HELPER_H_

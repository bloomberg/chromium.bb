// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_SESSION_TYPES_TEST_HELPER_H_
#define COMPONENTS_SESSIONS_SESSION_TYPES_TEST_HELPER_H_

#include <string>

#include "base/basictypes.h"

class GURL;

namespace base {
class Time;
}

namespace content {
class PageState;
struct Referrer;
}

namespace sessions {

class SerializedNavigationEntry;

// Set of test functions to manipulate a SerializedNavigationEntry.
class SerializedNavigationEntryTestHelper {
 public:
  // Compares the two entries. This uses EXPECT_XXX on each member, if your test
  // needs to stop after this wrap calls to this in EXPECT_NO_FATAL_FAILURE.
  static void ExpectNavigationEquals(const SerializedNavigationEntry& expected,
                                     const SerializedNavigationEntry& actual);

  // Create a SerializedNavigationEntry with the given URL and title and some
  // common values for the other fields.
  static SerializedNavigationEntry CreateNavigation(
      const std::string& virtual_url,
      const std::string& title);

  static void SetPageState(const content::PageState& page_state,
                           SerializedNavigationEntry* navigation);

  static void SetHasPostData(bool has_post_data,
                             SerializedNavigationEntry* navigation);

  static void SetOriginalRequestURL(const GURL& original_request_url,
                                    SerializedNavigationEntry* navigation);

  static void SetIsOverridingUserAgent(bool is_overriding_user_agent,
                                       SerializedNavigationEntry* navigation);

  static void SetTimestamp(base::Time timestamp,
                           SerializedNavigationEntry* navigation);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SerializedNavigationEntryTestHelper);
};

}  // sessions

#endif  // COMPONENTS_SESSIONS_SESSION_TYPES_TEST_HELPER_H_

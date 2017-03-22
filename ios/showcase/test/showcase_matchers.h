// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHOWCASE_TEST_SHOWCASE_MATCHERS_H_
#define IOS_SHOWCASE_TEST_SHOWCASE_MATCHERS_H_

#import <EarlGrey/EarlGrey.h>

namespace showcase_matchers {

// Matcher for the back button on screens presented from the Showcase home
// screen.
id<GREYMatcher> FirstLevelBackButton();

}  // namespace showcase_matchers

#endif  // IOS_SHOWCASE_TEST_SHOWCASE_MATCHERS_H_

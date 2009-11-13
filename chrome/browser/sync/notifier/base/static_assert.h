// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_BASE_STATIC_ASSERT_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_BASE_STATIC_ASSERT_H_

template<bool> struct STATIC_ASSERTION_FAILURE;

template<> struct STATIC_ASSERTION_FAILURE<true> {
  enum { value = 1 };
};

template<int> struct static_assert_test{};

#define STATIC_ASSERT(B) \
typedef static_assert_test<\
  sizeof(STATIC_ASSERTION_FAILURE< (bool)( B ) >)>\
    static_assert_typedef_ ##  __LINE__

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_BASE_STATIC_ASSERT_H_

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// window_utils mocks.

#ifndef CEEE_TESTING_UTILS_MOCK_WINDOW_UTILS_H_
#define CEEE_TESTING_UTILS_MOCK_WINDOW_UTILS_H_

#include <set>

#include "ceee/testing/utils/mock_static.h"
#include "ceee/common/window_utils.h"

namespace testing {

// Mock object for some User32 functions.
MOCK_STATIC_CLASS_BEGIN(MockWindowUtils)
  MOCK_STATIC_INIT_BEGIN(MockWindowUtils)
    MOCK_STATIC_INIT2(window_utils::IsWindowClass, IsWindowClass);
    MOCK_STATIC_INIT2(window_utils::IsWindowThread, IsWindowThread);
    MOCK_STATIC_INIT2(window_utils::GetTopLevelParent, GetTopLevelParent);
    MOCK_STATIC_INIT2(window_utils::WindowHasNoThread, WindowHasNoThread);
    MOCK_STATIC_INIT2(window_utils::FindDescendentWindow, FindDescendentWindow);
    MOCK_STATIC_INIT2(window_utils::FindTopLevelWindows, FindTopLevelWindows);
  MOCK_STATIC_INIT_END()

  MOCK_STATIC2(bool, , IsWindowClass, HWND, const std::wstring&);
  MOCK_STATIC1(bool, , IsWindowThread, HWND);
  MOCK_STATIC1(HWND, , GetTopLevelParent, HWND);
  MOCK_STATIC1(bool, , WindowHasNoThread, HWND);
  MOCK_STATIC4(bool, , FindDescendentWindow, HWND, const std::wstring&,
                                             bool, HWND*);
  MOCK_STATIC2(void, , FindTopLevelWindows, const std::wstring&,
                                            std::set<HWND>*);
MOCK_STATIC_CLASS_END(MockWindowUtils)

}  // namespace testing

#endif  // CEEE_TESTING_UTILS_MOCK_WINDOW_UTILS_H_

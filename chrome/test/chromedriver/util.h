// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_UTIL_H_
#define CHROME_TEST_CHROMEDRIVER_UTIL_H_

#include <string>

namespace base {
class ListValue;
}

class Status;
class WebView;

// Generates a random, 32-character hexidecimal ID.
std::string GenerateId();

// Send a sequence of key strokes to the active Element in window.
Status SendKeysOnWindow(
    WebView* web_view,
    const base::ListValue* key_list,
    bool release_modifiers,
    int* sticky_modifiers);

#endif  // CHROME_TEST_CHROMEDRIVER_UTIL_H_

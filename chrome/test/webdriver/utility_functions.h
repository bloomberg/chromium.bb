// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_UTILITY_FUNCTIONS_H_
#define CHROME_TEST_WEBDRIVER_UTILITY_FUNCTIONS_H_

#include <string>

namespace base {
class Value;
}

namespace webdriver {

// Generates a random, 32-character hexidecimal ID.
std::string GenerateRandomID();

// Returns the equivalent JSON string for the given value.
std::string JsonStringify(const base::Value* value);

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_UTILITY_FUNCTIONS_H_

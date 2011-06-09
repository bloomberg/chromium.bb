// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_WEBDRIVER_LOGGING_H_
#define CHROME_TEST_WEBDRIVER_WEBDRIVER_LOGGING_H_

#include <string>

namespace webdriver {

// Initializes logging for WebDriver. All logging below the given level
// will be discarded.
void InitWebDriverLogging(int min_log_level);

// Retrieves the current contents of the WebDriver log file.
// Returns true on success.
bool GetLogContents(std::string* log_contents);

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_WEBDRIVER_LOGGING_H_

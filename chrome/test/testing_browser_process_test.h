// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_TESTING_BROWSER_PROCESS_TEST_H_
#define CHROME_TEST_TESTING_BROWSER_PROCESS_TEST_H_
#pragma once

#include "chrome/test/testing_browser_process.h"
#include "testing/gtest/include/gtest/gtest.h"

// Base class for tests that need |g_browser_process| to be initialized.
class TestingBrowserProcessTest : public testing::Test {
 protected:
  ScopedTestingBrowserProcess testing_browser_process_;
};

#endif  // CHROME_TEST_TESTING_BROWSER_PROCESS_TEST_H_

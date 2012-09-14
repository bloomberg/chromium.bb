// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_TEST_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_TEST_UTIL_H_

#include "chrome/browser/chromeos/gdata/gdata_test_util.h"

#include <string>

#include "base/memory/scoped_ptr.h"

class FilePath;

namespace base {
class Value;
}

namespace gdata {
namespace test_util {

// Runs a task posted to the blocking pool, including subsequent tasks posted
// to the UI message loop and the blocking pool.
//
// A task is often posted to the blocking pool with PostTaskAndReply(). In
// that case, a task is posted back to the UI message loop, which can again
// post a task to the blocking pool. This function processes these tasks
// repeatedly.
void RunBlockingPoolTask();

// Returns the absolute path for a test file stored under
// chrome/test/data/chromeos.
FilePath GetTestFilePath(const std::string& relative_path);

// Loads a test JSON file as a base::Value, from a test file stored under
// chrome/test/data/chromeos.
scoped_ptr<base::Value> LoadJSONFile(const std::string& relative_path);

}  // namespace test_util
}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_TEST_UTIL_H_

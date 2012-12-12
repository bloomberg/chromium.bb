// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_TEST_UTIL_H_
#define CHROME_BROWSER_GOOGLE_APIS_TEST_UTIL_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"

class FilePath;

namespace base {
class Value;
}

namespace google_apis {

class ResourceList;

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

// Copies the results from GetDataCallback.
void CopyResultsFromGetDataCallback(GDataErrorCode* error_out,
                                    scoped_ptr<base::Value>* value_out,
                                    GDataErrorCode error_in,
                                    scoped_ptr<base::Value> value_in);

// Copies the results from GetResourceListCallback.
void CopyResultsFromGetResourceListCallback(
    GDataErrorCode* error_out,
    scoped_ptr<ResourceList>* resource_list_out,
    GDataErrorCode error_in,
    scoped_ptr<ResourceList> resource_list_in);

}  // namespace test_util
}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_TEST_UTIL_H_

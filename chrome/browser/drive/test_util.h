// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DRIVE_TEST_UTIL_H_
#define CHROME_BROWSER_DRIVE_TEST_UTIL_H_

namespace drive {

class FakeDriveService;

namespace test_util {

bool SetUpTestEntries(FakeDriveService* drive_service);

}  // namespace test_util
}  // namespace drive

#endif  // CHROME_BROWSER_DRIVE_TEST_UTIL_H_

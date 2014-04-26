// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVICE_ADB_MOCK_ADB_SERVER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVICE_ADB_MOCK_ADB_SERVER_H_

// Single instance mock ADB server for use in browser tests. Runs on IO thread.

// These methods can be called from any thread.
void StartMockAdbServer();
void StopMockAdbServer();

#endif  // CHROME_BROWSER_DEVTOOLS_DEVICE_ADB_MOCK_ADB_SERVER_H_

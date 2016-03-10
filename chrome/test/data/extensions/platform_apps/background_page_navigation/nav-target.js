// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// We should never reach this page; if we have then it's a signal that we've
// navigated away from the app page, and we should have the test fail.
// TODO(lazyboy): Because of http://crbug.com/585570, we *do* reach this page,
// enable the following line once the bug is fixed.
//chrome.test.notifyFail('Navigated to ' + window.location.href);

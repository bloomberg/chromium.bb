// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This extension makes a single call to the tab API, it is used to test the
// activity log page as it should display a call for this extension.
// This test might be expanded upon in the future.

chrome.tabs.query({}, (tabs) => {
  console.log(`queried tabs: ${tabs}`);
});

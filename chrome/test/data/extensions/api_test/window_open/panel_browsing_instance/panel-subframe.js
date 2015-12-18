// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// If panel-subframe is (expectedly) in the same BrowsingInstance as
// background-subframe, then window.open should find the existing iframe.
//
// Otherwise window.open will cause a new tab to be opened and we will
// find this out and fail the test when comparing newNumberOfTabs and
// oldNumberOfTabs in background.js.
//
// Note that we can't verify if |w| is a new window by looking
// at |w.parent|, because this leads to an error similar to:
//   Uncaught SecurityError: Blocked a frame with origin "http://foo.com:36559"
//   from accessing a frame with origin
//   "chrome-extension://jmanjdlcdjhgbdccaamcmmcilljchoad".  The frame
//   requesting access has a protocol of "http", the frame being accessed has a
//   protocol of "chrome-extension". Protocols must match.
var w = window.open('http://blah', 'background-subframe-name')

// Quick and very dirty query parameters parsing... ok only for test code.
extensionId = window.location.href.substr(
    window.location.href.lastIndexOf('=') + 1);

// Let background.js know that it can now calculate newNumberOfTabs.
chrome.runtime.sendMessage(extensionId, { windowOpened: true });


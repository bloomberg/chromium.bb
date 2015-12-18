// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Quick and very dirty query parameters parsing... ok only for test code.
extensionId = window.location.href.substr(
    window.location.href.lastIndexOf('=') + 1);

// Let background.js know that it can now open the panel.
chrome.runtime.sendMessage(extensionId, { backgroundSubframeNavigated: true });


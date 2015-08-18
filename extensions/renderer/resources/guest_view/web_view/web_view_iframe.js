// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module provides --site-per-process overrides for WebView (<webview>).
// See web_view.js for details.

// NOTE: Do not remove these, we implicitly depend on these in
// --site-per-process.
var GuestViewIframe = require('guestViewIframe');
var GuestViewIframeContainer = require('guestViewIframeContainer');

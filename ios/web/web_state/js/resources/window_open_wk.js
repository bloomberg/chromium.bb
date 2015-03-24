// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('__crweb.window_open_wk');

// WKWebView natively supports window.open so there is no need to install
// JavaScript-based fix. CRWJSWindowOpenManager always injected as multiple
// JS managers depend on it. The script does not have any content except
// presenceBeacon.


// Namespace for this module.
__gCrWeb.windowOpen = {};


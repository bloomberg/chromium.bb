// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var watchForTag = require("tagWatcher").watchForTag;

// Output error message to console when using <webview> tag with no permission.
var errorMessage = "You do not have permission to use <webview> tag." +
  " Be sure to declare 'webview' permission in your manifest.";

window.addEventListener('DOMContentLoaded', function() {
  watchForTag('WEBVIEW', function() { console.error(errorMessage); });
});

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var watchForTag = require("tagWatcher").watchForTag;

// Output error message to console when using <adview> tag with no permission.
var errorMessage = "You do not have permission to use <adview> tag." +
  " Be sure to declare 'adview' permission in your manifest.";

window.addEventListener('DOMContentLoaded', function() {
  watchForTag('ADVIEW', function() {  console.error(errorMessage); });
});

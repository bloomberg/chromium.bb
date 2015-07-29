// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var logForDebugging = false;
function log(message) {
  if (logForDebugging)
    console.log(message);
}

log("Hello from the service worker");
// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var extension_id = "ldnnhddmnhbkjipkidpdiheffobcpfmf";

var assertEq = chrome.test.assertEq;
var assertNoLastError = chrome.test.assertNoLastError;
var succeed = chrome.test.succeed;

// Calls |callback| with true/false indicating whether an item with an id of
// extension_id is installed.
function checkInstalled(callback) {
  chrome.management.getAll(function(extensions) {
    var found = false;
    callback(extensions.some(function(ext) {
      return ext.id == extension_id;
    }));
  });
}

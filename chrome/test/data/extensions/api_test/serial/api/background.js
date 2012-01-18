// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testSerial = function() {
  var onClose = function(result) {
    chrome.test.assertTrue(result);
    chrome.test.succeed();
  }

  var onOpen = function(connectionInfo) {
    var connectionId = connectionInfo.connectionId;
    chrome.test.assertTrue(connectionId > 0);
    chrome.experimental.serial.close(connectionId, onClose);
  }

  chrome.experimental.serial.open("/dev/null", onOpen);
};

chrome.test.runTests([testSerial]);

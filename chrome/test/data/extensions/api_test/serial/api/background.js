// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var serialPort = null;

var testSerial = function() {
  var connectionId = -1;
  var testString = 'x';

  var onClose = function(result) {
    chrome.test.assertTrue(result);
    chrome.test.succeed();
  }

  var onRead = function(readInfo) {
    chrome.test.assertEq(testString, readInfo.message);
    chrome.experimental.serial.close(connectionId, onClose);
  }

  var onWrite = function(writeInfo) {
    chrome.test.assertEq(1, writeInfo.bytesWritten);
    chrome.experimental.serial.read(connectionId, onRead);
  }

  var onOpen = function(connectionInfo) {
    connectionId = connectionInfo.connectionId;
    chrome.test.assertTrue(connectionId > 0, 'Failed to open serial port.');
    chrome.experimental.serial.write(connectionId, testString, onWrite);
  }

  chrome.experimental.serial.open(serialPort, onOpen);
};

var onMessageReply = function(message) {
  serialPort = message;
  if (message == 'none') {
    chrome.test.runTests([]);
  } else {
    chrome.test.runTests([ testSerial ]);
  }
};

chrome.test.sendMessage("serial_port", onMessageReply);

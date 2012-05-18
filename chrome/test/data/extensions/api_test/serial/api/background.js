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

var testGetPorts = function() {
  var onGetPorts = function(ports) {
    // Any length is potentially valid, because we're on unknown hardware. But
    // we are testing at least that the ports member was filled in, so it's
    // still a somewhat meaningful test.
    chrome.test.assertTrue(ports.length >= 0);
    chrome.test.succeed();
  }

  chrome.experimental.serial.getPorts(onGetPorts);
};

var testMaybeOpenPort = function() {
  var onGetPorts = function(ports) {
    // We're testing as much as we can here without actually assuming the
    // existence of attached hardware.
    //
    // TODO(miket): is there any chance that just opening a serial port but not
    // doing anything could be harmful to devices attached to a developer's
    // machine?
    if (ports.length > 0) {
      var closedCount = ports.length;
      var onClose = function(r) {
        if (--closedCount == 0) {
          chrome.test.succeed();
        }
      };
      var onOpen = function(connectionInfo) {
        chrome.experimental.serial.close(connectionInfo.connectionId,
                                         onClose);
      };
      for (var i = 0; i < ports.length; i++) {
        chrome.experimental.serial.open(ports[i], onOpen);
      }
    } else {
      // There aren't any valid ports on this machine. That's OK.
      chrome.test.succeed();
    }
  }

  chrome.experimental.serial.getPorts(onGetPorts);
};

var onMessageReply = function(message) {
  serialPort = message;
  var generalTests = [ testGetPorts, testMaybeOpenPort ];

  chrome.test.runTests(generalTests);
  if (message != 'none') {
    // We have a specific serial port set up to respond to test traffic. This
    // is a rare situation. TODO(miket): mock to make it testable under any
    // hardware conditions.
    chrome.test.runTests([ testSerial ]);
  }
};

chrome.test.sendMessage("serial_port", onMessageReply);

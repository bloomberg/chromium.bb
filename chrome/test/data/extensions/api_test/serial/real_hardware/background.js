// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(miket): opening Bluetooth ports on OSX is unreliable. Investigate.
function shouldSkipPort(portName) {
  return portName.match(/[Bb]luetooth/);
}

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
      var currentPort = 0;

      var onFinishedWithPort = function() {
        if (currentPort >= ports.length)
          chrome.test.succeed();
        else
          testPort();
      };

      var onClose = function(r) {
        onFinishedWithPort();
      };

      var onOpen = function(connectionInfo) {
        var id = connectionInfo.connectionId;
        if (id > 0)
          chrome.experimental.serial.close(id, onClose);
        else
          onFinishedWithPort();
      };

      var testPort = function() {
        var port = ports[currentPort++];

        if (shouldSkipPort(port)) {
          onFinishedWithPort();
        } else {
          console.log("Opening serial device " + port);
          chrome.experimental.serial.open(port, onOpen);
        }
      }

      testPort();
    } else {
      // There aren't any valid ports on this machine. That's OK.
      chrome.test.succeed();
    }
  }

  chrome.experimental.serial.getPorts(onGetPorts);
};

var tests = [testGetPorts, testMaybeOpenPort];
chrome.test.runTests(tests);

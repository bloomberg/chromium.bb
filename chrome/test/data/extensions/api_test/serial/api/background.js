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
          onStartTest();
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

      var onStartTest = function() {
        var port = ports[currentPort++];

        if (shouldSkipPort(port)) {
          onFinishedWithPort();
        } else {
          console.log("Opening serial device " + port);
          chrome.experimental.serial.open(port, onOpen);
        }
      }

      onStartTest();
    } else {
      // There aren't any valid ports on this machine. That's OK.
      chrome.test.succeed();
    }
  }

  chrome.experimental.serial.getPorts(onGetPorts);
};

var testSerial = function() {
  var serialPort = null;
  var connectionId = -1;
  var testBuffer = new ArrayBuffer(1);
  var readTries = 10;

  var uint8View = new Uint8Array(testBuffer);
  uint8View[0] = 42;

  var onClose = function(result) {
    chrome.test.assertTrue(result);
    chrome.test.succeed();
  };

  var onFlush = function(result) {
    chrome.test.assertTrue(result);
    chrome.experimental.serial.close(connectionId, onClose);
  }

  var doRead = function() {
    chrome.experimental.serial.read(connectionId, onRead);
  }

  var onRead = function(readInfo) {
    if (readInfo.bytesRead == 1) {
      var messageUint8View = new Uint8Array(readInfo.data);
      chrome.test.assertEq(uint8View[0], messageUint8View[0],
                           'Byte read was not equal to byte written.');
      chrome.experimental.serial.flush(connectionId, onFlush);
    } else {
      if (--readTries > 0) {
        setTimeout(doRead, 100);
      } else {
        chrome.test.assertEq(1, readInfo.bytesRead,
                             'read() failed to return bytesRead 1.');
      }
    }
  };

  var onWrite = function(writeInfo) {
    chrome.test.assertEq(1, writeInfo.bytesWritten,
                         'Failed to write byte.');
    if (writeInfo.bytesWritten == 1) {
      doRead();
    } else
      chrome.experimental.serial.close(connectionId, onClose);
  };

  var onOpen = function(connectionInfo) {
    connectionId = connectionInfo.connectionId;
    chrome.test.assertTrue(connectionId > 0, 'Failed to open serial port.');
    chrome.experimental.serial.write(connectionId, testBuffer, onWrite);
  };

  var onGetPorts = function(ports) {
    if (ports.length > 0) {
      var portNumber = 0;
      while (portNumber < ports.length) {
        if (shouldSkipPort(ports[portNumber])) {
          portNumber++;
          continue;
        } else
          break;
      }
      if (portNumber < ports.length) {
        serialPort = ports[portNumber];
        console.log('Connecting to serial device at ' + serialPort);
        chrome.experimental.serial.open(serialPort, onOpen);
      } else {
        // We didn't find a port that we think we should try.
        chrome.test.succeed();
      }
    } else {
      // No serial device found. This is still considered a success because we
      // can't rely on specific hardware being present on the machine.
      chrome.test.succeed();
    }
  };

  chrome.experimental.serial.getPorts(onGetPorts);
};

var onMessageReply = function(message) {
  var tests = [testGetPorts, testMaybeOpenPort];

  // Another way to force the test to run.
  var runTest = false;

  if (runTest || message == 'echo_device_attached') {
    // We have a specific serial port set up to respond to test traffic. This
    // is a rare situation. TODO(miket): mock to make it testable under any
    // hardware conditions.
    tests.push(testSerial);
  }
  chrome.test.runTests(tests);
};

chrome.test.sendMessage('serial_port', onMessageReply);

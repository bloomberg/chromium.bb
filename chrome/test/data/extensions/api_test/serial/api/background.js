// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(miket): opening Bluetooth ports on OSX is unreliable. Investigate.
function shouldSkipPort(portName) {
  return portName.match(/[Bb]luetooth/);
}

var testSerial = function() {
  var serialPort = null;
  var connectionId = -1;
  var testBuffer = new ArrayBuffer(1);
  var readTries = 10;

  var uint8View = new Uint8Array(testBuffer);
  uint8View[0] = 42;

  var operation = 0;
  var doNextOperation = function() {
    switch (operation++) {
      case 0:
      chrome.experimental.serial.getPorts(onGetPorts);
      break;
      case 1:
      var bitrate = 57600;
      console.log('Opening serial device ' + serialPort + ' at ' +
                  bitrate + ' bps.');
      chrome.experimental.serial.open(serialPort, {bitrate: bitrate},
                                      onOpen);
      break;
      case 2:
      chrome.experimental.serial.setControlSignals(
          connectionId, {dtr: true}, onSetControlSignals);
      break;
      case 3:
      chrome.experimental.serial.getControlSignals(connectionId,
                                                   onGetControlSignals);
      break;
      case 4:
      chrome.experimental.serial.write(connectionId, testBuffer, onWrite);
      break;
      case 5:
      chrome.experimental.serial.read(connectionId, onRead);
      break;
      case 6:
      chrome.experimental.serial.flush(connectionId, onFlush);
      break;
      case 50:  // GOTO 4 EVER
      chrome.experimental.serial.close(connectionId, onClose);
      break;
      default:
      // Beware! If you forget to assign a case for your next test, the whole
      // test suite will appear to succeed!
      chrome.test.succeed();
      break;
    }
  }

  var skipToTearDown = function() {
    operation = 50;
    doNextOperation();
  };

  var repeatOperation = function() {
    operation--;
    doNextOperation();
  }

  var onClose = function(result) {
    chrome.test.assertTrue(result);
    doNextOperation();
  };

  var onFlush = function(result) {
    chrome.test.assertTrue(result);
    doNextOperation();
  }

  var onRead = function(readInfo) {
    if (readInfo.bytesRead == 1) {
      var messageUint8View = new Uint8Array(readInfo.data);
      chrome.test.assertEq(uint8View[0], messageUint8View[0],
                           'Byte read was not equal to byte written.');
      doNextOperation();
    } else {
      if (--readTries > 0)
        setTimeout(repeatOperation, 100);
      else
        chrome.test.assertTrue(false,
                               'read() failed to return bytesRead 1.');
    }
  };

  var onWrite = function(writeInfo) {
    chrome.test.assertEq(1, writeInfo.bytesWritten,
                         'Failed to write byte.');
    if (writeInfo.bytesWritten == 1)
      doNextOperation();
    else
      skipToTearDown();
  };

  var onGetControlSignals = function(options) {
    chrome.test.assertTrue(typeof options.dcd != 'undefined');
    chrome.test.assertTrue(typeof options.cts != 'undefined');
    doNextOperation();
  };

  var onSetControlSignals = function(result) {
    chrome.test.assertTrue(result);
    doNextOperation();
  };

  var onOpen = function(connectionInfo) {
    connectionId = connectionInfo.connectionId;
    chrome.test.assertTrue(connectionId > 0, 'Failed to open serial port.');
    doNextOperation();
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
        doNextOperation();
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

  doNextOperation();
};

chrome.test.runTests([testSerial]);

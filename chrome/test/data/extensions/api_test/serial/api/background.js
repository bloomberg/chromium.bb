// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const serial = chrome.serial;

// TODO(miket): opening Bluetooth ports on OSX is unreliable. Investigate.
function shouldSkipPort(portName) {
  return portName.match(/[Bb]luetooth/);
}

var createTestArrayBuffer = function() {
  var bufferSize = 8;
  var buffer = new ArrayBuffer(bufferSize);

  var uint8View = new Uint8Array(buffer);
  for (var i = 0; i < bufferSize; i++) {
    uint8View[i] = 42 + i * 2;  // An arbitrary pattern.
  }
  return buffer;
}

var testSerial = function() {
  var serialPort = null;
  var connectionId = -1;
  var readTries = 10;
  var writeBuffer = createTestArrayBuffer();
  var writeBufferUint8View = new Uint8Array(writeBuffer);
  var bufferLength = writeBufferUint8View.length;
  var readBuffer = new ArrayBuffer(bufferLength);
  var readBufferUint8View = new Uint8Array(readBuffer);
  var bytesToRead = bufferLength;

  var operation = 0;
  var doNextOperation = function() {
    switch (operation++) {
      case 0:
      serial.getPorts(onGetPorts);
      break;
      case 1:
      var bitrate = 57600;
      console.log('Opening serial device ' + serialPort + ' at ' +
                  bitrate + ' bps.');
      serial.open(serialPort, {bitrate: bitrate}, onOpen);
      break;
      case 2:
      serial.setControlSignals(connectionId, {dtr: true}, onSetControlSignals);
      break;
      case 3:
      serial.getControlSignals(connectionId,onGetControlSignals);
      break;
      case 4:
      serial.write(connectionId, writeBuffer, onWrite);
      break;
      case 5:
      serial.read(connectionId, bytesToRead, onRead);
      break;
      case 6:
      serial.flush(connectionId, onFlush);
      break;
      case 50:  // GOTO 4 EVER
      serial.close(connectionId, onClose);
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
    bytesToRead -= readInfo.bytesRead;
    var readBufferIndex = bufferLength - readInfo.bytesRead;
    var messageUint8View = new Uint8Array(readInfo.data);
    for (var i = 0; i < readInfo.bytesRead; i++)
      readBufferUint8View[i + readBufferIndex] = messageUint8View[i];
    if (bytesToRead == 0) {
      chrome.test.assertEq(writeBufferUint8View, readBufferUint8View,
                           'Buffer read was not equal to buffer written.');
      doNextOperation();
    } else {
      if (--readTries > 0)
        setTimeout(repeatOperation, 100);
      else
        chrome.test.assertTrue(
            false,
            'read() failed to return requested number of bytes.');
    }
  };

  var onWrite = function(writeInfo) {
    chrome.test.assertEq(bufferLength, writeInfo.bytesWritten,
                         'Failed to write byte.');
    doNextOperation();
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

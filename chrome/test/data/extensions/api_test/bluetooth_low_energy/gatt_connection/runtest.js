// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var deviceAddress0 = '11:22:33:44:55:66';
var deviceAddress1 = '77:88:99:AA:BB:CC';
var badDeviceAddress = 'bad-address';
var ble = chrome.bluetoothLowEnergy;

var errorAlreadyConnected = 'Already connected';
var errorNotConnected = 'Not connected';
var errorNotFound = 'Instance not found';
var errorOperationFailed = 'Operation failed';

function expectError(message) {
  if (!chrome.runtime.lastError ||
      chrome.runtime.lastError.message != message)
    chrome.test.fail('Expected error: ' + message);
}

function expectSuccess() {
  if (chrome.runtime.lastError)
    chrome.test.fail('Unexpected error: ' + chrome.runtime.lastError.message);
}

// Disconnecting from unconnected devices should fail.
ble.disconnect(deviceAddress0, function () {
  expectError(errorNotConnected);
  ble.disconnect(deviceAddress1, function () {
    expectError(errorNotConnected);
    ble.disconnect(badDeviceAddress, function () {
      expectError(errorNotConnected);

      // Connect to device that doesn't exist.
      ble.connect(badDeviceAddress, function () {
        expectError(errorNotFound);

        // Failed connect to device 1.
        ble.connect(deviceAddress0, function () {
          expectError(errorOperationFailed);

          // Successful connect to device 0.
          ble.connect(deviceAddress0, function () {
            expectSuccess();

            // Device 0 already connected.
            ble.connect(deviceAddress0, function() {
              expectError(errorAlreadyConnected);

              // Device 1 still disconnected.
              ble.disconnect(deviceAddress1, function () {
                expectError(errorNotConnected);

                // Successful connect to device 1.
                ble.connect(deviceAddress1, function () {
                  expectSuccess();

                  // Device 1 already connected.
                  ble.connect(deviceAddress1, function () {
                    expectError(errorAlreadyConnected);

                    // Successfully disconnect device 0.
                    ble.disconnect(deviceAddress0, function () {
                      expectSuccess();

                      // Cannot disconnect device 0.
                      ble.disconnect(deviceAddress0, function () {
                        expectError(errorNotConnected);

                        // Device 1 still connected.
                        ble.connect(deviceAddress1, function () {
                          expectError(errorAlreadyConnected);

                          // Successfully disconnect device 1.
                          ble.disconnect(deviceAddress1, function () {
                            expectSuccess();

                            // Cannot disconnect device 1.
                            ble.disconnect(deviceAddress1, function () {
                              expectError(errorNotConnected);

                              // Re-connect device 0.
                              ble.connect(deviceAddress0, function () {
                                expectSuccess();
                                chrome.test.succeed();
                              });
                            });
                          });
                        });
                      });
                    });
                  });
                });
              });
            });
          });
        })
      });
    });
  });
});

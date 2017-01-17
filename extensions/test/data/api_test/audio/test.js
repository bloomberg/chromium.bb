// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Asserts that device property values match properties in |expectedProperties|.
 * The method will *not* assert that the device contains *only* properties
 * specified in expected properties.
 * @param {Object} expectedProperties Expected device properties.
 * @param {Object} device Device object to test.
 */
function assertDeviceMatches(expectedProperties, device) {
  Object.keys(expectedProperties).forEach(function(key) {
    chrome.test.assertEq(expectedProperties[key], device[key],
        'Property ' + key + ' of device ' + device.id);
  });
}

/**
 * Verifies that list of devices contains all and only devices from set of
 * expected devices. If will fail the test if an unexpected device is found.
 *
 * @param {Object.<string, Object>} expectedDevices Expected set of test
 *     devices. Maps device ID to device properties.
 * @param {Array.<Object>} devices List of input devices.
 */
function assertDevicesMatch(expectedDevices, devices) {
  var deviceIds = {};
  devices.forEach(function(device) {
    chrome.test.assertFalse(!!deviceIds[device.id],
                            'Duplicated device id: \'' + device.id + '\'.');
    deviceIds[device.id] = true;
  });

  function sortedKeys(obj) {
    return Object.keys(obj).sort();
  }
  chrome.test.assertEq(sortedKeys(expectedDevices), sortedKeys(deviceIds));

  devices.forEach(function(device) {
    assertDeviceMatches(expectedDevices[device.id], device);
  });
}

/**
 *
 * @param {Array.<Object>} devices List of devices returned by getInfo
 * @returns {Object.<string, Object>} List of devices formatted as map of
 *      expected devices used to assert devices match expectation.
 */
function deviceListToExpectedDevicesMap(devices) {
  var expectedDevicesMap = {};
  devices.forEach(function(device) {
    expectedDevicesMap[device.id] = device;
  });
  return expectedDevicesMap;
}

function getActiveDeviceIds(deviceList) {
  return deviceList
      .filter(function(device) {return device.isActive;})
      .map(function(device) {return device.id});
}

function getDevices(callback) {
  chrome.audio.getInfo(chrome.test.callbackPass(callback));
}

chrome.test.runTests([
  function getInfoTest() {
    // Test output devices. Maps device ID -> tested device properties.
    var kTestOutputDevices = {
      '30001': {
        id: '30001',
        name: 'Jabra Speaker: Jabra Speaker 1'
      },
      '30002': {
        id: '30002',
        name: 'Jabra Speaker: Jabra Speaker 2'
      },
      '30003': {
        id: '30003',
        name: 'HDMI output: HDA Intel MID'
      }
    };

    // Test input devices. Maps device ID -> tested device properties.
    var kTestInputDevices = {
      '40001': {
        id: '40001',
        name: 'Jabra Mic: Jabra Mic 1'
      },
      '40002': {
        id: '40002',
        name: 'Jabra Mic: Jabra Mic 2'
      },
      '40003': {
        id: '40003',
        name: 'Webcam Mic: Logitech Webcam'
      }
    };

    getDevices(function(outputInfo, inputInfo) {
      assertDevicesMatch(kTestOutputDevices, outputInfo);
      assertDevicesMatch(kTestInputDevices, inputInfo);
    });
  },

  function deprecatedSetActiveDevicesTest() {
    //Test output devices. Maps device ID -> tested device properties.
    var kTestOutputDevices = {
      '30001': {
        id: '30001',
        isActive: false
      },
      '30002': {
        id: '30002',
        isActive: false
      },
      '30003': {
        id: '30003',
        isActive: true
      }
    };

    // Test input devices. Maps device ID -> tested device properties.
    var kTestInputDevices = {
      '40001': {
        id: '40001',
        isActive: false
      },
      '40002': {
        id: '40002',
        isActive: true
      },
      '40003': {
        id: '40003',
        isActive: false
      }
    };

    chrome.audio.setActiveDevices([
      '30003',
      '40002'
    ], chrome.test.callbackPass(function() {
      getDevices(function(outputInfo, inputInfo) {
        assertDevicesMatch(kTestOutputDevices, outputInfo);
        assertDevicesMatch(kTestInputDevices, inputInfo);
      });
    }));
  },

  function setPropertiesTest() {
    getDevices(function(originalOutputInfo, originalInputInfo) {
      var expectedInput = deviceListToExpectedDevicesMap(originalInputInfo);
      // Update expected input devices with values that should be changed in
      // test.
      var updatedInput = expectedInput['40002'];
      chrome.test.assertFalse(updatedInput.isMuted);
      chrome.test.assertFalse(updatedInput.gain === 55);
      updatedInput.isMuted = true;
      updatedInput.gain = 55;

      var expectedOutput = deviceListToExpectedDevicesMap(originalOutputInfo);
      // Update expected output devices with values that should be changed in
      // test.
      var updatedOutput = expectedOutput['30001'];
      chrome.test.assertFalse(updatedOutput.isMuted);
      chrome.test.assertFalse(updatedOutput.volume === 35);
      updatedOutput.isMuted = true;
      updatedOutput.volume = 35;

      chrome.audio.setProperties('30001', {
        isMuted: true,
        volume: 35
      }, chrome.test.callbackPass(function() {
        chrome.audio.setProperties('40002', {
          isMuted: true,
          gain: 55
        }, chrome.test.callbackPass(function() {
          getDevices(
            chrome.test.callbackPass(function(outputInfo, inputInfo) {
              assertDevicesMatch(expectedInput, inputInfo);
              assertDevicesMatch(expectedOutput, outputInfo);
            }));
        }));
      }));
    });
  },

  function setPropertiesInvalidValuesTest() {
    getDevices(function(originalOutputInfo, originalInputInfo) {
      var expectedInput = deviceListToExpectedDevicesMap(originalInputInfo);
      var expectedOutput = deviceListToExpectedDevicesMap(originalOutputInfo);

      chrome.audio.setProperties('30001', {
        isMuted: true,
        // Output device - should have volume set.
        gain: 55
      }, chrome.test.callbackFail('Could not set properties', function() {
        chrome.audio.setProperties('40002', {
          isMuted: true,
          // Input device - should have gain set.
          volume:55
        }, chrome.test.callbackFail('Could not set properties', function() {
          // Assert that device properties haven't changed.
          getDevices(function(outputInfo, inputInfo) {
            assertDevicesMatch(expectedOutput, outputInfo);
            assertDevicesMatch(expectedInput, inputInfo);
          });
        }));
      }));
    });
  },

  function setActiveDevicesTest() {
    chrome.audio.setActiveDevices({
      input: ['40002', '40003'],
      output: ['30001']
    }, chrome.test.callbackPass(function() {
      getDevices(function(outputs, inputs) {
        chrome.test.assertEq(
            ['40002', '40003'], getActiveDeviceIds(inputs).sort());
        chrome.test.assertEq(['30001'], getActiveDeviceIds(outputs));
      });
    }));
  },

  function setActiveDevicesOutputOnlyTest() {
    getDevices(function(originalOutputs, originalInputs) {
      var originalActiveInputs = getActiveDeviceIds(originalInputs);
      chrome.test.assertTrue(originalActiveInputs.length > 0);

      chrome.audio.setActiveDevices({
        output: ['30003']
      }, chrome.test.callbackPass(function() {
        getDevices(function(outputs, inputs) {
          chrome.test.assertEq(
              originalActiveInputs.sort(), getActiveDeviceIds(inputs).sort());
          chrome.test.assertEq(['30003'], getActiveDeviceIds(outputs));
        });
      }));
    });
  },

  function setActiveDevicesFailInputTest() {
    getDevices(function(originalOutputs, originalInputs) {
      var originalActiveInputs = getActiveDeviceIds(originalInputs).sort();
      chrome.test.assertTrue(originalActiveInputs.length > 0);

      var originalActiveOutputs = getActiveDeviceIds(originalOutputs).sort();
      chrome.test.assertTrue(originalActiveOutputs.length > 0);

      chrome.audio.setActiveDevices({
        input: ['0000000'],  /* does not exist */
        output: []
      }, chrome.test.callbackFail('Failed to set active devices.', function() {
        getDevices(function(outputs, inputs) {
          chrome.test.assertEq(
              originalActiveInputs, getActiveDeviceIds(inputs).sort());
          chrome.test.assertEq(
              originalActiveOutputs, getActiveDeviceIds(outputs).sort());
        });
      }));
    });
  },

  function setActiveDevicesFailOutputTest() {
    getDevices(function(originalOutputs, originalInputs) {
      var originalActiveInputs = getActiveDeviceIds(originalInputs).sort();
      chrome.test.assertTrue(originalActiveInputs.length > 0);

      var originalActiveOutputs = getActiveDeviceIds(originalOutputs).sort();
      chrome.test.assertTrue(originalActiveOutputs.length > 0);

      chrome.audio.setActiveDevices({
        input: [],
        output: ['40001'] /* id is input node ID */
      }, chrome.test.callbackFail('Failed to set active devices.', function() {
        getDevices(function(outputs, inputs) {
          chrome.test.assertEq(
              originalActiveInputs, getActiveDeviceIds(inputs).sort());
          chrome.test.assertEq(
              originalActiveOutputs, getActiveDeviceIds(outputs).sort());
        });
      }));
    });
  },

  function clearActiveDevicesTest() {
    getDevices(function(originalOutputs, originalInputs) {
      chrome.test.assertTrue(getActiveDeviceIds(originalInputs).length > 0);
      chrome.test.assertTrue(getActiveDeviceIds(originalOutputs).length > 0);

      chrome.audio.setActiveDevices({
        input: [],
        output: []
      }, chrome.test.callbackPass(function() {
        getDevices(function(outputs, inputs) {
          chrome.test.assertEq([], getActiveDeviceIds(inputs));
          chrome.test.assertEq([], getActiveDeviceIds(outputs));
        });
      }));
    });
  },
]);

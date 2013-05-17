// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function verifyActiveDevices(output_id, input_id) {
  chrome.audio.getInfo(
      chrome.test.callbackPass(function(outputInfo, inputInfo) {
        var outputFound = false;
        var inputFound = false;
        for (var i = 0; i < outputInfo.length; ++i) {
          if (outputInfo[i].isActive) {
            chrome.test.assertTrue(outputInfo[i].id == output_id);
            outputFound = true;
          }
        }
        for (var i = 0; i < inputInfo.length; ++i) {
          if (inputInfo[i].isActive) {
            chrome.test.assertTrue(inputInfo[i].id == input_id);
            inputFound = true;
          }
        }
        chrome.test.assertTrue(outputFound);
        chrome.test.assertTrue(inputFound);
  }));
}


function verifyDeviceProperties(output_id, input_id,
                                output_props, input_props) {
  chrome.audio.getInfo(
      chrome.test.callbackPass(function(outputInfo, inputInfo) {
        var outputFound = false;
        var inputFound = false;
        for (var i = 0; i < outputInfo.length; ++i) {
          if (outputInfo[i].id == output_id) {
            chrome.test.assertTrue(outputInfo[i].volume == output_props.volume);
            chrome.test.assertTrue(
                outputInfo[i].isMuted == output_props.isMuted);
            outputFound = true;
          }
        }
        for (var i = 0; i < inputInfo.length; ++i) {
          if (inputInfo[i].id == input_id) {
            chrome.test.assertTrue(inputInfo[i].gain == input_props.gain);
            chrome.test.assertTrue(inputInfo[i].isMuted == input_props.isMuted);
            inputFound = true;
          }
        }
        chrome.test.assertTrue(outputFound);
        chrome.test.assertTrue(inputFound);
  }));
}


function setActiveDevicesAndVerify(output_id, input_id) {
  chrome.audio.setActiveDevices([output_id, input_id],
      chrome.test.callbackPass(function() {
        verifyActiveDevices(output_id, input_id);
  }));
}

function setPropertiesAndVerify(outputInfo, inputInfo) {
  var outputProps = new Object;
  outputProps.isMuted = true;
  outputProps.volume = 55;
  chrome.audio.setProperties(outputInfo.id, outputProps,
      chrome.test.callbackPass(function() {
    // Once the output properties have been set, set input properties, then
    // verify.
    var inputProps = new Object;
    inputProps.isMuted = true;
    inputProps.gain = 35;
    chrome.audio.setProperties(inputInfo.id, inputProps,
        chrome.test.callbackPass(function() {
          verifyDeviceProperties(outputInfo.id, inputInfo.id,
                                 outputProps, inputProps);
    }));
  }));
}

chrome.test.runTests([
  function getInfoTest() {
    chrome.audio.getInfo(
        chrome.test.callbackPass(function(outputInfo, inputInfo) {
    }));
  },

  function setActiveDevicesTest() {
    chrome.audio.getInfo(
        chrome.test.callbackPass(function(outputInfo, inputInfo) {
          setActiveDevicesAndVerify(outputInfo[2].id, inputInfo[1].id);
    }));
  },

  function setPropertiesTest() {
    chrome.audio.getInfo(
        chrome.test.callbackPass(function(outputInfo, inputInfo) {
          setPropertiesAndVerify(outputInfo[0], inputInfo[1]);
    }));
  }
]);

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function verifyGetInfoOutput(outputs) {
  chrome.test.assertEq("30001", outputs[0].id);
  chrome.test.assertEq("Jabra Speaker: Jabra Speaker 1", outputs[0].name);

  chrome.test.assertEq("30002", outputs[1].id);
  chrome.test.assertEq("Jabra Speaker: Jabra Speaker 2", outputs[1].name);

  chrome.test.assertEq("30003", outputs[2].id);
  chrome.test.assertEq("HDMI output: HDA Intel MID", outputs[2].name);
}

function verifyGetInfoInput(inputs) {
  chrome.test.assertEq("40001", inputs[0].id);
  chrome.test.assertEq("Jabra Mic: Jabra Mic 1", inputs[0].name);

  chrome.test.assertEq("40002", inputs[1].id);
  chrome.test.assertEq("Jabra Mic: Jabra Mic 2", inputs[1].name);

  chrome.test.assertEq("40003", inputs[2].id);
  chrome.test.assertEq("Webcam Mic: Logitech Webcam", inputs[2].name);
}

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
            chrome.test.assertEq(output_props.volume, outputInfo[i].volume);
            chrome.test.assertEq(output_props.isMuted, outputInfo[i].isMuted);
            outputFound = true;
          }
        }
        for (var i = 0; i < inputInfo.length; ++i) {
          if (inputInfo[i].id == input_id) {
            chrome.test.assertEq(input_props.gain, inputInfo[i].gain);
            chrome.test.assertEq(input_props.isMuted, inputInfo[i].isMuted);
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
          verifyGetInfoOutput(outputInfo);
          verifyGetInfoInput(inputInfo);
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

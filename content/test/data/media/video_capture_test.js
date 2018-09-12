// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

$ = function(id) {
  return document.getElementById(id);
};

const WIDTH = 320;
var CONSTRAINTS = {video: {width: {exact: WIDTH}}};
var hasReceivedTrackEndedEvent = false;

function startVideoCaptureAndVerifySize() {
  console.log('Calling getUserMediaAndWaitForVideoRendering.');
  navigator.mediaDevices.getUserMedia(CONSTRAINTS)
      .then(gotStreamCallback)
      .catch(failedCallback);
}

function startVideoCaptureFromDeviceNamedVirtualDeviceAndVerifySize() {
  console.log('Trying to find device named "Virtual Device".');
  navigator.mediaDevices.enumerateDevices().then(function(devices) {
    var target_device;
    devices.forEach(function(device) {
      if (device.kind == 'videoinput') {
        console.log('Found videoinput device with label ' + device.label);
        if (device.label == 'Virtual Device') {
          target_device = device;
        }
      }
    });
    if (target_device == null) {
      failTest(
          'No video input device was found with label = Virtual ' +
          'Device');
      return;
    }
    var device_specific_constraints = {
      video: {width: {exact: WIDTH}, deviceId: {exact: target_device.deviceId}}
    };
    navigator.mediaDevices.getUserMedia(device_specific_constraints)
        .then(gotStreamCallback)
        .catch(failedCallback);
  });
}

function enumerateVideoCaptureDevicesAndVerifyCount(expected_count) {
  console.log('Enumerating devices and verifying count.');
  navigator.mediaDevices.enumerateDevices().then(function(devices) {
    var actual_count = 0;
    devices.forEach(function(device) {
      if (device.kind == 'videoinput') {
        console.log('Found videoinput device with label ' + device.label);
        actual_count = actual_count + 1;
      }
    });
    if (actual_count == expected_count) {
      reportTestSuccess();
    } else {
      failTest(
          'Device count ' + actual_count + ' did not match expectation of ' +
          expected_count);
    }
  });
}

function failedCallback(error) {
  failTest('GetUserMedia call failed with code ' + error.code);
}

function gotStreamCallback(stream) {
  var localView = $('local-view');
  localView.srcObject = stream;

  var videoTracks = stream.getVideoTracks();
  if (videoTracks.length == 0) {
    failTest('Did not receive any video tracks');
  }
  var videoTrack = videoTracks[0];
  videoTrack.onended = function() {
    hasReceivedTrackEndedEvent = true;
  };

  detectVideoPlaying('local-view').then(() => {
    if (localView.videoWidth == WIDTH) {
      reportTestSuccess();
    } else {
      failTest('Video has unexpected width.');
    }
  });
}

function waitForVideoToTurnBlack() {
  detectBlackVideo('local-view').then(() => {
    reportTestSuccess();
  });
}

function verifyHasReceivedTrackEndedEvent() {
  if (hasReceivedTrackEndedEvent) {
    reportTestSuccess();
  } else {
    failTest('Did not receive ended event from track.');
  }
}

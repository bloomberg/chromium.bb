/**
 * Copyright 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Queries for media devices on the current system using the getMediaDevices
 * API.
 *
 * Returns the list of devices available.
 */
function getMediaDevices() {
  navigator.getMediaDevices(function(devices) {
    returnToTest(JSON.stringify(devices));
  });
}

/**
 * Queries for media sources on the current system using the getSources API.
 *
 * Returns the list of sources available.
 */
function getSources() {
  MediaStreamTrack.getSources(function(sources) {
    returnToTest(JSON.stringify(sources));
  });
}

/**
 * Queries for video input devices on the current system using the
 * getSources API.
 *
 * This does not guarantee that a getUserMedia with video will succeed, as the
 * camera could be busy for instance.
 *
 * Returns has-video-input-device to the test if there is a webcam available,
 * no-video-input-devices otherwise.
 */
function hasVideoInputDeviceOnSystem() {
  MediaStreamTrack.getSources(function(devices) {
    var hasVideoInputDevice = false;
    devices.forEach(function(device) {
      if (device.kind == 'video')
        hasVideoInputDevice = true;
    });

    if (hasVideoInputDevice)
      returnToTest('has-video-input-device');
    else
      returnToTest('no-video-input-devices');
  });
}

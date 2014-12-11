// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 */
function WebRTCDeviceProvider() {
}

WebRTCDeviceProvider.prototype.handleCommandFailure = function() {
  // TODO(serya): Remove debugging code and implement.
  console.error('Command failed');
};

WebRTCDeviceProvider.prototype.handleCommandSuccess = function(result) {
  // TODO(serya): Remove debugging code and implement.
  console.log('Command succeded', result);
};

WebRTCDeviceProvider.prototype.startSessionIfNeeded = function(deviceId) {
  // TODO(serya): Remove debugging code and implement.
  this.lastDeviceId = deviceId;
};

addEventListener('DOMContentLoaded', function() {
  window.WebRTCDeviceProvider.instance = new WebRTCDeviceProvider();
}, false);

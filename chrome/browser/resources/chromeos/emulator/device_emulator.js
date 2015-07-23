// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('device_emulator', function() {
  'use strict';

  var batterySettings = $('battery-settings');

  function initialize() {
    batterySettings = $('battery-settings');
    chrome.send('requestPowerInfo');

    document.querySelector('.menu-item-toggle')
        .addEventListener('click', function(e) {
      // Show/hide the element's content.
    });
  }

  // Return an object with all of the exports.
  return {
    initialize: initialize,
    batterySettings: batterySettings,
  };
});

document.addEventListener('DOMContentLoaded', device_emulator.initialize);

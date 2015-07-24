// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('device_emulator', function() {
  'use strict';

  var batterySettings = $('battery-settings');
  var bluetoothSettings = $('bluetooth-settings');

  function initialize() {
    chrome.send('requestPowerInfo');

    var toggles = document.getElementsByClassName('menu-item-toggle');
    for (var i = 0; i < toggles.length; ++i) {
        toggles[i].addEventListener('click', handleDrawerItemClick);
    }
  }

  /**
   * Shows/hides a sidebar elements designated content.
   * The content is identified by the |data-content-id| attribute of the
   * sidebar element. This value is the ID of the HTML element to be toggled.
   * @param {Event} e Contains information about the event which was fired.
   */
  function handleDrawerItemClick(e) {
    var content = $(e.target.dataset.contentId);
    content.hidden = !content.hidden;
  }

  // Return an object with all of the exports.
  return {
    initialize: initialize,
    batterySettings: batterySettings,
    bluetoothSettings: bluetoothSettings,
  };
});

document.addEventListener('DOMContentLoaded', device_emulator.initialize);

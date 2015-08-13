// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('device_emulator', function() {
  'use strict';

  var audioSettings = $('audio-settings');
  var batterySettings = $('battery-settings');
  var bluetoothSettings = $('bluetooth-settings');

  function initialize() {
    audioSettings.initialize();
    batterySettings.initialize();
    bluetoothSettings.initialize();

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
    var contentId = e.target.dataset.contentId;
    var card = $(contentId);
    card.hidden = !card.hidden;
  }

  // Return an object with all of the exports.
  return {
    initialize: initialize,
    audioSettings: audioSettings,
    batterySettings: batterySettings,
    bluetoothSettings: bluetoothSettings,
  };
});

document.addEventListener('DOMContentLoaded', device_emulator.initialize);

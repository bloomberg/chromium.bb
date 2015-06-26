// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('device_emulator', function() {
  'use strict';

  /**
   * Updates the UI with the battery status.
   * @param {number} percent Battery percentage (out of 100).
   */
  function setBatteryInfo(percent) {
    var slider = $('battery-percent-slider');
    var text = $('battery-percent-text');

    slider.valueAsNumber = percent;
    text.valueAsNumber = percent;
  }

  /**
   * Event listener fired when the battery percent slider is moved and the mouse
   * is released. Updates the Chrome OS UI.
   * @param {Event} event Contains information about the event which was fired.
   */
  function onBatterySliderChange(event) {
    var slider = event.target;
    chrome.send('updateBatteryInfo', [slider.valueAsNumer]);
  }

  /**
   * Event listener fired when the battery percent slider is moved. Updates
   * the battery slider's associated text input.
   * @param {Event} event Contains information about the event which was fired.
   */
  function onBatterySliderInput(event) {
    var slider = event.target;
    var text = $('battery-percent-text');

    text.value = slider.value;
  }

  /**
   * Event listener fired when a percentage is entered in the battery
   * percentage text input. Updates the slider and ChromeOS UI.
   * @param {Event} event Contains information about the event which was fired.
   */
  function onBatteryTextInput(event) {
    var text = event.target;
    var slider = $('battery-percent-slider');
    var percent = text.valueAsNumber;

    if (isNaN(percent)) {
      percent = 0;
      text.valueAsNumber = 0;
    }

    slider.value = percent;

    chrome.send('updateBatteryInfo', [percent]);
  }

  function initialize() {
    chrome.send('requestBatteryInfo');

    wireEvents();
    initializeControls();
  }

  /**
   * Initializes any form controls as necessary.
   */
  function initializeControls() {
    // Initialize the Power Source select box
    var select = $('power-source-select');
    var disconnectedOptionValue = loadTimeData.getString('disconnected');
    var usbPowerOptionValue = loadTimeData.getString('usbPower');
    var acPowerOptionValue = loadTimeData.getString('acPower');

    select.appendChild(createOptionForSelect(acPowerOptionValue,
        'AC Power (Main/Line Power Connected)'));
    select.appendChild(createOptionForSelect(usbPowerOptionValue,
        'USB Power'));
    select.appendChild(createOptionForSelect(disconnectedOptionValue,
        'Disconnected (No external power source)'));

    select.value = disconnectedOptionValue;
  }

  /**
   * A helper function to create and return an <option> node
   * to be added to a select box.
   * @param {string} value Will be the <option>'s value attribute.
   * @param {string} text Will be the <option>'s innerHTML attribute.
   */
  function createOptionForSelect(value, text) {
    var opt = document.createElement('option');
    opt.value = value;
    opt.innerHTML = text;

    return opt;
  }

  /**
   * Sets up all event listeners for the page.
   */
  function wireEvents() {
    var slider = $('battery-percent-slider');
    var text = $('battery-percent-text');

    slider.addEventListener('change', onBatterySliderChange);
    slider.addEventListener('input', onBatterySliderInput);
    text.addEventListener('input', onBatteryTextInput);
  }

  // Return an object with all of the exports.
  return {
    initialize: initialize,
    setBatteryInfo: setBatteryInfo,
  };
});

document.addEventListener('DOMContentLoaded', device_emulator.initialize);

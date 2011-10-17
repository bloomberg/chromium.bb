// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var OptionsPage = options.OptionsPage;
  /////////////////////////////////////////////////////////////////////////////
  // SystemOptions class:

  /**
   * Encapsulated handling of ChromeOS system options page.
   * @constructor
   */

  function SystemOptions() {
    OptionsPage.call(this, 'system', templateData.systemPageTabTitle,
                     'systemPage');
  }

  cr.addSingletonGetter(SystemOptions);

  // Inherit SystemOptions from OptionsPage.
  SystemOptions.prototype = {
    __proto__: options.OptionsPage.prototype,

    /**
     * Initializes SystemOptions page.
     * Calls base class implementation to starts preference initialization.
     */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      // Disable time-related settings if we're not logged in as a real user.
      if (AccountsOptions.loggedInAsGuest()) {
        var timezone = $('timezone-select');
        if (timezone)
          timezone.disabled = true;
        var use_24hour_clock = $('use-24hour-clock');
        if (use_24hour_clock)
          use_24hour_clock.disabled = true;
      }

      options.system.bluetooth.BluetoothListElement.decorate(
          $('bluetooth-device-list'));

      // TODO(kevers): Populate list of connected bluetooth devices.
      //               Set state of 'Enable bluetooth' checkbox.

      $('bluetooth-find-devices').disabled =
          $('enable-bluetooth-label') ? false : true;
      $('bluetooth-find-devices').onclick = function(event) {
        findBluetoothDevices_();
      };

      $('language-button').onclick = function(event) {
        OptionsPage.navigateToPage('language');
      };
      $('modifier-keys-button').onclick = function(event) {
        OptionsPage.navigateToPage('languageCustomizeModifierKeysOverlay');
      };
      $('accesibility-check').onchange = function(event) {
        chrome.send('accessibilityChange',
                    [String($('accesibility-check').checked)]);
      };

      if (cr.isTouch) {
        initializeBrightnessButton_('brightness-decrease-button',
            'decreaseScreenBrightness');
        initializeBrightnessButton_('brightness-increase-button',
            'increaseScreenBrightness');
      }
    }
  };

  /**
   * Initializes a button for controlling screen brightness on touch builds of
   * ChromeOS.
   * @param {string} id Button ID.
   * @param {string} callback Name of the callback function.
   */
  function initializeBrightnessButton_(id, callback) {
    // TODO(kevers): Make brightness buttons auto-repeat if held.
    $(id).onclick = function(event) {
      chrome.send(callback);
    }
  }

  /**
   * Scan for bluetooth devices.
   * @private
   */
  function findBluetoothDevices_() {
    setVisibility_('bluetooth-scanning-label', true);
    setVisibility_('bluetooth-scanning-icon', true);

    // Remove devices that are not currently connected.
    var devices = $('bluetooth-device-list').childNodes;
    for (var i = devices.length - 1; i >= 0; i--) {
      var device = devices.item(i);
      var data = device.data;
      if (!data || data.status !== 'connected')
        $('bluetooth-device-list').removeChild(device);
    }
    // TODO(kevers): Set arguments to a list of the currently connected
    //               devices.
    chrome.send('findBluetoothDevices');
  }

  /**
   * Sets the visibility of an element.
   * @param {string} id The id of the element.
   * @param {boolean} visible True if the element should be made visible.
   * @private
   */
  function setVisibility_(id, visible) {
    if (visible)
      $(id).classList.remove("transparent");
    else
      $(id).classList.add("transparent");
  }

  //
  // Chrome callbacks
  //

  /**
   * Set the initial state of the accessibility checkbox.
   */
  SystemOptions.SetAccessibilityCheckboxState = function(checked) {
    $('accesibility-check').checked = checked;
  };

  /**
   * Activate the bluetooth settings section on the System settings page.
   */
  SystemOptions.showBluetoothSettings = function() {
    $('bluetooth-devices').hidden = false;
  }

  /**
   * Adds an element to the list of available bluetooth devices.
   * @param{{'deviceName': string,
   *         'deviceId': string,
   *         'deviceType': Constants.DEVICE_TYPE,
   *         'deviceStatus': Constants.DEVICE_STATUS} device
   *     Decription of the bluetooth device.
   */
  SystemOptions.addBluetoothDevice = function(device) {
    $('bluetooth-device-list').appendDevice(device);
  }

  /**
   * Hides the scanning label and icon that are used to indicate that a device
   * search is in progress.
   */
  SystemOptions.notifyBluetoothSearchComplete = function() {
    // TDOO (kevers) - Reset state immediately once results are received
    //                 asynchronously.
    setTimeout(function() {
      setVisibility_('bluetooth-scanning-label', false);
      setVisibility_('bluetooth-scanning-icon', false);
    }, 2000);
  }

  // Export
  return {
    SystemOptions: SystemOptions
  };

});

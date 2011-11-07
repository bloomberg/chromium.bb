// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var OptionsPage = options.OptionsPage;
  var RepeatingButton = cr.ui.RepeatingButton;

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
      $('bluetooth-find-devices').onclick = function(event) {
        findBluetoothDevices_();
      };
      $('enable-bluetooth-check').onchange = function(event) {
        chrome.send('bluetoothEnableChange',
                    [Boolean($('enable-bluetooth-check').checked)]);
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
      initializeBrightnessButton_('brightness-decrease-button',
          'decreaseScreenBrightness');
      initializeBrightnessButton_('brightness-increase-button',
          'increaseScreenBrightness');
    }
  };

  /**
   * Initializes a button for controlling screen brightness.
   * @param {string} id Button ID.
   * @param {string} callback Name of the callback function.
   */
  function initializeBrightnessButton_(id, callback) {
    var button = $(id);
    cr.ui.decorate(button, RepeatingButton);
    button.addEventListener(RepeatingButton.Event.BUTTON_HELD, function(e) {
      chrome.send(callback);
    });
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
  };

  /**
   * Sets the state of the checkbox indicating if bluetooth is turned on. The
   * state of the "Find devices" button and the list of discovered devices may
   * also be affected by a change to the state.
   * @param {boolean} checked Flag Indicating if Bluetooth is turned on.
   */
  SystemOptions.setBluetoothCheckboxState = function(checked) {
    $('enable-bluetooth-check').checked = checked;
    $('bluetooth-find-devices').disabled = !checked;
    // Flush list of previously discovered devices if bluetooth is turned off.
    if (!checked) {
      var devices = $('bluetooth-device-list').childNodes;
      for (var i = devices.length - 1; i >= 0; i--) {
        var device = devices.item(i);
        $('bluetooth-device-list').removeChild(device);
      }
    }
  }

  /**
   * Adds an element to the list of available bluetooth devices.
   * @param {{name: string,
   *          address: string,
   *          icon: string,
   *          paired: boolean,
   *          connected: boolean}} device
   *     Decription of the bluetooth device.
   */
  SystemOptions.addBluetoothDevice = function(device) {
    $('bluetooth-device-list').appendDevice(device);
  };

  /**
   * Updates the state of a Bluetooth device.
   * @param {{name: string,
   *          address: string,
   *          icon: string,
   *          paired: boolean,
   *          connected: boolean}} device
   *     Decription of the bluetooth device.
   * @param {'pairing': string,
   *         'passkey': number,
   *         'entered': number} op
   *     Description of the pairing operation.
   */
  SystemOptions.connectBluetoothDevice = function(device, op) {
    var data = {};
    for (var key in device)
      data[key] = device[key];
    for (var key in op)
      data[key] = op[key];
    // Replace the existing element for the device.
    SystemOptions.addBluetoothDevice(data);
  }

  /**
   * Hides the scanning label and icon that are used to indicate that a device
   * search is in progress.
   */
  SystemOptions.notifyBluetoothSearchComplete = function() {
    setVisibility_('bluetooth-scanning-label', false);
    setVisibility_('bluetooth-scanning-icon', false);
  };

  /**
   * Displays the Touchpad Controls section when we detect a touchpad.
   */
  SystemOptions.showTouchpadControls = function() {
    $('touchpad-controls').hidden = false;
  };

  // Export
  return {
    SystemOptions: SystemOptions
  };

});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
     * Flag indicating if currently scanning for Bluetooth devices.
     * @type {boolean}
     */
    isScanning_: false,

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

      options.system.bluetooth.BluetoothDeviceList.decorate(
          $('bluetooth-paired-devices-list'));

      $('bluetooth-add-device').onclick = function(event) {
        if (! this.isScanning_)
          findBluetoothDevices_(true);
        OptionsPage.navigateToPage('bluetooth');
      };
      $('enable-bluetooth').onclick = function(event) {
        chrome.send('bluetoothEnableChange', [Boolean(true)]);
      };
      $('disable-bluetooth').onclick = function(event) {
        chrome.send('bluetoothEnableChange', [Boolean(false)]);
      };
      $('language-button').onclick = function(event) {
        OptionsPage.navigateToPage('language');
      };
      $('modifier-keys-button').onclick = function(event) {
        OptionsPage.navigateToPage('languageCustomizeModifierKeysOverlay');
      };
      $('accessibility-spoken-feedback-check').onchange = function(event) {
        chrome.send('spokenFeedbackChange',
                    [$('accessibility-spoken-feedback-check').checked]);
      };
      $('accessibility-high-contrast-check').onchange = function(event) {
        chrome.send('highContrastChange',
                    [$('accessibility-high-contrast-check').checked]);
      };
      $('accessibility-screen-magnifier-check').onchange = function(event) {
        chrome.send('screenMagnifierChange',
                    [$('accessibility-screen-magnifier-check').checked]);
      };
      $('accessibility-virtual-keyboard-check').onchange = function(event) {
        chrome.send('virtualKeyboardChange',
                    [$('accessibility-virtual-keyboard-check').checked]);
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
    button.repeatInterval = 300;
    button.addEventListener(RepeatingButton.Event.BUTTON_HELD, function(e) {
      chrome.send(callback);
    });
  }

  /**
   * Scan for bluetooth devices.
   * @param {boolean} reset Indicates if the list of unpaired devices should be
   *     cleared.
   * @private
   */
  function findBluetoothDevices_(reset) {
    this.isScanning_ = true;
    if (reset)
      $('bluetooth-unpaired-devices-list').clear();
    chrome.send('findBluetoothDevices');
  }

  //
  // Chrome callbacks
  //

  /**
   * Set the initial state of the spoken feedback checkbox.
   */
  SystemOptions.setSpokenFeedbackCheckboxState = function(checked) {
    $('accessibility-spoken-feedback-check').checked = checked;
  };

  /**
   * Set the initial state of the high contrast checkbox.
   */
  SystemOptions.setHighContrastCheckboxState = function(checked) {
    $('accessibility-high-contrast-check').checked = checked;
  };

  /**
   * Set the initial state of the screen magnifier checkbox.
   */
  SystemOptions.setScreenMagnifierCheckboxState = function(checked) {
    $('accessibility-screen-magnifier-check').checked = checked;
  };

  /**
   * Set the initial state of the virtual keyboard checkbox.
   */
  SystemOptions.setVirtualKeyboardCheckboxState = function(checked) {
    $('accessibility-virtual-keyboard-check').checked = checked;
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
  SystemOptions.setBluetoothState = function(checked) {
    $('disable-bluetooth').hidden = !checked;
    $('enable-bluetooth').hidden = checked;
    $('bluetooth-paired-devices-list').parentNode.hidden = !checked;
    $('bluetooth-add-device').hidden = !checked;
    // Flush list of previously discovered devices if bluetooth is turned off.
    if (!checked) {
      $('bluetooth-paired-devices-list').clear();
      $('bluetooth-unpaired-devices-list').clear();
    }
    if (checked && ! this.isScanning_)
      findBluetoothDevices_(true);
  }

  /**
   * Adds an element to the list of available bluetooth devices. If an element
   * with a matching address is found, the existing element is updated.
   * @param {{name: string,
   *          address: string,
   *          icon: string,
   *          paired: boolean,
   *          connected: boolean}} device
   *     Decription of the bluetooth device.
   */
  SystemOptions.addBluetoothDevice = function(device) {
    if (device.paired) {
      // Test to see if the device is currently in the unpaired list, in which
      // case it should be removed from that list.
      var index = $('bluetooth-unpaired-devices-list').find(device.address);
      if (index != undefined)
        $('bluetooth-unpaired-devices-list').deleteItemAtIndex(index);
      $('bluetooth-paired-devices-list').appendDevice(device);
    } else {
      $('bluetooth-unpaired-devices-list').appendDevice(device);
    }
    // One device can be in the process of pairing.  If found, display
    // the Bluetooth pairing overlay.
    if (device.pairing)
      BluetoothPairing.showDialog(device);
  };

  /**
   * Notification that a single pass of device discovery has completed.
   */
  SystemOptions.notifyBluetoothSearchComplete = function() {
    // TODO(kevers): Determine the fate of this method once continuous
    // scanning is implemented in the Bluetooth code.
    this.isScanning_ = false;
  };

  /**
   * Displays the touchpad controls section when we detect a touchpad, hides it
   * otherwise.
   */
  SystemOptions.showTouchpadControls = function(show) {
    $('touchpad-controls').hidden = !show;
  };

  /**
   * Displays the mouse controls section when we detect a mouse, hides it
   * otherwise.
   */
  SystemOptions.showMouseControls = function(show) {
    $('mouse-controls').hidden = !show;
  };

  // Export
  return {
    SystemOptions: SystemOptions
  };

});

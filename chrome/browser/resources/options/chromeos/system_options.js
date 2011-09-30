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

      // TODO (kevers) - Populate list of connected bluetooth devices.
      //                 Set state of 'Enable bluetooth' checkbox.
      //                 Set state of 'Find devices' button.

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
    }
  };

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
  SystemOptions.ShowBluetoothSettings = function() {
    $('bluetooth-devices').hidden = false;
  }

  // Export
  return {
    SystemOptions: SystemOptions
  };

});

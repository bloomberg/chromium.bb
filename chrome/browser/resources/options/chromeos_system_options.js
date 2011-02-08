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
    OptionsPage.call(this, 'system', templateData.systemPage, 'systemPage');
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
      var timezone = $('timezone-select');
      if (timezone) {
        // Disable the timezone setting for non-owners, as this is a
        // system wide setting.
        if (!AccountsOptions.currentUserIsOwner())
          timezone.disabled = true;
      }

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

  // Export
  return {
    SystemOptions: SystemOptions
  };

});

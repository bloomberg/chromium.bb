// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
        timezone.initializeValues(templateData.timezoneList);
        // Disable the timezone setting for non-owners, as this is a
        // system wide setting.
        if (!AccountsOptions.currentUserIsOwner()) {
          timezone.disabled = true;
          // Mark that this is manually disabled. See also pref_ui.js.
          timezone.manually_disabled = true;
        }
      }

      $('language-button').onclick = function(event) {
        OptionsPage.showPageByName('language');
      };
      $('modifier-keys-button').onclick = function(event) {
        OptionsPage.showOverlay('languageCustomizeModifierKeysOverlay');
      };
    }
  };

  // Export
  return {
    SystemOptions: SystemOptions
  };

});

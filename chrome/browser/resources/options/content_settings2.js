// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (loadTimeData.getBoolean('newContentSettings')) {

cr.define('options', function() {
  /** @const */ var OptionsPage = options.OptionsPage;

  //////////////////////////////////////////////////////////////////////////////
  // ContentSettings class:

  /**
   * Encapsulated handling of content settings page.
   * @constructor
   */
  function ContentSettings() {
    this.activeNavTab = null;
    OptionsPage.call(this, 'content',
                     loadTimeData.getString('contentSettingsPageTabTitle'),
                     'content-settings-page2');
  }

  cr.addSingletonGetter(ContentSettings);

  ContentSettings.prototype = {
    __proto__: OptionsPage.prototype,

    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      $('content-settings-overlay-confirm2').onclick =
          OptionsPage.closeOverlay.bind(OptionsPage);
    },
  };

  ContentSettings.updateHandlersEnabledRadios = function(enabled) {
    // Not implemented.
  };

  /**
   * Sets the values for all the content settings radios.
   * @param {Object} dict A mapping from radio groups to the checked value for
   *     that group.
   */
  ContentSettings.setContentFilterSettingsValue = function(dict) {
    // Not implemented.
  };

  /**
   * Initializes an exceptions list.
   * @param {string} type The content type that we are setting exceptions for.
   * @param {Array} list An array of pairs, where the first element of each pair
   *     is the filter string, and the second is the setting (allow/block).
   */
  ContentSettings.setExceptions = function(type, list) {
    // Not implemented.
  };

  ContentSettings.setHandlers = function(list) {
    // Not implemented.
  };

  ContentSettings.setIgnoredHandlers = function(list) {
    // Not implemented.
  };

  ContentSettings.setOTRExceptions = function(type, list) {
    // Not implemented.
  };

  /**
   * Enables the Pepper Flash camera and microphone settings.
   * Please note that whether the settings are actually showed or not is also
   * affected by the style class pepper-flash-settings.
   */
  ContentSettings.enablePepperFlashCameraMicSettings = function() {
    // Not implemented.
  }

  // Export
  return {
    ContentSettings: ContentSettings
  };

});

}

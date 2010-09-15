// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var OptionsPage = options.OptionsPage;

  /**
   * FontSettings class
   * Encapsulated handling of the 'Font Settings' page.
   * @class
   */
  function FontSettings() {
    OptionsPage.call(this, 'fontSettings',
                     templateData.fontSettingsTitle,
                     'fontSettings');
  }

  cr.addSingletonGetter(FontSettings);

  FontSettings.prototype = {
    // Inherit FontSettings from OptionsPage.
    __proto__: OptionsPage.prototype,

    /**
     * Initialize the page.
     */
    initializePage: function() {
      // Call base class implementation to starts preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      // Initialize values for selector controls.
      $('fontSettingsSerifSelector').initializeValues(
          templateData.fontSettingsFontList)
      $('fontSettingsSerifSizeSelector').initializeValues(
          templateData.fontSettingsFontSizeList)
      $('fontSettingsSansSerifSelector').initializeValues(
          templateData.fontSettingsFontList)
      $('fontSettingsSansSerifSizeSelector').initializeValues(
          templateData.fontSettingsFontSizeList)
      $('fontSettingsFixedWidthSelector').initializeValues(
          templateData.fontSettingsFontList)
      $('fontSettingsFixedWidthSizeSelector').initializeValues(
          templateData.fontSettingsFontSizeList)
      $('fontSettingsEncodingSelector').initializeValues(
          templateData.fontSettingsEncodingList)
    }
  };

  FontSettings.setupFontPreview = function(id, font, size) {
    $(id).textContent = font + " " + size;
    $(id).style.fontFamily = font;
    $(id).style.fontSize = size + "pt";
  }

  // Chrome callbacks
  FontSettings.setupSerifFontPreview = function(text, size) {
    this.setupFontPreview('fontSettingsSerifPreview', text, size);
  }

  FontSettings.setupSansSerifFontPreview = function(text, size) {
    this.setupFontPreview('fontSettingsSansSerifPreview', text, size);
  }

  FontSettings.setupFixedFontPreview = function(text, size) {
    this.setupFontPreview('fontSettingsFixedWidthPreview', text, size);
  }

  // Export
  return {
    FontSettings: FontSettings
  };

});


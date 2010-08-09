// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var OptionsPage = options.OptionsPage;

  /**
   * FontSettingsOverlay class
   * Encapsulated handling of the 'Font Settings' overlay page.
   * @class
   */
  function FontSettingsOverlay() {
    OptionsPage.call(this, 'fontSettingsOverlay',
                     templateData.fontSettingsOverlayTitle,
                     'fontSettingsOverlay');
  }

  cr.addSingletonGetter(FontSettingsOverlay);

  FontSettingsOverlay.prototype = {
    // Inherit FontSettingsOverlay from OptionsPage.
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

  FontSettingsOverlay.setupFontPreview = function(id, font, size) {
    $(id).textContent = font + " " + size;
    $(id).style.fontFamily = font;
    $(id).style.fontSize = size + "pt";
  }

  // Chrome callbacks
  FontSettingsOverlay.setupSerifFontPreview = function(text, size) {
    this.setupFontPreview('fontSettingsSerifPreview', text, size);
  }

  FontSettingsOverlay.setupSansSerifFontPreview = function(text, size) {
    this.setupFontPreview('fontSettingsSansSerifPreview', text, size);
  }

  FontSettingsOverlay.setupFixedFontPreview = function(text, size) {
    this.setupFontPreview('fontSettingsFixedWidthPreview', text, size);
  }

  // Export
  return {
    FontSettingsOverlay: FontSettingsOverlay
  };

});


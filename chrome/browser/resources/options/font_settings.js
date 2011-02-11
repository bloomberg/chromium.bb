// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var OptionsPage = options.OptionsPage;

  /**
   * FontSettings class
   * Encapsulated handling of the 'Fonts and Encoding' page.
   * @class
   */
  function FontSettings() {
    OptionsPage.call(this,
                     'fontSettings',
                     templateData.fontSettingsPageTabTitle,
                     'font-settings');
  }

  cr.addSingletonGetter(FontSettings);

  FontSettings.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * Initialize the page.
     */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      var serifFontRange = $('serif-font-size');
      serifFontRange.valueMap = $('fixed-font-size').valueMap = [9, 10, 11, 12,
          13, 14, 15, 16, 17, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 40, 44,
          48, 56, 64, 72];
      serifFontRange.continuous = false;
      serifFontRange.fontSampleEl = $('serif-font-sample');
      serifFontRange.notifyChange = this.rangeChanged_.bind(this);

      var minimumFontRange = $('minimum-font-size');
      minimumFontRange.valueMap = [9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20,
          22, 24];
      minimumFontRange.continuous = false;
      minimumFontRange.fontSampleEl = $('minimum-font-sample');
      minimumFontRange.notifyChange = this.rangeChanged_.bind(this);
    },

    /**
     * @inheritDoc
     */
    rangeChanged_: function(el, value) {
      this.setupFontSample_(
          el.fontSampleEl, value, el.fontSampleEl.style.fontFamily);
    },

    /**
     * Sets the text, font size and font family of the sample text.
     * @param {Element} el The div containing the sample text.
     * @param {number} size The font size of the sample text.
     * @param {string} font The font family of the sample text.
     * @private
     */
    setupFontSample_: function(el, size, font) {
      el.textContent =
          size + "pt: " + localStrings.getString('fontSettingsLoremIpsum');
      el.style.fontSize = size + "pt";
      if (font)
        el.style.fontFamily = font;
    },
  };

  // Chrome callbacks
  FontSettings.setupSerifFontSample = function(font, size) {
    FontSettings.getInstance().setupFontSample_(
        $('serif-font-sample'), size, font);
  };

  FontSettings.setupFixedFontSample = function(font, size) {
    FontSettings.getInstance().setupFontSample_(
        $('fixed-font-sample'), size, font);
  };

  FontSettings.setupMinimumFontSample = function(size) {
    FontSettings.getInstance().setupFontSample_(
        $('minimum-font-sample'), size);
  };

  // Export
  return {
    FontSettings: FontSettings
  };
});


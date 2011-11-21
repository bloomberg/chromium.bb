// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Creates a ColorSettings object. This object encapsulates all settings and
   * logic related to color selection (color/bw).
   * @constructor
   */
  function ColorSettings() {
    this.colorOption_ = $('color-option');
    this.colorRadioButton_ = $('color');
    this.bwRadioButton_ = $('bw');

    this.printerColorModelForColor_ = ColorSettings.COLOR;
    this.printerColorModelForBlack_ = ColorSettings.GRAY;
    this.addEventListeners_();
  }

  ColorSettings.GRAY = 1;
  ColorSettings.COLOR = 2;
  cr.addSingletonGetter(ColorSettings);

  ColorSettings.prototype = {
    /**
     * The radio button corresponding to the color option.
     * @type {HTMLInputElement}
     */
    get colorRadioButton() {
      return this.colorRadioButton_;
    },

    /**
     * The radio button corresponding to the black and white option.
     * @type {HTMLInputElement}
     */
    get bwRadioButton() {
      return this.bwRadioButton_;
    },

    /**
     * @return {number} The color mode for print preview.
     */
    get colorMode() {
      return this.bwRadioButton_.checked ?
             this.printerColorModelForBlack_ :
             this.printerColorModelForColor_;
    },

    /**
     * Adding listeners to all color related controls. The listeners take care
     * of altering their behavior depending on |hasPendingPreviewRequest|.
     * @private
     */
    addEventListeners_: function() {
      this.colorRadioButton_.onclick = function() {
        setColor(true);
      };
      this.bwRadioButton_.onclick = function() {
        setColor(false);
      };
      document.addEventListener(customEvents.PDF_LOADED,
                                this.onPDFLoaded_.bind(this));
      document.addEventListener(customEvents.PRINTER_CAPABILITIES_UPDATED,
                                this.onPrinterCapabilitiesUpdated_.bind(this));
    },

    /**
     * Executes when a |customEvents.PRINTER_CAPABILITIES_UPDATED| event occurs.
     * @private
     */
    onPrinterCapabilitiesUpdated_: function(e) {
      var disableColorOption = e.printerCapabilities.disableColorOption;

      disableColorOption ? fadeOutOption(this.colorOption_) :
          fadeInOption(this.colorOption_);
      this.colorOption_.setAttribute('aria-hidden', disableColorOption);

      var setColorAsDefault = e.printerCapabilities.setColorAsDefault;
      if (e.printerCapabilities.printerColorModelForColor) {
        this.printerColorModelForColor_ =
            e.printerCapabilities.printerColorModelForColor;
      } else {
        this.printerColorModelForColor_ = ColorSettings.COLOR;
      }
      if (e.printerCapabilities.printerColorModelForBlack) {
        this.printerColorModelForBlack_ =
            e.printerCapabilities.printerColorModelForBlack;
      } else {
        this.printerColorModelForBlack_ = ColorSettings.GRAY;
      }
      this.colorRadioButton_.checked = setColorAsDefault;
      this.bwRadioButton_.checked = !setColorAsDefault;
      setColor(this.colorRadioButton_.checked);
    },

    /**
     * Executes when a |customEvents.PDF_LOADED| event occurs.
     * @private
     */
    onPDFLoaded_: function() {
      setColor(this.colorRadioButton_.checked);
    },

  };

  return {
    ColorSettings: ColorSettings,
  };
});

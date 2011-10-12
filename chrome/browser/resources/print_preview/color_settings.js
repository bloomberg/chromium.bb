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
    this.GRAY = 1;
    this.COLOR = 2;
    this.CMYK = 3;  // cmyk - Cyan, magenta, yellow, black
    this.printerColorModelForColor_ = this.COLOR;
  }

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
     * Returns the color mode for print preview.
     * @return {Number} Returns the printer color space
     */
    get colorMode() {
      if (this.bwRadioButton_.checked)
        return this.GRAY;
      return this.printerColorModelForColor_;
    },

    /**
     * Adding listeners to all color related controls. The listeners take care
     * of altering their behavior depending on |hasPendingPreviewRequest|.
     */
    addEventListeners: function() {
      this.colorRadioButton_.onclick = function() {
        setColor(true);
      };
      this.bwRadioButton_.onclick = function() {
        setColor(false);
      };
      document.addEventListener('PDFLoaded', this.onPDFLoaded_.bind(this));
      document.addEventListener('printerCapabilitiesUpdated',
                                this.onPrinterCapabilitiesUpdated_.bind(this));
    },

    /**
     * Listener triggered when a printerCapabilitiesUpdated event occurs.
     * @private
     */
    onPrinterCapabilitiesUpdated_: function(e) {
      var disableColorOption = e.printerCapabilities.disableColorOption;

      disableColorOption ? fadeOutElement(this.colorOption_) :
          fadeInElement(this.colorOption_);
      this.colorOption_.setAttribute('aria-hidden', disableColorOption);

      var setColorAsDefault = e.printerCapabilities.setColorAsDefault;
      this.printerColorModelForColor_ =
          e.printerCapabilities.printerColorModelForColor;
      this.colorRadioButton_.checked = setColorAsDefault;
      this.bwRadioButton_.checked = !setColorAsDefault;
      setColor(this.colorRadioButton_.checked);
    },

    /**
     * Listener executing when a PDFLoaded event occurs.
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

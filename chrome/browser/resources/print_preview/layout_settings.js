// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Creates a LayoutSettings object. This object encapsulates all settings and
   * logic related to layout mode (portrait/landscape).
   * @constructor
   */
  function LayoutSettings() {
    this.layoutOption_ = $('layout-option');
    this.portraitRadioButton_ = $('portrait');
    this.landscapeRadioButton_ = $('landscape');
  }

  cr.addSingletonGetter(LayoutSettings);

  LayoutSettings.prototype = {
    /**
     * The radio button corresponding to the portrait option.
     * @type {HTMLInputElement}
     */
    get portraitRadioButton() {
      return this.portraitRadioButton_;
    },

    /**
     * The radio button corresponding to the landscape option.
     * @type {HTMLInputElement}
     */
    get landscapeRadioButton() {
      return this.landscapeRadioButton_;
    },

    /**
     * @return {boolean} true if |this.landscapeRadioButton_| is checked, false
     *     if not.
     */
    isLandscape: function() {
      return this.landscapeRadioButton_.checked;
    },

    /**
     * Adding listeners to all layout related controls. The listeners take care
     * of altering their behavior depending on |hasPendingPreviewRequest|.
     */
    addEventListeners: function() {
      this.landscapeRadioButton_.onclick = function() {
        if (!hasPendingPreviewRequest)
          this.onLayoutButtonClick_();
      }.bind(this);
      this.portraitRadioButton_.onclick = function() {
        if (!hasPendingPreviewRequest)
          this.onLayoutButtonClick_();
      }.bind(this);
      document.addEventListener('PDFLoaded', this.onPDFLoaded_.bind(this));
      document.addEventListener('printerCapabilitiesUpdated',
                                this.onPrinterCapabilitiesUpdated_.bind(this));
    },

    /**
     * Listener triggered when a printerCapabilitiesUpdated event occurs.
     * @private
     */
    onPrinterCapabilitiesUpdated_: function(e) {
      this.fadeInOut(e.printerCapabilities.disableLandscapeOption);
    },

    /**
     * Listener executing when |this.landscapeRadioButton_| or
     * |this.portraitRadioButton_| is clicked.
     * @private
     */
    onLayoutButtonClick_: function() {
      // If the chosen layout is same as before, nothing needs to be done.
      if (printSettings.isLandscape == this.isLandscape())
        return;
      setDefaultValuesAndRegeneratePreview();
    },

    /**
     * Listener executing when a PDFLoaded event occurs.
     * @private
     */
    onPDFLoaded_: function() {
      if (!previewModifiable)
        fadeOutElement(this.layoutOption_);
    },

    /**
     * @param {boolean} fadeOut True if |this.layoutOption_| should be faded
     *     out, false if it should be faded in.
     */
    fadeInOut: function(fadeOut) {
      fadeOut ? fadeOutElement(this.layoutOption_) :
          fadeInElement(this.layoutOption_);
    }
  };

  return {
    LayoutSettings: LayoutSettings,
  };
});

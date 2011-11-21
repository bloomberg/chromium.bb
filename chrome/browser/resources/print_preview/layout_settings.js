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
    this.wasLandscape_ = false;
    this.updateState();
    this.addEventListeners_();
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
     * @return {boolean} true if the chosen layout mode has changed since last
     *     time the state was updated.
     */
    hasChanged_: function() {
      return this.isLandscape() != this.wasLandscape_;
    },

    /**
     * Saves the currently selected layout mode. Used  in |this.hasChanged_|.
     */
    updateState: function() {
      this.wasLandscape_ = this.isLandscape();
    },

    /**
     * Adding listeners to all layout related controls. The listeners take care
     * of altering their behavior depending on |hasPendingPreviewRequest|.
     * @private
     */
    addEventListeners_: function() {
      this.landscapeRadioButton_.onclick = this.onLayoutButtonClick_.bind(this);
      this.portraitRadioButton_.onclick = this.onLayoutButtonClick_.bind(this);
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
      if (e.printerCapabilities.disableLandscapeOption)
        this.fadeInOut_(e.printerCapabilities.disableLandscapeOption);
    },

    /**
     * Listener executing when |this.landscapeRadioButton_| or
     * |this.portraitRadioButton_| is clicked.
     * @private
     */
    onLayoutButtonClick_: function() {
      // If the chosen layout is same as before, nothing needs to be done.
      if (this.hasChanged_())
        setDefaultValuesAndRegeneratePreview(true);
    },

    /**
     * Listener executing when a |customEvents.PDF_LOADED| event occurs.
     * @private
     */
    onPDFLoaded_: function() {
      this.fadeInOut_(!previewModifiable);
    },

    /**
     * @param {boolean} fadeOut True if |this.layoutOption_| should be faded
     *     out, false if it should be faded in.
     * @private
     */
    fadeInOut_: function(fadeOut) {
      fadeOut ? fadeOutOption(this.layoutOption_) :
          fadeInOption(this.layoutOption_);
    }
  };

  return {
    LayoutSettings: LayoutSettings,
  };
});

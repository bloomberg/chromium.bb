// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Creates a MarginSettings object. This object encapsulates all settings and
   * logic related to the margins mode.
   * @constructor
   */
  function MarginSettings() {
    this.marginsOption_ = $('margins-option');
    this.marginList_ = $('margin-list');
    // Holds the custom left margin value (if set).
    this.customMarginLeft_ = -1;
    // Holds the custom right margin value (if set).
    this.customMarginRight_ = -1;
    // Holds the custom top margin value (if set).
    this.customMarginTop_ = -1;
    // Holds the custom bottom margin value (if set).
    this.customMarginBottom_ = -1;
    // Margin list values.
    this.customMarginsValue_ = 2;
    this.defaultMarginsValue_ = 0;
    this.noMarginsValue_ = 1;
    // Default Margins option index.
    this.defaultMarginsIndex_ = 0;
  }

  cr.addSingletonGetter(MarginSettings);

  MarginSettings.prototype = {
    /**
     * The selection list corresponding to the margins option.
     * @return {HTMLInputElement}
     */
    get marginList() {
      return this.marginList_;
    },

    /**
     * Returns a dictionary of the four custom margin values.
     * @return {object}
     */
    get customMargins() {
      return {'marginLeft': this.customMarginLeft_,
              'marginTop': this.customMarginTop_,
              'marginRight': this.customMarginRight_,
              'marginBottom': this.customMarginBottom_};
    },

    /**
     * Gets the value of the selected margin option.
     * @private
     * @return {number}
     */
    get selectedMarginsValue_() {
      return this.marginList_.options[this.marginList_.selectedIndex].value;
    },

    /**
     * Checks whether user has selected the Default Margins option or not.
     *
     * @return {boolean} true if default margins are selected.
     */
    isDefaultMarginsSelected: function() {
      return this.selectedMarginsValue_ == this.defaultMarginsValue_;
    },

    /**
     * Adds listeners to all margin related controls. The listeners take care
     * of altering their behavior depending on |hasPendingPreviewRequest|.
     */
    addEventListeners: function() {
      this.marginList_.onchange = this.onMarginsChanged_.bind(this);
      document.addEventListener('PDFLoaded', this.onPDFLoaded_.bind(this));
    },

    /**
     * Listener executing when user selects a different margin option, ie,
     * |this.marginList_| is changed.
     * @private
     */
    onMarginsChanged_: function() {
      if (this.selectedMarginsValue_ == this.defaultMarginsValue_) {
        setDefaultValuesAndRegeneratePreview(false);
      } else if (this.selectedMarginsValue_ == this.noMarginsValue_) {
        this.customMarginLeft_ = 0;
        this.customMarginTop_ = 0;
        this.customMarginRight_ = 0;
        this.customMarginBottom_ = 0;
        setDefaultValuesAndRegeneratePreview(false);
      }
      // TODO(aayushkumar): Add handler for custom margins
    },

    /**
     * If custom margins is the currently selected option then change to the
     * default margins option.
     */
    resetMarginsIfNeeded: function() {
      if (this.selectedMarginsValue_ == this.customMarginsValue_)
        this.marginList_.options[this.defaultMarginsIndex_].selected = true;
    },

    /**
     * Listener executing when a PDFLoaded event occurs.
     * @private
     */
    onPDFLoaded_: function() {
      if (!previewModifiable)
        fadeOutElement(this.marginsOption_);
    }
  };

  return {
    MarginSettings: MarginSettings,
  };
});

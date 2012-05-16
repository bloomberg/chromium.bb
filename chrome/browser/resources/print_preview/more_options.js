// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Creates a MoreOptions object. This object encapsulates all
   * settings and logic related to the more options section.
   * @constructor
   */
  function MoreOptions() {
    // @type {HTMLDivElement} HTML element representing more options.
    this.moreOptions_ = $('more-options');

    // @type {boolean} True if header footer option is hidden.
    this.hideHeaderFooterOption_ = true;

    // @type {boolean} True if fit to page option should be hidden.
    this.hideFitToPageOption_ = true;

    this.addEventListeners_();
  }

  cr.addSingletonGetter(MoreOptions);

  MoreOptions.prototype = {
    /**
     * Adding listeners to more options section.
     * @private
     */
    addEventListeners_: function() {
      document.addEventListener(customEvents.PDF_LOADED,
                                this.onPDFLoaded_.bind(this));
      document.addEventListener(
          customEvents.HEADER_FOOTER_VISIBILITY_CHANGED,
          this.onHeaderFooterVisibilityChanged_.bind(this));
      document.addEventListener(customEvents.PRINTER_SELECTION_CHANGED,
                                this.onPrinterSelectionChanged_.bind(this));
    },

    /**
     * Listener executing when a |customEvents.HEADER_FOOTER_VISIBILITY_CHANGED|
     * event occurs.
     * @param {cr.Event} event The event that triggered this listener.
     * @private
     */
    onHeaderFooterVisibilityChanged_: function(event) {
      this.hideHeaderFooterOption_ = !event.headerFooterApplies;
      this.updateVisibility_();
    },

    /**
     * Listener executing when a |customEvents.PRINTER_SEELCTION_CHANGED| event
     * occurs.
     * @param {cr.Event} event The event that triggered this listener.
     * @private
     */
    onPrinterSelectionChanged_: function(event) {
      if (previewModifiable)
        return;
      this.hideFitToPageOption_ = event.selectedPrinter == PRINT_TO_PDF;
      this.updateVisibility_();
    },

    /**
     * Listener executing when a |customEvents.PDF_LOADED| event occurs.
     * @private
     */
    onPDFLoaded_: function() {
      if (previewModifiable)
        this.hideHeaderFooterOption_ = false;
      else
        this.hideFitToPageOption_ = false;
    },

    /**
     * Hides or shows |this.moreOptions_|.
     * @private
     */
    updateVisibility_: function() {
      if (this.hideFitToPageOption_ && this.hideHeaderFooterOption_)
        fadeOutOption(this.moreOptions_);
      else
        fadeInOption(this.moreOptions_);
    }
  };

  return {
    MoreOptions: MoreOptions
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
cr.define('print_preview', function() {
  'use strict';

  /**
   * Creates a |FitToPageSettings| object. This object encapsulates all
   * settings and logic related to the fit to page checkbox.
   * @constructor
   */
  function FitToPageSettings() {
    // @type {HTMLDivElement} This represents fit to page div element.
    this.fitToPageOption_ = $('fit-to-page-option');

    // @type {HTMLInputElement} This represents fit to page input element.
    this.fitToPageCheckbox_ = $('fit-to-page');

    // @type {boolean} True if fit to page option applies for the selected
    //      user options. Fit to Page options applies only if we are previewing
    //      a PDF and the current destination printer is actually a physcial
    //      printer.
    this.fitToPageApplies_ = true;

    this.addEventListeners_();
  }

  cr.addSingletonGetter(FitToPageSettings);

  FitToPageSettings.prototype = {
    /**
     * Returns true if we need to fit the page contents to printable area.
     * @return {boolean} true if Fit to page is checked.
     */
    hasFitToPage: function() {
      return previewModifiable || this.fitToPageCheckbox_.checked;
    },

    /**
     * Updates |this.fitToPageApplies_| depending on the selected printer and
     * preview data source type.
     * @param {!string} printerName Selected printer name.
     * @private
     */
    resetState_: function(printerName) {
      if (!previewModifiable)
        isPrintReadyMetafileReady = false;
      var printToPDF = printerName == PRINT_TO_PDF;
      this.fitToPageApplies_ = !previewModifiable && !printToPDF;
    },

    /**
     * Print scaling is disabled for preview source plugin. Uncheck the fit to
     * page option.
     */
    onPrintScalingDisabled: function() {
      this.fitToPageCheckbox_.checked = false;
    },

    /**
     * Adding listeners to fit to page control.
     * @private
     */
    addEventListeners_: function() {
      this.fitToPageCheckbox_.onclick =
          this.onFitToPageCheckboxClicked_.bind(this);
      document.addEventListener(customEvents.PDF_LOADED,
                                this.onPDFLoaded_.bind(this));
      document.addEventListener(customEvents.PRINTER_SELECTION_CHANGED,
                                this.onPrinterSelectionChanged_.bind(this));
    },

    /**
     * Listener executing when a |customEvents.PRINTER_SELECTION_CHANGED| event
     * occurs.
     * @param {cr.Event} event The event that triggered this listener.
     * @private
     */
    onPrinterSelectionChanged_: function(event) {
      this.resetState_(event.selectedPrinter);
      this.updateVisibility_();
    },

    /**
     * Listener executing when the user selects or de-selects the fit to page
     * option.
     * @private
     */
    onFitToPageCheckboxClicked_: function() {
      requestPrintPreview();
    },

    /**
     * Listener executing when a |customEvents.PDF_LOADED| event occurs.
     * @private
     */
    onPDFLoaded_: function() {
      this.updateVisibility_();
    },

    /**
     * Hides or shows |this.fitToPageOption_|.
     * @private
     */
    updateVisibility_: function() {
      this.fitToPageOption_.style.display =
          this.fitToPageApplies_ ? 'block' : 'none';
    }
  };

  return {
    FitToPageSettings: FitToPageSettings
  };
});

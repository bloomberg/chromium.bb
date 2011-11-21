// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Creates a PrintHeader object. This object encapsulates all the elements
   * and logic related to the top part of the left pane in print_preview.html.
   * @constructor
   */
  function PrintHeader() {
    this.printButton_ = $('print-button');
    this.cancelButton_ = $('cancel-button');
    this.summary_ = $('print-summary');
    this.printButton_.focus();
    this.addEventListeners_();
  }

  cr.addSingletonGetter(PrintHeader);

  PrintHeader.prototype = {
    get printButton() {
      return this.printButton_;
    },

    get cancelButton() {
      return this.cancelButton_;
    },

    get summary() {
      return this.summary_;
    },

    /**
     * Adding event listeners where necessary. Listeners take care of changing
     * their behavior depending on the current state, no need to remove them.
     * @private
     */
    addEventListeners_: function() {
      this.cancelButton_.onclick = function() {
        this.disableCancelButton();
        closePrintPreviewTab();
      }.bind(this);
      this.printButton_.onclick = this.onPrintButtonClicked_.bind(this);
      document.addEventListener(customEvents.UPDATE_SUMMARY,
                                this.updateSummary_.bind(this));
      document.addEventListener(customEvents.UPDATE_PRINT_BUTTON,
                                this.updatePrintButton_.bind(this));
      document.addEventListener(customEvents.PDF_GENERATION_ERROR,
                                this.onPDFGenerationError_.bind(this));
    },

    /**
     * Enables the cancel button and attaches its keydown event listener.
     */
    enableCancelButton: function() {
      window.onkeydown = onKeyDown;
      this.cancelButton_.disabled = false;
    },

    /**
     * Executes when a |customEvents.PDF_GENERATION_ERROR| event occurs.
     * @private
     */
    onPDFGenerationError_: function() {
      this.printButton_.disabled = true;
    },

    /**
     * Disables the cancel button and removes its keydown event listener.
     */
    disableCancelButton: function() {
      window.onkeydown = null;
      this.cancelButton_.disabled = true;
    },

    /**
     * Listener executing whenever |this.printButton_| is clicked.
     * @private
     */
    onPrintButtonClicked_: function() {
      var printToPDF = getSelectedPrinterName() == PRINT_TO_PDF;
      if (!printToPDF) {
        this.printButton_.classList.add('loading');
        this.cancelButton_.classList.add('loading');
        this.summary_.innerHTML = localStrings.getString('printing');
      }
      this.disableCancelButton();
      requestToPrintDocument();
    },

    /**
     * Updates the state of |this.printButton_| depending on the user selection.
     * The button is enabled only when the following conditions are true.
     * 1) The selected page ranges are valid.
     * 2) The number of copies is valid (if applicable).
     * @private
     */
    updatePrintButton_: function() {
      if (showingSystemDialog)
        return;
      this.printButton_.disabled = !areSettingsValid();
    },

    /**
     * Updates |this.summary_| based on the currently selected user options.
     * @private
     */
    updateSummary_: function() {
      var printToPDF = getSelectedPrinterName() == PRINT_TO_PDF;
      var copies = printToPDF ? 1 : copiesSettings.numberOfCopies;

      if ((!printToPDF && !copiesSettings.isValid()) ||
          !pageSettings.isPageSelectionValid()) {
        this.summary_.innerHTML = '';
        return;
      }

      if (!marginSettings.areMarginSettingsValid()) {
        this.summary_.innerHTML = '';
        return;
      }

      var pageSet = pageSettings.selectedPagesSet;
      var numOfSheets = pageSet.length;
      if (numOfSheets == 0)
        return;

      var summaryLabel =
          localStrings.getString('printPreviewSheetsLabelSingular');
      var numOfPagesText = '';
      var pagesLabel = localStrings.getString('printPreviewPageLabelPlural');

      if (printToPDF)
        summaryLabel = localStrings.getString('printPreviewPageLabelSingular');

      if (!printToPDF && copiesSettings.twoSidedCheckbox.checked)
        numOfSheets = Math.ceil(numOfSheets / 2);
      numOfSheets *= copies;

      if (numOfSheets > 1) {
        summaryLabel = printToPDF ? pagesLabel :
            localStrings.getString('printPreviewSheetsLabelPlural');
      }

      var html = '';
      if (pageSet.length * copies != numOfSheets) {
        numOfPagesText = pageSet.length * copies;
        html = localStrings.getStringF('printPreviewSummaryFormatLong',
                                       '<b>' + numOfSheets + '</b>',
                                       '<b>' + summaryLabel + '</b>',
                                       numOfPagesText, pagesLabel);
      } else {
        html = localStrings.getStringF('printPreviewSummaryFormatShort',
                                       '<b>' + numOfSheets + '</b>',
                                       '<b>' + summaryLabel + '</b>');
      }

      // Removing extra spaces from within the string.
      html = html.replace(/\s{2,}/g, ' ');
      this.summary_.innerHTML = html;
    },
  };

  return {
    PrintHeader: PrintHeader,
  };
});

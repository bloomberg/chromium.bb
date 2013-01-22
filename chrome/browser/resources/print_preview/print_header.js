// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Creates a PrintHeader object. This object encapsulates all the elements
   * and logic related to the top part of the left pane in print_preview.html.
   * @param {!print_preview.PrintTicketStore} printTicketStore Used to read
   *     information about the document.
   * @param {!print_preview.DestinationStore} destinationStore Used to get the
   *     selected destination.
   * @constructor
   * @extends {print_preview.Component}
   */
  function PrintHeader(printTicketStore, destinationStore) {
    print_preview.Component.call(this);

    /**
     * Used to read information about the document.
     * @type {!print_preview.PrintTicketStore}
     * @private
     */
    this.printTicketStore_ = printTicketStore;

    /**
     * Used to get the selected destination.
     * @type {!print_preview.DestinationStore}
     * @private
     */
    this.destinationStore_ = destinationStore;

    /**
     * Whether the component is enabled.
     * @type {boolean}
     * @private
     */
    this.isEnabled_ = true;

    /**
     * Whether the print button is enabled.
     * @type {boolean}
     * @private
     */
    this.isPrintButtonEnabled_ = true;
  };

  /**
   * Event types dispatched by the print header.
   * @enum {string}
   */
  PrintHeader.EventType = {
    PRINT_BUTTON_CLICK: 'print_preview.PrintHeader.PRINT_BUTTON_CLICK',
    CANCEL_BUTTON_CLICK: 'print_preview.PrintHeader.CANCEL_BUTTON_CLICK'
  },

  /**
   * CSS classes used by the print header.
   * @enum {string}
   * @private
   */
  PrintHeader.Classes_ = {
    CANCEL_BUTTON: 'print-header-cancel-button',
    PRINT_BUTTON: 'print-header-print-button',
    SUMMARY: 'print-header-summary'
  };

  PrintHeader.prototype = {
    __proto__: print_preview.Component.prototype,

    set isEnabled(isEnabled) {
      this.isEnabled_ = isEnabled;
      this.updatePrintButtonEnabledState_();
      this.cancelButton_.disabled = !isEnabled;
    },

    set isPrintButtonEnabled(isEnabled) {
      this.isPrintButtonEnabled_ = isEnabled;
      this.updatePrintButtonEnabledState_();
    },

    /** @param {string} message Error message to display in the print header. */
    setErrorMessage: function(message) {
      var summaryEl = this.getElement().getElementsByClassName(
          PrintHeader.Classes_.SUMMARY)[0];
      summaryEl.innerHTML = '';
      summaryEl.textContent = message;
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);

      // User events
      this.tracker.add(
          this.cancelButton_, 'click', this.onCancelButtonClick_.bind(this));
      this.tracker.add(
          this.printButton_, 'click', this.onPrintButtonClick_.bind(this));

      // Data events.
      this.tracker.add(
          this.printTicketStore_,
          print_preview.PrintTicketStore.EventType.INITIALIZE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_,
          print_preview.PrintTicketStore.EventType.DOCUMENT_CHANGE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_,
          print_preview.PrintTicketStore.EventType.TICKET_CHANGE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.DESTINATION_SELECT,
          this.onDestinationSelect_.bind(this));
    },

    /**
     * @return {Element} Print button element.
     * @private
     */
    get printButton_() {
      return this.getElement().getElementsByClassName(
          PrintHeader.Classes_.PRINT_BUTTON)[0];
    },

    /**
     * @return {Element} Cancel button element.
     * @private
     */
    get cancelButton_() {
      return this.getElement().getElementsByClassName(
          PrintHeader.Classes_.CANCEL_BUTTON)[0];
    },

    /**
     * Updates Print Button state.
     * @private
     */
    updatePrintButtonEnabledState_: function() {
      this.printButton_.disabled = !this.isEnabled_ ||
                                   !this.isPrintButtonEnabled_ ||
                                   !this.printTicketStore_.isTicketValid();
    },

    /**
     * Updates the summary element based on the currently selected user options.
     * @private
     */
    updateSummary_: function() {
      var summaryEl = this.getElement().getElementsByClassName(
          PrintHeader.Classes_.SUMMARY)[0];
      if (!this.printTicketStore_.isTicketValid()) {
        summaryEl.innerHTML = '';
        return;
      }

      var summaryLabel =
          localStrings.getString('printPreviewSheetsLabelSingular');
      var pagesLabel = localStrings.getString('printPreviewPageLabelPlural');

      var saveToPdf = this.destinationStore_.selectedDestination &&
          this.destinationStore_.selectedDestination.id ==
              print_preview.Destination.GooglePromotedId.SAVE_AS_PDF;
      if (saveToPdf) {
        summaryLabel = localStrings.getString('printPreviewPageLabelSingular');
      }

      var numPages = this.printTicketStore_.getPageNumberSet().size;
      var numSheets = numPages;
      if (!saveToPdf && this.printTicketStore_.isDuplexEnabled()) {
        numSheets = Math.ceil(numPages / 2);
      }

      var copies = this.printTicketStore_.getCopies();
      numSheets *= copies;
      numPages *= copies;

      if (numSheets > 1) {
        summaryLabel = saveToPdf ? pagesLabel :
            localStrings.getString('printPreviewSheetsLabelPlural');
      }

      var html;
      if (numPages != numSheets) {
        html = localStrings.getStringF('printPreviewSummaryFormatLong',
                                       '<b>' + numSheets + '</b>',
                                       '<b>' + summaryLabel + '</b>',
                                       numPages,
                                       pagesLabel);
      } else {
        html = localStrings.getStringF('printPreviewSummaryFormatShort',
                                       '<b>' + numSheets + '</b>',
                                       '<b>' + summaryLabel + '</b>');
      }

      // Removing extra spaces from within the string.
      html = html.replace(/\s{2,}/g, ' ');
      summaryEl.innerHTML = html;
    },

    /**
     * Called when the print button is clicked. Dispatches a PRINT_DOCUMENT
     * common event.
     * @private
     */
    onPrintButtonClick_: function() {
      if (this.destinationStore_.selectedDestination.id !=
          print_preview.Destination.GooglePromotedId.SAVE_AS_PDF) {
        this.printButton_.classList.add('loading');
        this.cancelButton_.classList.add('loading');
        var summaryEl = this.getElement().getElementsByClassName(
            PrintHeader.Classes_.SUMMARY)[0];
        summaryEl.innerHTML = localStrings.getString('printing');
      }
      cr.dispatchSimpleEvent(this, PrintHeader.EventType.PRINT_BUTTON_CLICK);
    },

    /**
     * Called when the cancel button is clicked. Dispatches a
     * CLOSE_PRINT_PREVIEW event.
     * @private
     */
    onCancelButtonClick_: function() {
      cr.dispatchSimpleEvent(this, PrintHeader.EventType.CANCEL_BUTTON_CLICK);
    },

    /**
     * Called when a new destination is selected. Updates the text on the print
     * button.
     * @private
     */
    onDestinationSelect_: function() {
      if (this.destinationStore_.selectedDestination.id ==
              print_preview.Destination.GooglePromotedId.SAVE_AS_PDF ||
          this.destinationStore_.selectedDestination.id ==
              print_preview.Destination.GooglePromotedId.DOCS) {
        this.printButton_.textContent = localStrings.getString('saveButton');
      } else {
        this.printButton_.textContent = localStrings.getString('printButton');
      }
      this.printButton_.focus();
    },

    /**
     * Called when the print ticket has changed. Disables the print button if
     * any of the settings are invalid.
     * @private
     */
    onTicketChange_: function() {
      this.updatePrintButtonEnabledState_();
      this.updateSummary_();
      if (document.activeElement == null ||
          document.activeElement == document.body) {
        this.printButton_.focus();
      }
    }
  };

  // Export
  return {
    PrintHeader: PrintHeader
  };
});

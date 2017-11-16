// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Enumeration of Google-promoted destination IDs.
 * @enum {string}
 */
const GooglePromotedId = {
  DOCS: '__google__docs',
  SAVE_AS_PDF: 'Save as PDF'
};

Polymer({
  is: 'print-preview-header',

  properties: {
    /** @type {!print_preview_new.Model} */
    model: Object,

    /** @private {boolean} */
    printInProgress_: {
      type: Boolean,
      notify: true,
      value: false,
    },

    /**
     * @private {?string} Null value indicates that there is no error or
     *     state to display in the summary.
     */
    currentErrorOrState_: {
      type: String,
      computed: 'computeErrorOrStateString_(model.previewLoading, ' +
          'model.previewFailed, model.cloudPrintError, ' +
          'model.privetExtensionError, model.invalidSettings, ' +
          'model.copiesInvalid, model.scalingInvalid, model.pagesInvalid, ' +
          'printInProgress_)'
    },

    /**
     * @private {{numPages: number,
     *            numSheets: number,
     *            pagesLabel: string,
     *            summaryLabel: string}}
     */
    labelInfo_: {
      type: Object,
      computed: 'getLabelInfo_(currentErrorOrState_, model.destinationId, ' +
          'model.copies, model.pageRange, model.duplex)'
    },
  },

  /** @private */
  onPrintButtonTap_: function() {
    this.printInProgress_ = true;
  },

  /** @private */
  onCancelButtonTap_: function() {
    this.printInProgress_ = false;
  },

  /**
   * @return {boolean}
   * @private
   */
  isPdfOrDrive_: function() {
    return this.model.destinationId == GooglePromotedId.SAVE_AS_PDF ||
        this.model.destinationId == GooglePromotedId.DOCS;
  },

  /**
   * @return {string}
   * @private
   */
  getPrintButton_: function() {
    return loadTimeData.getString(
        this.isPdfOrDrive_() ? 'saveButton' : 'printButton');
  },

  /**
   * @return {?string}
   * @private
   */
  computeErrorOrStateString_: function() {
    if (this.model.cloudPrintError != '')
      return this.model.cloudPrintError;
    if (this.model.privetExtensionError != '')
      return this.model.privetExtensionError;
    if (this.model.invalidSettings || this.model.previewFailed ||
        this.model.previewLoading || this.model.copiesInvalid ||
        this.model.scalingInvalid || this.model.pagesInvalid) {
      return '';
    }
    if (this.printInProgress_) {
      return loadTimeData.getString(
          this.isPdfOrDrive_() ? 'saving' : 'printing');
    }
    return null;
  },

  /**
   * @return {{numPages: number,
   *           numSheets: number,
   *           pagesLabel: string,
   *           summaryLabel: string}}
   * @private
   */
  getLabelInfo_: function() {
    const saveToPdfOrDrive = this.isPdfOrDrive_();
    let numPages = this.model.pageRange.length;
    let numSheets = numPages;
    if (!saveToPdfOrDrive && this.model.duplex) {
      numSheets = Math.ceil(numPages / 2);
    }

    const copies = this.model.copies;
    numSheets *= copies;
    numPages *= copies;

    const pagesLabel = loadTimeData.getString('printPreviewPageLabelPlural');
    let summaryLabel;
    if (numSheets > 1) {
      summaryLabel = saveToPdfOrDrive ?
          pagesLabel :
          loadTimeData.getString('printPreviewSheetsLabelPlural');
    } else {
      summaryLabel = loadTimeData.getString(
          saveToPdfOrDrive ? 'printPreviewPageLabelSingular' :
                             'printPreviewSheetsLabelSingular');
    }
    return {
      numPages: numPages,
      numSheets: numSheets,
      pagesLabel: pagesLabel,
      summaryLabel: summaryLabel
    };
  },

  /**
   * @return {boolean}
   * @private
   */
  printButtonDisabled_: function() {
    return this.currentErrorOrState_ != null;
  },

  /**
   * @return {string}
   * @private
   */
  getSummary_: function() {
    let html = this.currentErrorOrState_;
    if (html != null)
      return html;
    const labelInfo = this.labelInfo_;
    if (labelInfo.numPages != labelInfo.numSheets) {
      html = loadTimeData.getStringF(
          'printPreviewSummaryFormatLong',
          '<b>' + labelInfo.numSheets.toLocaleString() + '</b>',
          '<b>' + labelInfo.summaryLabel + '</b>',
          labelInfo.numPages.toLocaleString(), labelInfo.pagesLabel);
    } else {
      html = loadTimeData.getStringF(
          'printPreviewSummaryFormatShort',
          '<b>' + labelInfo.numSheets.toLocaleString() + '</b>',
          '<b>' + labelInfo.summaryLabel + '</b>');
    }

    // Removing extra spaces from within the string.
    html = html.replace(/\s{2,}/g, ' ');
    return html;
  },

  /**
   * @return {string}
   * @private
   */
  getSummaryLabel_: function() {
    if (this.currentErrorOrState_ != null)
      return this.currentErrorOrState_;
    const labelInfo = this.labelInfo_;
    if (labelInfo.numPages != labelInfo.numSheets) {
      return loadTimeData.getStringF(
          'printPreviewSummaryFormatLong', labelInfo.numSheets.toLocaleString(),
          labelInfo.summaryLabel, labelInfo.numPages.toLocaleString(),
          labelInfo.pagesLabel);
    }
    return loadTimeData.getStringF(
        'printPreviewSummaryFormatShort', labelInfo.numSheets.toLocaleString(),
        labelInfo.summaryLabel);
  }
});

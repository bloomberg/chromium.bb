// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview_new');

/** @enum {number} */
const PagesInputErrorState = {
  NO_ERROR: 0,
  INVALID_SYNTAX: 1,
  OUT_OF_BOUNDS: 2,
};

Polymer({
  is: 'print-preview-pages-settings',

  behaviors: [SettingsBehavior],

  properties: {
    /** @type {!print_preview.DocumentInfo} */
    documentInfo: Object,

    /** @private {string} */
    inputString_: {
      type: String,
      value: '',
    },

    /** @private {!Array<number>} */
    allPagesArray_: {
      type: Array,
      computed: 'computeAllPagesArray_(documentInfo.pageCount)',
    },

    /** @private {boolean} */
    allSelected_: {
      type: Boolean,
      value: true,
    },

    /** @private {boolean} */
    customSelected_: {
      type: Boolean,
      value: false,
    },

    /** @private {!Array<number>} */
    pagesToPrint_: {
      type: Array,
      computed: 'computePagesToPrint_(' +
          'inputString_, allSelected_, allPagesArray_)',
    },

    /** @private {!Array<{to: number, from: number}>} */
    rangesToPrint_: {
      type: Array,
      computed: 'computeRangesToPrint_(pagesToPrint_, allPagesArray_)',
    },

    /** @private {!PagesInputErrorState} */
    errorState_: {
      type: Number,
      computed: 'computeErrorState_(documentInfo.pageCount, pagesToPrint_)',
    },

  },

  observers: [
    'onRangeChange_(errorState_, rangesToPrint_)',
    'onRadioChange_(allSelected_, customSelected_)'
  ],

  /**
   * @return {!Array<number>}
   * @private
   */
  computeAllPagesArray_: function() {
    const array = new Array(this.documentInfo.pageCount);
    for (let i = 0; i < array.length; i++)
      array[i] = i + 1;
    return array;
  },

  /**
   * Updates pages to print and error state based on the validity and
   * current value of the input.
   * @return {!Array<number>}
   * @private
   */
  computePagesToPrint_: function() {
    if (this.allSelected_ || this.inputString_.trim() == '')
      return this.allPagesArray_;
    if (!this.$$('.user-value').validity.valid)
      return [];

    const pages = [];
    const added = {};
    const ranges = this.inputString_.split(',');
    const maxPage = this.allPagesArray_.length;
    for (let range of ranges) {
      range = range.trim();
      if (range == '')
        continue;
      const limits = range.split('-');
      let min = parseInt(limits[0], 10);
      if (min < 1)
        return [];
      if (limits.length == 1) {
        if (min > maxPage)
          return [-1];
        if (!added.hasOwnProperty(min)) {
          pages.push(min);
          added[min] = true;
        }
        continue;
      }

      let max = parseInt(limits[1], 10);
      if (isNaN(min))
        min = 1;
      if (isNaN(max))
        max = maxPage;
      if (min > max)
        return [];
      if (max > maxPage)
        return [-1];
      for (let i = min; i <= max; i++) {
        if (!added.hasOwnProperty(i)) {
          pages.push(i);
          added[i] = true;
        }
      }
    }
    return pages;
  },

  /**
   * Updates ranges to print.
   * @return {!Array<{to: number, from: number}>}
   * @private
   */
  computeRangesToPrint_: function() {
    let lastPage = 0;
    if (this.pagesToPrint_.length == 0 || this.pagesToPrint_[0] == -1 ||
        this.pagesToPrint_ == this.allPagesArray_)
      return [];

    let from = this.pagesToPrint_[0];
    let to = this.pagesToPrint_[0];
    let ranges = [];
    for (let page of this.pagesToPrint_.slice(1)) {
      if (page == to + 1) {
        to = page;
        continue;
      }
      ranges.push({from: from, to: to});
      from = page;
      to = page;
    }
    ranges.push({from: from, to: to});
    return ranges;
  },

  /**
   * @return {!PagesInputErrorState}
   * @private
   */
  computeErrorState_: function() {
    if (this.documentInfo.pageCount == 0)  // page count not yet initialized
      return PagesInputErrorState.NO_ERROR;
    if (this.pagesToPrint_.length == 0)
      return PagesInputErrorState.INVALID_SYNTAX;
    if (this.pagesToPrint_[0] == -1)
      return PagesInputErrorState.OUT_OF_BOUNDS;
    return PagesInputErrorState.NO_ERROR;
  },

  /**
   * Updates the model with pages and validity, and adds error styling if
   * needed.
   * @private
   */
  onRangeChange_: function() {
    if (this.errorState_ != PagesInputErrorState.NO_ERROR) {
      this.setSettingValid('pages', false);
      this.$$('.user-value').classList.add('invalid');
      return;
    }
    this.$$('.user-value').classList.remove('invalid');
    this.setSettingValid('pages', true);
    this.setSetting('pages', this.pagesToPrint_);
    this.setSetting('ranges', this.rangesToPrint_);
  },

  /** @private */
  onRadioChange_: function() {
    if (this.$$('#all-radio-button').checked)
      this.customSelected_ = false;
    if (this.$$('#custom-radio-button').checked)
      this.allSelected_ = false;
  },

  /** @private */
  onCustomRadioClick_: function() {
    this.$$('#page-settings-custom-input').focus();
  },

  /** @private */
  onCustomInputFocus_: function() {
    this.$$('#all-radio-button').checked = false;
    this.$$('#custom-radio-button').checked = true;
    this.customSelected_ = true;
  },

  /**
   * @param {Event} event Contains information about where focus is going.
   * @private
   */
  onCustomInputBlur_: function(event) {
    if (this.inputString_.trim() == '' &&
        event.relatedTarget != this.$$('.custom-input-wrapper') &&
        event.relatedTarget != this.$$('#custom-radio-button')) {
      this.$$('#all-radio-button').checked = true;
      this.$$('#custom-radio-button').checked = false;
      this.allSelected_ = true;
    }
  },

  /**
   * @return {string} Gets message to show as hint.
   * @private
   */
  getHintMessage_: function() {
    if (this.errorState_ == PagesInputErrorState.INVALID_SYNTAX) {
      return loadTimeData.getStringF(
          'pageRangeSyntaxInstruction',
          loadTimeData.getString('examplePageRangeText'));
    } else {
      return loadTimeData.getStringF(
          'pageRangeLimitInstructionWithValue', this.documentInfo.pageCount);
    }
  },

  /**
   * @return {boolean} Whether to hide the hint.
   * @private
   */
  hintHidden_: function() {
    return this.errorState_ == PagesInputErrorState.NO_ERROR;
  }
});

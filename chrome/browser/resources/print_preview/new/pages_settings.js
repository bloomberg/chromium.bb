// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

/** @enum {number} */
const PagesInputErrorState = {
  NO_ERROR: 0,
  INVALID_SYNTAX: 1,
  OUT_OF_BOUNDS: 2,
};

/** @enum {number} */
const PagesValue = {
  ALL: 0,
  CUSTOM: 1,
};

Polymer({
  is: 'print-preview-pages-settings',

  behaviors: [
    SettingsBehavior, print_preview_new.InputBehavior,
    print_preview_new.SelectBehavior
  ],

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

    disabled: Boolean,

    /** @private {boolean} */
    customSelected_: {
      type: Boolean,
      value: false,
    },

    /** @private {number} */
    errorState_: {
      type: Number,
      reflectToAttribute: true,
      value: PagesInputErrorState.NO_ERROR,
    },

    /** @private {!Array<number>} */
    pagesToPrint_: {
      type: Array,
      computed: 'computePagesToPrint_(' +
          'inputString_, customSelected_, allPagesArray_)',
    },

    /** @private {!Array<{to: number, from: number}>} */
    rangesToPrint_: {
      type: Array,
      computed: 'computeRangesToPrint_(pagesToPrint_, allPagesArray_)',
    },

    /**
     * Mirroring the enum so that it can be used from HTML bindings.
     * @private
     */
    pagesValueEnum_: {
      type: Object,
      value: PagesValue,
    },
  },

  observers: [
    'onRangeChange_(errorState_, rangesToPrint_, settings.pages, ' +
        'settings.pagesPerSheet.value)',
  ],

  listeners: {
    'input-change': 'onInputChange_',
  },

  // Initialize in attached() since this doesn't observe settings.pages.
  attached: function() {
    this.selectedValue = PagesValue.ALL.toString();
  },

  /** @return {!CrInputElement} The cr-input field element for InputBehavior. */
  getInput: function() {
    return this.$.pageSettingsCustomInput;
  },

  /**
   * @param {!CustomEvent} e Contains the new input value.
   * @private
   */
  onInputChange_: function(e) {
    this.inputString_ = /** @type {string} */ (e.detail);
  },

  onProcessSelectChange: function(value) {
    this.customSelected_ = value === PagesValue.CUSTOM.toString();
    if (this.customSelected_) {
      /** @type {!CrInputElement} */ (this.$.pageSettingsCustomInput)
          .inputElement.focus();
    }
  },

  /**
   * @return {boolean} Whether the controls should be disabled.
   * Does not need to depend on |errorState_|, since |errorState_| is always
   * defined and any change to or from NO_ERROR will also trigger a change in
   * |disabled|.
   * @private
   */
  getDisabled_: function() {
    // Disable the input if other settings are responsible for the error state.
    return this.errorState_ == PagesInputErrorState.NO_ERROR && this.disabled;
  },

  /**
   * @return {!Array<number>}
   * @private
   */
  computeAllPagesArray_: function() {
    // This computed function will unnecessarily get triggered if
    // this.documentInfo is set to a new object, which happens whenever the
    // preview refreshes, but the page count is the same as before. We do not
    // want to trigger all observers unnecessarily.
    if (!!this.allPagesArray_ &&
        this.allPagesArray_.length == this.documentInfo.pageCount) {
      return this.allPagesArray_;
    }

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
    if (!this.customSelected_ || this.inputString_ === '') {
      this.errorState_ = PagesInputErrorState.NO_ERROR;
      return this.allPagesArray_;
    }

    const pages = [];
    const added = {};
    const ranges = this.inputString_.split(/,|\u3001/);
    const maxPage = this.allPagesArray_.length;
    for (let range of ranges) {
      range = range.trim();
      if (range == '') {
        this.errorState_ = PagesInputErrorState.INVALID_SYNTAX;
        this.onRangeChange_();
        return this.pagesToPrint_;
      }

      const limits = range.split('-');
      if (limits.length > 2) {
        this.errorState_ = PagesInputErrorState.INVALID_SYNTAX;
        this.onRangeChange_();
        return this.pagesToPrint_;
      }

      let min = Number.parseInt(limits[0], 10);
      if ((limits[0].length > 0 && Number.isNaN(min)) || min < 1) {
        this.errorState_ = PagesInputErrorState.INVALID_SYNTAX;
        this.onRangeChange_();
        return this.pagesToPrint_;
      }
      if (limits.length == 1) {
        if (min > maxPage) {
          this.errorState_ = PagesInputErrorState.OUT_OF_BOUNDS;
          this.onRangeChange_();
          return this.pagesToPrint_;
        }
        if (!added.hasOwnProperty(min)) {
          pages.push(min);
          added[min] = true;
        }
        continue;
      }

      let max = Number.parseInt(limits[1], 10);
      if (Number.isNaN(max) && limits[1].length > 0) {
        this.errorState_ = PagesInputErrorState.INVALID_SYNTAX;
        this.onRangeChange_();
        return this.pagesToPrint_;
      }

      if (Number.isNaN(min))
        min = 1;
      if (Number.isNaN(max))
        max = maxPage;
      if (min > max) {
        this.errorState_ = PagesInputErrorState.INVALID_SYNTAX;
        this.onRangeChange_();
        return this.pagesToPrint_;
      }
      if (max > maxPage) {
        this.errorState_ = PagesInputErrorState.OUT_OF_BOUNDS;
        this.onRangeChange_();
        return this.pagesToPrint_;
      }
      for (let i = min; i <= max; i++) {
        if (!added.hasOwnProperty(i)) {
          pages.push(i);
          added[i] = true;
        }
      }
    }
    this.errorState_ = PagesInputErrorState.NO_ERROR;
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
        this.pagesToPrint_ == this.allPagesArray_) {
      return [];
    }

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
   * @return {!Array<number>} The final page numbers, reflecting N-up setting.
   *     Page numbers are 1 indexed, since these numbers are displayed to the
   *     user.
   * @private
   */
  getNupPages_: function() {
    const pagesPerSheet =
        /** @type {number} */ (this.getSettingValue('pagesPerSheet'));
    if (pagesPerSheet <= 1 || this.pagesToPrint_.length == 0)
      return this.pagesToPrint_;

    const numPages = Math.ceil(this.pagesToPrint_.length / pagesPerSheet);
    const nupPages = new Array(numPages);
    for (let i = 0; i < nupPages.length; i++)
      nupPages[i] = i + 1;
    return nupPages;
  },

  /**
   * Updates the model with pages and validity, and adds error styling if
   * needed.
   * @private
   */
  onRangeChange_: function() {
    if (this.settings === undefined || this.pagesToPrint_ === undefined)
      return;

    if (this.errorState_ != PagesInputErrorState.NO_ERROR) {
      this.setSettingValid('pages', false);
      this.$.pageSettingsCustomInput.invalid = true;
      return;
    }
    this.$.pageSettingsCustomInput.invalid = false;
    this.setSettingValid('pages', true);
    const nupPages = this.getNupPages_();
    const rangesChanged = !areRangesEqual(
        this.rangesToPrint_,
        /** @type {!Array} */ (this.getSettingValue('ranges')));
    if (rangesChanged ||
        nupPages.length != this.getSettingValue('pages').length) {
      this.setSetting('pages', nupPages);
    }
    if (rangesChanged)
      this.setSetting('ranges', this.rangesToPrint_);
  },

  /** @private */
  onSelectBlur_: function(event) {
    if (!this.customSelected_ ||
        event.relatedTarget === this.$.pageSettingsCustomInput) {
      return;
    }

    this.onCustomInputBlur_(event);
  },

  /** @private */
  resetIfEmpty_: function() {
    if (this.inputString_ === '') {
      this.customSelected_ = false;
      this.selectedValue = PagesValue.ALL.toString();
    }
  },

  /**
   * @param {!KeyboardEvent} e The keyboard event
   */
  onKeydown_: function(e) {
    if (e.key == 'Enter') {
      this.resetAndUpdate();
      this.resetIfEmpty_();
    }
  },

  /**
   * @param {Event} event Contains information about where focus is going.
   * @private
   */
  onCustomInputBlur_: function(event) {
    this.resetAndUpdate();

    if (event.relatedTarget != this.$.customInputWrapper &&
        event.relatedTarget != this.$$('.md-select')) {
      this.resetIfEmpty_();
    }
  },

  /**
   * @return {string} Gets message to show as hint.
   * @private
   */
  getHintMessage_: function() {
    if (this.errorState_ == PagesInputErrorState.NO_ERROR)
      return '';

    let formattedMessage = '';
    if (this.errorState_ == PagesInputErrorState.INVALID_SYNTAX) {
      formattedMessage = loadTimeData.getStringF(
          'pageRangeSyntaxInstruction',
          loadTimeData.getString('examplePageRangeText'));
    } else {
      formattedMessage = (this.documentInfo === undefined) ?
          '' :
          loadTimeData.getStringF(
              'pageRangeLimitInstructionWithValue',
              this.documentInfo.pageCount);
    }
    return formattedMessage.replace(/<\/b>|<b>/g, '');
  },

  /**
   * @return {boolean} Whether to hide the hint.
   * @private
   */
  hintHidden_: function() {
    return this.errorState_ == PagesInputErrorState.NO_ERROR;
  }
});
})();

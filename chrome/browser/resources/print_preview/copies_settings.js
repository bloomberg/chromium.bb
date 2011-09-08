// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Creates a CopiesSettings object.
   * @constructor
   */
  function CopiesSettings() {
    this.copiesOption_ = $('copies-option');
    this.textfield_ = $('copies');
    this.incrementButton_ = $('increment');
    this.decrementButton_ = $('decrement');
    // Minimum allowed value for number of copies.
    this.minValue_ = 1;
    // Maximum allowed value for number of copies.
    this.maxValue_ = 999;
    this.collateOption_ = $('collate-option');
    this.collateCheckbox_ = $('collate');
    this.hint_ = $('copies-hint');
    this.twoSidedCheckbox_ = $('two-sided');
  }

  cr.addSingletonGetter(CopiesSettings);

  CopiesSettings.prototype = {
    /**
     * The number of copies represented by the contents of |this.textfield_|.
     * If the text is not valid returns |this.minValue_|.
     * @type {number}
     */
    get numberOfCopies() {
      var value = parseInt(this.textfield_.value, 10);
      if (!value || value <= this.minValue_)
        value = this.minValue_;
      return value;
    },

    /**
     * Getter method for |collateOption_|.
     * @type {HTMLElement}
     */
    get collateOption() {
      return this.collateOption_;
    },

    /**
     * Getter method for |twoSidedCheckbox_|.
     * @type {HTMLInputElement}
     */
    get twoSidedCheckbox() {
      return this.twoSidedCheckbox_;
    },

    /**
     * Gets the duplex mode for printing.
     * @return {number} duplex mode.
     */
     get duplexMode() {
      // Constant values matches printing::DuplexMode enum. Not using const
      // keyword because it is not allowed by JS strict mode.
      var SIMPLEX = 0;
      var LONG_EDGE = 1;
      return !this.twoSidedCheckbox_.checked ? SIMPLEX : LONG_EDGE;
    },

    /**
     * @return {boolean} true if |this.textfield_| is empty, or represents a
     *     positive integer value.
     */
    isValid: function() {
      return !this.textfield_.value || isPositiveInteger(this.textfield_.value);
    },

    /**
     * Checks whether the preview collate setting value is set or not.
     * @return {boolean} true if collate option is enabled and checked.
     */
    isCollated: function() {
      return !this.collateOption_.hidden && this.collateCheckbox_.checked;
    },

    /**
     * Resets |this.textfield_| to |this.minValue_|.
     * @private
     */
    reset_: function() {
      this.textfield_.value = this.minValue_;
    },

    /**
     * Listener function to execute whenever the increment/decrement buttons are
     * clicked.
     * @private
     * @param {int} sign Must be 1 for an increment button click and -1 for a
     *     decrement button click.
     */
    onButtonClicked_: function(sign) {
      if (!this.isValid()) {
        this.reset_();
      } else {
        var newValue = this.numberOfCopies + sign;
        this.textfield_.value = newValue;
      }
      this.updateButtonsState_();
      this.showHideCollateOption_();

      if (!hasPendingPreviewRequest) {
        cr.dispatchSimpleEvent(document, 'updateSummary');
        cr.dispatchSimpleEvent(document, 'updatePrintButton');
      }
    },

    /**
     * Listener function to execute whenever a change occurs in |textfield_|
     * textfield.
     * @private
     */
    onTextfieldChanged_: function() {
      this.updateButtonsState_();
      this.showHideCollateOption_();
      if (!hasPendingPreviewRequest) {
        cr.dispatchSimpleEvent(document, 'updateSummary');
        cr.dispatchSimpleEvent(document, 'updatePrintButton');
      }
    },

    /**
     * Adding listeners to all copies related controls. The listeners take care
     * of altering their behavior depending on |hasPendingPreviewRequest|.
     */
    addEventListeners: function() {
      this.textfield_.oninput = this.onTextfieldChanged_.bind(this);
      this.incrementButton_.onclick = this.onIncrementButtonClicked_.bind(this);
      this.decrementButton_.onclick = this.onDecrementButtonClicked_.bind(this);
      this.twoSidedCheckbox_.onclick = function() {
        if (!hasPendingPreviewRequest)
          cr.dispatchSimpleEvent(document, 'updateSummary');
      }
      document.addEventListener('PDFLoaded',
                                this.updateButtonsState_.bind(this));
      document.addEventListener('printerCapabilitiesUpdated',
                                this.onPrinterCapabilitiesUpdated_.bind(this));
    },

    /**
     * Listener triggered when a printerCapabilitiesUpdated event occurs.
     * @private
     */
    onPrinterCapabilitiesUpdated_: function(e) {
      if (e.printerCapabilities.disableCopiesOption) {
        fadeOutElement(this.copiesOption_);
        $('hr-before-copies').classList.remove('invisible');
      } else {
        fadeInElement(this.copiesOption_);
        $('hr-before-copies').classList.add('invisible');
      }
      this.twoSidedCheckbox_.checked = e.printerCapabilities.setDuplexAsDefault;
    },

    /**
     * Listener triggered when |incrementButton_| is clicked.
     * @private
     */
    onIncrementButtonClicked_: function() {
      this.onButtonClicked_(1);
    },

    /**
     * Listener triggered when |decrementButton_| is clicked.
     * @private
     */
    onDecrementButtonClicked_: function() {
      this.onButtonClicked_(-1);
    },

    /**
     * Takes care of showing/hiding the collate option.
     * @private
     */
    showHideCollateOption_: function() {
      this.collateOption_.hidden = this.numberOfCopies <= 1;
      // TODO(aayushkumar): Remove aria-hidden attribute once elements within
      // the hidden attribute are no longer read out by a screen-reader.
      // (Currently a bug in webkit).
      this.collateOption_.setAttribute('aria-hidden',
                                       this.collateOption_.hidden);
    },

    /**
     * Updates the state of the increment/decrement buttons based on the current
     * |textfield_| value.
     * @private
     */
    updateButtonsState_: function() {
      if (!this.isValid()) {
        this.textfield_.classList.add('invalid');
        this.incrementButton_.disabled = false;
        this.decrementButton_.disabled = false;
        fadeInElement(this.hint_);
      } else {
        this.textfield_.classList.remove('invalid');
        this.incrementButton_.disabled = this.numberOfCopies == this.maxValue_;
        this.decrementButton_.disabled = this.numberOfCopies == this.minValue_;
        fadeOutElement(this.hint_);
      }
      this.hint_.setAttribute('aria-hidden', this.isValid());
    }
  };

  return {
    CopiesSettings: CopiesSettings,
  };
});

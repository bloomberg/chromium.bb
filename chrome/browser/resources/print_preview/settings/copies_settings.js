// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Component that renders the copies settings UI.
   * @param {!print_preview.PrintTicketStore} printTicketStore Used to read and
   *     write the copies settings.
   * @constructor
   * @extends {print_preview.Component}
   */
  function CopiesSettings(printTicketStore) {
    print_preview.Component.call(this);

    /**
     * Used for writing to the print ticket and validating inputted values.
     * @type {!print_preview.PrintTicketStore}
     * @private
     */
    this.printTicketStore_ = printTicketStore;

    /**
     * Timeout used to delay processing of the copies input.
     * @type {?number}
     * @private
     */
    this.textfieldTimeout_ = null;

    /**
     * Whether this component is enabled or not.
     * @type {boolean}
     * @private
     */
    this.isEnabled_ = true;

    /**
     * Textfield for entering copies values.
     * @type {HTMLInputElement}
     * @private
     */
    this.textfield_ = null;

    /**
     * Increment button used to increment the copies value.
     * @type {HTMLButtonElement}
     * @private
     */
    this.incrementButton_ = null;

    /**
     * Decrement button used to decrement the copies value.
     * @type {HTMLButtonElement}
     * @private
     */
    this.decrementButton_ = null;

    /**
     * Container div for the collate checkbox.
     * @type {HTMLDivElement}
     * @private
     */
    this.collateDiv_ = null;

    /**
     * Checkbox used to enable/disable collation.
     * @type {HTMLInputElement}
     * @private
     */
    this.collateCheckbox_ = null;

    /**
     * Hint element used to show a hint when the copies value is invalid.
     * @type {HTMLElement}
     * @private
     */
    this.hintEl_ = null;
  };

  /**
   * CSS classes used by the component.
   * @enum {string}
   * @private
   */
  CopiesSettings.Classes_ = {
    COPIES: 'copies-settings-copies',
    INCREMENT: 'copies-settings-increment',
    DECREMENT: 'copies-settings-decrement',
    HINT: 'copies-settings-hint',
    COLLATE: 'copies-settings-collate',
    COLLATE_CHECKBOX: 'copies-settings-collate-checkbox',
  };

  /**
   * Delay in milliseconds before processing the textfield.
   * @type {number}
   * @private
   */
  CopiesSettings.TEXTFIELD_DELAY_ = 250;

  CopiesSettings.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @param {boolean} isEnabled Whether the copies settings is enabled. */
    set isEnabled(isEnabled) {
      this.textfield_.disabled = !isEnabled;
      this.collateCheckbox_.disabled = !isEnabled;
      this.isEnabled_ = isEnabled;
      if (isEnabled) {
        this.updateState_();
      } else {
        this.textfield_.disabled = true;
        this.incrementButton_.disabled = true;
        this.decrementButton_.disabled = true;
      }
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);

      this.tracker.add(
          this.textfield_, 'keyup', this.onTextfieldKeyUp_.bind(this));
      this.tracker.add(
          this.textfield_, 'blur', this.onTextfieldBlur_.bind(this));
      this.tracker.add(
          this.incrementButton_, 'click', this.onButtonClicked_.bind(this, 1));
      this.tracker.add(
          this.decrementButton_, 'click', this.onButtonClicked_.bind(this, -1));
      this.tracker.add(
          this.collateCheckbox_,
          'click',
          this.onCollateCheckboxClick_.bind(this));
      this.tracker.add(
          this.printTicketStore_,
          print_preview.PrintTicketStore.EventType.INITIALIZE,
          this.updateState_.bind(this));
      this.tracker.add(
          this.printTicketStore_,
          print_preview.PrintTicketStore.EventType.CAPABILITIES_CHANGE,
          this.updateState_.bind(this));
      this.tracker.add(
          this.printTicketStore_,
          print_preview.PrintTicketStore.EventType.TICKET_CHANGE,
          this.updateState_.bind(this));
    },

    /** @override */
    exitDocument: function() {
      print_preview.Component.prototype.exitDocument.call(this);
      this.textfield_ = null;
      this.incrementButton_ = null;
      this.decrementButton_ = null;
      this.collateDiv_ = null;
      this.collateCheckbox_ = null;
      this.hintEl_ = null;
    },

    /** @override */
    decorateInternal: function() {
      this.textfield_ = this.getElement().getElementsByClassName(
          CopiesSettings.Classes_.COPIES)[0];
      this.incrementButton_ = this.getElement().getElementsByClassName(
          CopiesSettings.Classes_.INCREMENT)[0];
      this.decrementButton_ = this.getElement().getElementsByClassName(
          CopiesSettings.Classes_.DECREMENT)[0];
      this.collateDiv_ = this.getElement().getElementsByClassName(
          CopiesSettings.Classes_.COLLATE)[0];
      this.collateCheckbox_ = this.getElement().getElementsByClassName(
          CopiesSettings.Classes_.COLLATE_CHECKBOX)[0];
      this.hintEl_ = this.getElement().getElementsByClassName(
          CopiesSettings.Classes_.HINT)[0];
    },

    /**
     * Updates the state of the copies settings UI controls.
     * @private
     */
    updateState_: function() {
      if (!this.printTicketStore_.hasCopiesCapability()) {
        fadeOutOption(this.getElement());
        return;
      }

      if (this.textfield_.value != this.printTicketStore_.getCopiesStr()) {
        this.textfield_.value = this.printTicketStore_.getCopiesStr();
      }

      var currentValueGreaterThan1 = false;
      if (this.printTicketStore_.isCopiesValid()) {
        this.textfield_.classList.remove('invalid');
        fadeOutElement(this.hintEl_);
        this.hintEl_.setAttribute('aria-hidden', true);
        var currentValue = parseInt(this.printTicketStore_.getCopiesStr());
        var currentValueGreaterThan1 = currentValue > 1;
        this.incrementButton_.disabled =
            !this.isEnabled_ ||
            !this.printTicketStore_.isCopiesValidForValue(currentValue + 1);
        this.decrementButton_.disabled =
            !this.isEnabled_ ||
            !this.printTicketStore_.isCopiesValidForValue(currentValue - 1);
      } else {
        this.textfield_.classList.add('invalid');
        this.hintEl_.setAttribute('aria-hidden', false);
        fadeInElement(this.hintEl_);
        this.incrementButton_.disabled = true;
        this.decrementButton_.disabled = true;
      }

      if (!(this.collateDiv_.hidden =
             !this.printTicketStore_.hasCollateCapability() ||
             !currentValueGreaterThan1)) {
        this.collateCheckbox_.checked =
            this.printTicketStore_.isCollateEnabled();
      }

      fadeInOption(this.getElement());
    },

    /**
     * Called whenever the increment/decrement buttons are clicked.
     * @param {number} delta Must be 1 for an increment button click and -1 for
     *     a decrement button click.
     * @private
     */
    onButtonClicked_: function(delta) {
      // Assumes text field has a valid number.
      var newValue = parseInt(this.textfield_.value) + delta;
      this.printTicketStore_.updateCopies(newValue);
    },

    /**
     * Called after a timeout after user input into the textfield.
     * @private
     */
    onTextfieldTimeout_: function() {
      if (this.textfield_ != '') {
        this.printTicketStore_.updateCopies(this.textfield_.value);
      }
    },

    /**
     * Called when a keyup event occurs on the textfield. Starts an input
     * timeout.
     * @param {Event} event Contains the pressed key.
     * @private
     */
    onTextfieldKeyUp_: function(event) {
      if (this.textfieldTimeout_) {
        clearTimeout(this.textfieldTimeout_);
      }
      this.textfieldTimeout_ = setTimeout(
          this.onTextfieldTimeout_.bind(this), CopiesSettings.TEXTFIELD_DELAY_);
    },

    /**
     * Called when the focus leaves the textfield. If the textfield is empty,
     * its value is set to 1.
     * @private
     */
    onTextfieldBlur_: function() {
      if (this.textfield_.value == '') {
        this.printTicketStore_.updateCopies('1');
      }
    },

    /**
     * Called when the collate checkbox is clicked. Updates the print ticket.
     * @private
     */
    onCollateCheckboxClick_: function() {
      this.printTicketStore_.updateCollate(this.collateCheckbox_.checked);
    }
  };

  // Export
  return {
    CopiesSettings: CopiesSettings
  };
});

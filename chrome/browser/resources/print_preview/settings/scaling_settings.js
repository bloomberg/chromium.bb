// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Component that renders the scaling settings UI.
   * @param {!print_preview.ticket_items.Scaling} scalingTicketItem
   *     Used to read and write the scaling value.
   * @param {!print_preview.ticket_items.FitToPage}
   *     fitToPageTicketItem Used to read the fit to page value.
   * @constructor
   * @extends {print_preview.SettingsSection}
   */
  function ScalingSettings(scalingTicketItem, fitToPageTicketItem) {
    print_preview.SettingsSection.call(this);

    /**
     * Used to read and write the scaling value.
     * @private {!print_preview.ticket_items.Scaling}
     */
    this.scalingTicketItem_ = scalingTicketItem;

    /**
     * Used to read the fit to page value
     * @private {!print_preview.ticket_items.FitToPage}
     */
    this.fitToPageTicketItem_ = fitToPageTicketItem;

    /**
     * The scaling percentage required to fit the document to page
     * @private {number}
     */
    this.fitToPageScaling_ = 100;

    /**
     * Timeout used to delay processing of the scaling input, in ms.
     * @private {?number}
     */
    this.textfieldTimeout_ = null;

    /**
     * Whether this component is enabled or not.
     * @private {boolean}
     */
    this.isEnabled_ = true;

    /**
     * The textfield input element
     * @private {HTMLElement}
     */
    this.inputField_ = null;
  }

  /**
   * Delay in milliseconds before processing the textfield.
   * @private {number}
   * @const
   */
  ScalingSettings.TEXTFIELD_DELAY_MS_ = 250;

  ScalingSettings.prototype = {
    __proto__: print_preview.SettingsSection.prototype,

    /** @override */
    isAvailable: function() {
      return this.scalingTicketItem_.isCapabilityAvailable();
    },

    /** @override */
    hasCollapsibleContent: function() {
      return this.isAvailable();
    },

    /** @override */
    set isEnabled(isEnabled) {
      this.inputField_.disabled = !isEnabled;
      this.isEnabled_ = isEnabled;
      if (isEnabled)
        this.updateState_();
    },

    /** @override */
    enterDocument: function() {
      this.inputField_ = assert(this.getChildElement('input.user-value'));
      print_preview.SettingsSection.prototype.enterDocument.call(this);
      this.tracker.add(
          this.inputField_, 'keydown', this.onTextfieldKeyDown_.bind(this));
      this.tracker.add(
          this.inputField_, 'input', this.onTextfieldInput_.bind(this));
      this.tracker.add(
          this.inputField_, 'blur', this.onTextfieldBlur_.bind(this));
      this.tracker.add(
          this.scalingTicketItem_,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.updateState_.bind(this));
      this.tracker.add(
          this.fitToPageTicketItem_,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onFitToPageChange_.bind(this));
    },

    /**
     * @return {boolean} true if fit to page is available and selected.
     * @private
     */
    isFitToPageSelected: function() {
      return this.fitToPageTicketItem_.isCapabilityAvailable() &&
          !!this.fitToPageTicketItem_.getValue();
    },

    /**
     * @return {number} The current input field value as an integer.
     * @private
     */
    getInputAsNumber: function() {
      return this.inputField_.valueAsNumber;
    },

    /**
     * Display the fit to page scaling in the scaling field if there is a valid
     * fit to page scaling value. If not, make the field blank.
     * @private
     */
    displayFitToPageScaling: function() {
      this.inputField_.value = this.fitToPageScaling_ || '';
    },

    /**
     * Whether the displayed scaling value matches the fit to page scaling.
     * @private
     */
    displayMatchesFitToPage: function() {
      return (
          this.getInputAsNumber() == this.fitToPageScaling_ ||
          (this.inputField_.value == '' && !this.fitToPageScaling_));
    },

    /**
     * Updates the fit to page scaling value of the scalings settings UI.
     * @param {number} fitToPageScaling The updated percentage scaling required
     *     to fit the document to page.
     */
    updateFitToPageScaling: function(fitToPageScaling) {
      this.fitToPageScaling_ = fitToPageScaling;
      // Display the new value if fit to page is checked.
      if (this.isFitToPageSelected())
        this.displayFitToPageScaling();
    },

    /**
     * Updates the state of the scaling settings UI controls.
     * @private
     */
    updateState_: function() {
      if (this.isAvailable()) {
        var displayedValue = this.getInputAsNumber();
        // If fit to page is selected and the display matches, mark valid
        // and return.
        if (this.isFitToPageSelected() && this.displayMatchesFitToPage()) {
          this.inputField_.classList.remove('invalid');
          fadeOutElement(this.getChildElement('.hint'));
          this.updateUiStateInternal();
          return;
        }

        if (!this.inputField_.validity.valid) {
          this.inputField_.classList.add('invalid');
          fadeInElement(this.getChildElement('.hint'));
          this.updateUiStateInternal();
          return;
        }

        this.inputField_.value = this.scalingTicketItem_.getValue();
        this.inputField_.classList.remove('invalid');
        fadeOutElement(this.getChildElement('.hint'));
      }
      this.updateUiStateInternal();
    },

    /**
     * Helper function to adjust the scaling display if fit to page is checked
     * by the user.
     * @private
     */
    onFitToPageChange_: function() {
      if (this.isFitToPageSelected()) {
        // Fit to page was checked. Set scaling to the fit to page scaling.
        this.displayFitToPageScaling();
      } else if (
          this.fitToPageTicketItem_.isCapabilityAvailable() &&
          this.displayMatchesFitToPage()) {
        // Fit to page unchecked. Return to last scaling.
        this.inputField_.value = this.scalingTicketItem_.getValue();
      }
    },

    /**
     * Called after a timeout after user input into the textfield.
     * @private
     */
    onTextfieldTimeout_: function() {
      this.textfieldTimeout_ = null;
      if (!this.inputField_.validity.valid) {
        this.updateState_();
        return;
      }
      if (this.isFitToPageSelected() && !this.displayMatchesFitToPage()) {
        // User modified value away from fit to page.
        this.fitToPageTicketItem_.updateValue(false);
      }
      // Check this after checking for fit to page in case the user went
      // back to their old value.
      if (this.inputField_.valueAsNumber.toString() ===
          this.scalingTicketItem_.getValue()) {
        this.updateState_();
        return;
      }
      if (this.inputField_.value == '')
        return;
      this.scalingTicketItem_.updateValue(
          this.inputField_.valueAsNumber.toString());
    },

    /**
     * Called when a key is pressed on the custom input.
     * @param {!Event} event Contains the key that was pressed.
     * @private
     */
    onTextfieldKeyDown_: function(event) {
      if (event.keyCode != 'Enter')
        return;

      if (this.textfieldTimeout_)
        clearTimeout(this.textfieldTimeout_);
      this.onTextfieldTimeout_();
    },

    /**
     * Called when a input event occurs on the textfield. Starts an input
     * timeout.
     * @private
     */
    onTextfieldInput_: function() {
      if (this.textfieldTimeout_) {
        clearTimeout(this.textfieldTimeout_);
      }
      this.textfieldTimeout_ = setTimeout(
          this.onTextfieldTimeout_.bind(this),
          ScalingSettings.TEXTFIELD_DELAY_MS_);
    },

    /**
     * Called when the focus leaves the textfield. If the textfield is empty,
     * its value is set to the fit to page value if fit to page is checked and
     * 100 otherwise.
     * @private
     */
    onTextfieldBlur_: function() {
      if (this.inputField_.value == '' && this.inputField_.validity.valid) {
        if (this.isFitToPageSelected()) {
          this.displayFitToPageScaling();
        } else {
          if (this.scalingTicketItem_.getValue() == '100')
            // No need to update the ticket, but change the display to match.
            this.inputField_.value = '100';
          else
            this.scalingTicketItem_.updateValue('100');
        }
      }
    },

  };

  // Export
  return {ScalingSettings: ScalingSettings};
});

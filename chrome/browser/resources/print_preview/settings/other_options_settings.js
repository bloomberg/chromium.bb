// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * UI component that renders checkboxes for various print options.
   * @param {!print_preview.PrintTicketStore} printTicketStore Used to monitor
   *     the state of the print ticket.
   * @constructor
   * @extends {print_preview.Component}
   */
  function OtherOptionsSettings(printTicketStore) {
    print_preview.Component.call(this);

    /**
     * Used to monitor the state of the print ticket.
     * @type {!print_preview.PrintTicketStore}
     * @private
     */
    this.printTicketStore_ = printTicketStore;

    /**
     * Header footer container element.
     * @type {HTMLElement}
     * @private
     */
    this.headerFooterContainer_ = null;

    /**
     * Header footer checkbox.
     * @type {HTMLInputElement}
     * @private
     */
    this.headerFooterCheckbox_ = null;

    /**
     * Fit-to-page container element.
     * @type {HTMLElement}
     * @private
     */
    this.fitToPageContainer_ = null;

    /**
     * Fit-to-page checkbox.
     * @type {HTMLInputElement}
     * @private
     */
    this.fitToPageCheckbox_ = null;

    /**
     * Duplex container element.
     * @type {HTMLElement}
     * @private
     */
    this.duplexContainer_ = null;

    /**
     * Duplex checkbox.
     * @type {HTMLInputElement}
     * @private
     */
    this.duplexCheckbox_ = null;
  };

  OtherOptionsSettings.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @param {boolean} isEnabled Whether the settings is enabled. */
    set isEnabled(isEnabled) {
      this.headerFooterCheckbox_.disabled = !isEnabled;
      this.fitToPageCheckbox_.disabled = !isEnabled;
      this.duplexCheckbox_.disabled = !isEnabled;
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);
      this.tracker.add(
          this.headerFooterCheckbox_,
          'click',
          this.onHeaderFooterCheckboxClick_.bind(this));
      this.tracker.add(
          this.fitToPageCheckbox_,
          'click',
          this.onFitToPageCheckboxClick_.bind(this));
      this.tracker.add(
          this.duplexCheckbox_,
          'click',
          this.onDuplexCheckboxClick_.bind(this));
      this.tracker.add(
          this.printTicketStore_,
          print_preview.PrintTicketStore.EventType.INITIALIZE,
          this.onPrintTicketStoreChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_,
          print_preview.PrintTicketStore.EventType.DOCUMENT_CHANGE,
          this.onPrintTicketStoreChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_,
          print_preview.PrintTicketStore.EventType.CAPABILITIES_CHANGE,
          this.onPrintTicketStoreChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_,
          print_preview.PrintTicketStore.EventType.TICKET_CHANGE,
          this.onPrintTicketStoreChange_.bind(this));
    },

    /** @override */
    exitDocument: function() {
      print_preview.Component.prototype.exitDocument.call(this);
      this.headerFooterContainer_ = null;
      this.headerFooterCheckbox_ = null;
      this.fitToPageContainer_ = null;
      this.fitToPageCheckbox_ = null;
      this.duplexContainer_ = null;
      this.duplexCheckbox_ = null;
    },

    /** @override */
    decorateInternal: function() {
      this.headerFooterContainer_ = this.getElement().querySelector(
          '.header-footer-container');
      this.headerFooterCheckbox_ = this.headerFooterContainer_.querySelector(
          '.header-footer-checkbox');
      this.fitToPageContainer_ = this.getElement().querySelector(
          '.fit-to-page-container');
      this.fitToPageCheckbox_ = this.fitToPageContainer_.querySelector(
          '.fit-to-page-checkbox');
      this.duplexContainer_ = this.getElement().querySelector(
          '.duplex-container');
      this.duplexCheckbox_ = this.duplexContainer_.querySelector(
          '.duplex-checkbox');
    },

    /**
     * Called when the header-footer checkbox is clicked. Updates the print
     * ticket.
     * @private
     */
    onHeaderFooterCheckboxClick_: function() {
      this.printTicketStore_.updateHeaderFooter(
          this.headerFooterCheckbox_.checked);
    },

    /**
     * Called when the fit-to-page checkbox is clicked. Updates the print
     * ticket.
     * @private
     */
    onFitToPageCheckboxClick_: function() {
      this.printTicketStore_.updateFitToPage(this.fitToPageCheckbox_.checked);
    },

    /**
     * Called when the duplex checkbox is clicked. Updates the print ticket.
     * @private
     */
    onDuplexCheckboxClick_: function() {
      this.printTicketStore_.updateDuplex(this.duplexCheckbox_.checked);
    },

    /**
     * Called when the print ticket store has changed. Hides or shows the
     * setting.
     * @private
     */
    onPrintTicketStoreChange_: function() {
      setIsVisible(this.headerFooterContainer_,
                   this.printTicketStore_.hasHeaderFooterCapability());
      this.headerFooterCheckbox_.checked =
          this.printTicketStore_.isHeaderFooterEnabled();

      setIsVisible(this.fitToPageContainer_,
                   this.printTicketStore_.hasFitToPageCapability());
      this.fitToPageCheckbox_.checked =
          this.printTicketStore_.isFitToPageEnabled();

      setIsVisible(this.duplexContainer_,
                   this.printTicketStore_.hasDuplexCapability());
      this.duplexCheckbox_.checked = this.printTicketStore_.isDuplexEnabled();

      if (this.printTicketStore_.hasHeaderFooterCapability() ||
          this.printTicketStore_.hasFitToPageCapability() ||
          this.printTicketStore_.hasDuplexCapability()) {
        fadeInOption(this.getElement());
      } else {
        fadeOutOption(this.getElement());
      }
    }
  };

  // Export
  return {
    OtherOptionsSettings: OtherOptionsSettings
  };
});

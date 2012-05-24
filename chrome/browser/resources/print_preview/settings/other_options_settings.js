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
    this.headerFooterEl_ = null;

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
    this.fitToPageEl_ = null;

    /**
     * Fit-to-page checkbox.
     * @type {HTMLInputElement}
     * @private
     */
    this.fitToPageCheckbox_ = null;
  };

  /**
   * CSS classes used by the other options component.
   * @enum {string}
   * @private
   */
  OtherOptionsSettings.Classes_ = {
    FIT_TO_PAGE: 'other-options-settings-fit-to-page',
    FIT_TO_PAGE_CHECKBOX: 'other-options-settings-fit-to-page-checkbox',
    HEADER_FOOTER: 'other-options-settings-header-footer',
    HEADER_FOOTER_CHECKBOX: 'other-options-settings-header-footer-checkbox'
  };

  OtherOptionsSettings.prototype = {
    __proto__: print_preview.Component.prototype,

    set isEnabled(isEnabled) {
      this.headerFooterCheckbox_.disabled = !isEnabled;
      this.fitToPageCheckbox_.disabled = !isEnabled;
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
      this.headerFooterEl_ = null;
      this.headerFooterCheckbox_ = null;
      this.fitToPageEl_ = null;
      this.fitToPageCheckbox_ = null;
    },

    /** @override */
    decorateInternal: function() {
      this.headerFooterEl_ = this.getElement().getElementsByClassName(
          OtherOptionsSettings.Classes_.HEADER_FOOTER)[0];
      this.headerFooterCheckbox_ = this.getElement().getElementsByClassName(
          OtherOptionsSettings.Classes_.HEADER_FOOTER_CHECKBOX)[0];
      this.fitToPageEl_ = this.getElement().getElementsByClassName(
          OtherOptionsSettings.Classes_.FIT_TO_PAGE)[0];
      this.fitToPageCheckbox_ = this.getElement().getElementsByClassName(
          OtherOptionsSettings.Classes_.FIT_TO_PAGE_CHECKBOX)[0];
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
      this.printTicketStore_.updateFitToPage(
          this.fitToPageCheckbox_.checked);
    },

    /**
     * Called when the print ticket store has changed. Hides or shows the
     * setting.
     * @private
     */
    onPrintTicketStoreChange_: function() {
      setIsVisible(this.headerFooterEl_,
                   this.printTicketStore_.hasHeaderFooterCapability());
      this.headerFooterCheckbox_.checked =
            this.printTicketStore_.isHeaderFooterEnabled();
      setIsVisible(this.fitToPageEl_,
                   this.printTicketStore_.hasFitToPageCapability());
      this.fitToPageCheckbox_.checked =
          this.printTicketStore_.isFitToPageEnabled();

      if (this.printTicketStore_.hasHeaderFooterCapability() ||
          this.printTicketStore_.hasFitToPageCapability()) {
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

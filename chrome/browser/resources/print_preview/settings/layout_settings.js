// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Creates a LayoutSettings object. This object encapsulates all settings and
   * logic related to layout mode (portrait/landscape).
   * @param {!print_preview.PrintTicketStore} printTicketStore Used to get the
   *     layout written to the print ticket.
   * @constructor
   * @extends {print_preview.Component}
   */
  function LayoutSettings(printTicketStore) {
    print_preview.Component.call(this);

    /**
     * Used to get the layout written to the print ticket.
     * @type {!print_preview.PrintTicketStore}
     * @private
     */
    this.printTicketStore_ = printTicketStore;
  };

  /**
   * CSS classes used by the layout settings.
   * @enum {string}
   * @private
   */
  LayoutSettings.Classes_ = {
    LANDSCAPE_RADIO: 'layout-settings-landscape-radio',
    PORTRAIT_RADIO: 'layout-settings-portrait-radio'
  };

  LayoutSettings.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @param {boolean} isEnabled Whether this component is enabled. */
    set isEnabled(isEnabled) {
      this.landscapeRadioButton_.disabled = !isEnabled;
      this.portraitRadioButton_.disabled = !isEnabled;
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);
      this.tracker.add(
          this.portraitRadioButton_,
          'click',
          this.onLayoutButtonClick_.bind(this));
      this.tracker.add(
          this.landscapeRadioButton_,
          'click',
          this.onLayoutButtonClick_.bind(this));
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
      this.tracker.add(
          this.printTicketStore_,
          print_preview.PrintTicketStore.EventType.INITIALIZE,
          this.onPrintTicketStoreChange_.bind(this));
    },

    /**
     * @return {HTMLInputElement} The portrait orientation radio button.
     * @private
     */
    get portraitRadioButton_() {
      return this.getElement().getElementsByClassName(
          LayoutSettings.Classes_.PORTRAIT_RADIO)[0];
    },

    /**
     * @return {HTMLInputElement} The landscape orientation radio button.
     * @private
     */
    get landscapeRadioButton_() {
      return this.getElement().getElementsByClassName(
          LayoutSettings.Classes_.LANDSCAPE_RADIO)[0];
    },

    /**
     * Called when one of the radio buttons is clicked. Updates the print ticket
     * store.
     * @private
     */
    onLayoutButtonClick_: function() {
      this.printTicketStore_.updateOrientation(
          this.landscapeRadioButton_.checked);
    },

    /**
     * Called when the print ticket store changes state. Updates the state of
     * the radio buttons and hides the setting if necessary.
     * @private
     */
    onPrintTicketStoreChange_: function() {
      if (this.printTicketStore_.hasOrientationCapability()) {
        var isLandscapeEnabled = this.printTicketStore_.isLandscapeEnabled();
        this.portraitRadioButton_.checked = !isLandscapeEnabled;
        this.landscapeRadioButton_.checked = isLandscapeEnabled;
        fadeInOption(this.getElement());
      } else {
        fadeOutOption(this.getElement());
      }
    }
  };

  // Export
  return {
    LayoutSettings: LayoutSettings
  };
});

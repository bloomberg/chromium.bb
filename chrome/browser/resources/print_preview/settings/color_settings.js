// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Creates a ColorSettings object. This object encapsulates all settings and
   * logic related to color selection (color/bw).
   * @param {!print_preview.PrintTicketStore} printTicketStore Used for writing
   *     to the print ticket.
   * @constructor
   * @extends {print_preview.Component}
   */
  function ColorSettings(printTicketStore) {
    print_preview.Component.call(this);

    /**
     * Used for writing to the print ticket.
     * @type {!print_preview.PrintTicketStore}
     * @private
     */
    this.printTicketStore_ = printTicketStore;
  };

  /**
   * CSS classes used by the color settings.
   * @enum {string}
   * @private
   */
  ColorSettings.Classes_ = {
    BW_OPTION: 'color-settings-bw-option',
    COLOR_OPTION: 'color-settings-color-option'
  };

  ColorSettings.prototype = {
    __proto__: print_preview.Component.prototype,

    set isEnabled(isEnabled) {
      this.colorRadioButton_.disabled = !isEnabled;
      this.bwRadioButton_.disabled = !isEnabled;
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);
      this.addEventListeners_();
    },

    get colorRadioButton_() {
      return this.getElement().getElementsByClassName(
          ColorSettings.Classes_.COLOR_OPTION)[0];
    },

    get bwRadioButton_() {
      return this.getElement().getElementsByClassName(
          ColorSettings.Classes_.BW_OPTION)[0];
    },

    /**
     * Adding listeners to all targets and UI controls.
     * @private
     */
    addEventListeners_: function() {
      this.tracker.add(
          this.colorRadioButton_,
          'click',
          this.updatePrintTicket_.bind(this, true));
      this.tracker.add(
          this.bwRadioButton_,
          'click',
          this.updatePrintTicket_.bind(this, false));
      this.tracker.add(
          this.printTicketStore_,
          print_preview.PrintTicketStore.EventType.INITIALIZE,
          this.onCapabilitiesChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_,
          print_preview.PrintTicketStore.EventType.CAPABILITIES_CHANGE,
          this.onCapabilitiesChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_,
          print_preview.PrintTicketStore.EventType.DOCUMENT_CHANGE,
          this.onCapabilitiesChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_,
          print_preview.PrintTicketStore.EventType.TICKET_CHANGE,
          this.onTicketChange_.bind(this));
    },

    /**
     * Updates print ticket with whether the document should be printed in
     * color.
     * @param {boolean} isColor Whether the document should be printed in color.
     * @private
     */
    updatePrintTicket_: function(isColor) {
      this.printTicketStore_.updateColor(isColor);
    },

    /**
     * Called when the ticket store's capabilities have changed. Shows or hides
     * the color settings.
     * @private
     */
    onCapabilitiesChange_: function() {
      if (this.printTicketStore_.hasColorCapability()) {
        fadeInOption(this.getElement());
        var isColorEnabled = this.printTicketStore_.isColorEnabled();
        this.colorRadioButton_.checked = isColorEnabled;
        this.bwRadioButton_.checked = !isColorEnabled;
      } else {
        fadeOutOption(this.getElement());
      }
      this.getElement().setAttribute(
          'aria-hidden', !this.printTicketStore_.hasColorCapability());
    },

    onTicketChange_: function() {
      if (this.printTicketStore_.hasColorCapability()) {
        var isColorEnabled = this.printTicketStore_.isColorEnabled();
        this.colorRadioButton_.checked = isColorEnabled;
        this.bwRadioButton_.checked = !isColorEnabled;
      }
    }
  };

  // Export
  return {
    ColorSettings: ColorSettings
  };
});

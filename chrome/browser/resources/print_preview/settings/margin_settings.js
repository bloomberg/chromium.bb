// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Creates a MarginSettings object. This object encapsulates all settings and
   * logic related to the margins mode.
   * @param {!print_preview.PrintTicketStore} printTicketStore Used to get
   *     ticket margins value.
   * @constructor
   * @extends {print_preview.Component}
   */
  function MarginSettings(printTicketStore) {
    print_preview.Component.call(this);

    /**
     * Used to get ticket margins value.
     * @type {!print_preview.PrintTicketStore}
     * @private
     */
    this.printTicketStore_ = printTicketStore;
  };

  /**
   * CSS classes used by the margin settings component.
   * @enum {string}
   * @private
   */
  MarginSettings.Classes_ = {
    SELECT: 'margin-settings-select'
  };

  MarginSettings.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @param {boolean} isEnabled Whether this component is enabled. */
    set isEnabled(isEnabled) {
      this.select_.disabled = !isEnabled;
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);
      this.tracker.add(
          this.select_, 'change', this.onSelectChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_,
          print_preview.PrintTicketStore.EventType.DOCUMENT_CHANGE,
          this.onPrintTicketStoreChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_,
          print_preview.PrintTicketStore.EventType.TICKET_CHANGE,
          this.onPrintTicketStoreChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_,
          print_preview.PrintTicketStore.EventType.CAPABILITIES_CHANGE,
          this.onPrintTicketStoreChange_.bind(this));
    },

    /**
     * @return {HTMLSelectElement} Select element containing the margin options.
     * @private
     */
    get select_() {
      return this.getElement().getElementsByClassName(
          MarginSettings.Classes_.SELECT)[0];
    },

    /**
     * Called when the select element is changed. Updates the print ticket
     * margin type.
     * @private
     */
    onSelectChange_: function() {
      var select = this.select_;
      var marginsType =
          /** @type {!print_preview.ticket_items.MarginsType.Value} */ (
              select.selectedIndex);
      this.printTicketStore_.updateMarginsType(marginsType);
    },

    /**
     * Called when the print ticket store changes. Selects the corresponding
     * select option.
     * @private
     */
    onPrintTicketStoreChange_: function() {
      if (this.printTicketStore_.hasMarginsCapability()) {
        var select = this.select_;
        var marginsType = this.printTicketStore_.getMarginsType();
        var selectedMarginsType =
            /** @type {!print_preview.ticket_items.MarginsType.Value} */ (
                select.selectedIndex);
        if (marginsType != selectedMarginsType) {
          select.options[selectedMarginsType].selected = false;
          select.options[marginsType].selected = true;
        }
        fadeInOption(this.getElement());
      } else {
        fadeOutOption(this.getElement());
      }
    }
  };

  // Export
  return {
    MarginSettings: MarginSettings
  };
});

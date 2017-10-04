// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview.ticket_items');

/**
 * Must be kept in sync with the C++ MarginType enum in
 * printing/print_job_constants.h.
 * @enum {number}
 */
print_preview.ticket_items.MarginsTypeValue = {
  DEFAULT: 0,
  NO_MARGINS: 1,
  MINIMUM: 2,
  CUSTOM: 3
};


cr.define('print_preview.ticket_items', function() {
  'use strict';

  class MarginsType extends print_preview.ticket_items.TicketItem {
    /**
     * Margins type ticket item whose value is a
     * print_preview.ticket_items.MarginsTypeValue} that indicates what
     * predefined margins type to use.
     * @param {!print_preview.AppState} appState App state persistence object to
     *     save the state of the margins type selection.
     * @param {!print_preview.DocumentInfo} documentInfo Information about the
     *     document to print.
     * @param {!print_preview.ticket_items.CustomMargins} customMargins Custom
     *     margins ticket item, used to write when margins type changes.
     */
    constructor(appState, documentInfo, customMargins) {
      super(
          appState, print_preview.AppStateField.MARGINS_TYPE,
          null /*destinationStore*/, documentInfo);

      /**
       * Custom margins ticket item, used to write when margins type changes.
       * @type {!print_preview.ticket_items.CustomMargins}
       * @private
       */
      this.customMargins_ = customMargins;
    }

    /** @override */
    wouldValueBeValid(value) {
      return true;
    }

    /** @override */
    isCapabilityAvailable() {
      return this.getDocumentInfoInternal().isModifiable;
    }

    /** @override */
    getDefaultValueInternal() {
      return print_preview.ticket_items.MarginsTypeValue.DEFAULT;
    }

    /** @override */
    getCapabilityNotAvailableValueInternal() {
      return print_preview.ticket_items.MarginsTypeValue.DEFAULT;
    }

    /** @override */
    updateValueInternal(value) {
      print_preview.ticket_items.TicketItem.prototype.updateValueInternal.call(
          this, value);
      if (this.isValueEqual(
              print_preview.ticket_items.MarginsTypeValue.CUSTOM)) {
        // If CUSTOM, set the value of the custom margins so that it won't be
        // overridden by the default value.
        this.customMargins_.updateValue(this.customMargins_.getValue());
      }
    }
  }

  // Export
  return {MarginsType: MarginsType};
});

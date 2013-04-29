// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  // TODO(rltoscano): Maybe clear print ticket when destination changes. Or
  // better yet, carry over any print ticket state that is possible. I.e. if
  // destination changes, the new destination might not support duplex anymore,
  // so we should clear the ticket's isDuplexEnabled state.

  /**
   * Storage of the print ticket and document statistics. Dispatches events when
   * the contents of the print ticket or document statistics change. Also
   * handles validation of the print ticket against destination capabilities and
   * against the document.
   * @param {!print_preview.DestinationStore} destinationStore Used to
   *     understand which printer is selected.
   * @param {!print_preview.AppState} appState Print preview application state.
   * @param {!print_preview.DocumentInfo} documentInfo Document data model.
   * @constructor
   * @extends {cr.EventTarget}
   */
  function PrintTicketStore(destinationStore, appState, documentInfo) {
    cr.EventTarget.call(this);

    /**
     * Destination store used to understand which printer is selected.
     * @type {!print_preview.DestinationStore}
     * @private
     */
    this.destinationStore_ = destinationStore;

    /**
     * App state used to persist and load ticket values.
     * @type {!print_preview.AppState}
     * @private
     */
    this.appState_ = appState;

    /**
     * Information about the document to print.
     * @type {!print_preview.DocumentInfo}
     * @private
     */
    this.documentInfo_ = documentInfo;

    /**
     * Printing capabilities of Chromium and the currently selected destination.
     * @type {!print_preview.CapabilitiesHolder}
     * @private
     */
    this.capabilitiesHolder_ = new print_preview.CapabilitiesHolder();

    /**
     * Current measurement system. Used to work with margin measurements.
     * @type {!print_preview.MeasurementSystem}
     * @private
     */
    this.measurementSystem_ = new print_preview.MeasurementSystem(
        ',', '.', print_preview.MeasurementSystem.UnitType.IMPERIAL);

    /**
     * Collate ticket item.
     * @type {!print_preview.ticket_items.Collate}
     * @private
     */
    this.collate_ = new print_preview.ticket_items.Collate(
        this.appState_, this.destinationStore_);

    /**
     * Color ticket item.
     * @type {!print_preview.ticket_items.Color}
     * @private
     */
    this.color_ = new print_preview.ticket_items.Color(
        this.appState_, this.destinationStore_);

    /**
     * Copies ticket item.
     * @type {!print_preview.ticket_items.Copies}
     * @private
     */
    this.copies_ =
        new print_preview.ticket_items.Copies(this.destinationStore_);

    /**
     * Duplex ticket item.
     * @type {!print_preview.ticket_items.Duplex}
     * @private
     */
    this.duplex_ = new print_preview.ticket_items.Duplex(
        this.appState_, this.destinationStore_);

    /**
     * Landscape ticket item.
     * @type {!print_preview.ticket_items.Landscape}
     * @private
     */
    this.landscape_ = new print_preview.ticket_items.Landscape(
        this.capabilitiesHolder_, this.documentInfo_);

    /**
     * Page range ticket item.
     * @type {!print_preview.ticket_items.PageRange}
     * @private
     */
    this.pageRange_ =
        new print_preview.ticket_items.PageRange(this.documentInfo_);

    /**
     * Margins type ticket item.
     * @type {!print_preview.ticket_items.MarginsType}
     * @private
     */
    this.marginsType_ =
        new print_preview.ticket_items.MarginsType(this.documentInfo_);

    /**
     * Custom margins ticket item.
     * @type {!print_preview.ticket_items.CustomMargins}
     * @private
     */
    this.customMargins_ = new print_preview.ticket_items.CustomMargins(
        this.documentInfo_, this.measurementSystem_);

    /**
     * Header-footer ticket item.
     * @type {!print_preview.ticket_items.HeaderFooter}
     * @private
     */
    this.headerFooter_ = new print_preview.ticket_items.HeaderFooter(
        this.documentInfo_, this.marginsType_, this.customMargins_);

    /**
     * Fit-to-page ticket item.
     * @type {!print_preview.ticket_items.FitToPage}
     * @private
     */
    this.fitToPage_ = new print_preview.ticket_items.FitToPage(
        this.documentInfo_, this.destinationStore_);

    /**
     * Print CSS backgrounds ticket item.
     * @type {!print_preview.ticket_items.CssBackground}
     * @private
     */
    this.cssBackground_ = new print_preview.ticket_items.CssBackground(
        this.appState_, this.documentInfo_);

    /**
     * Print selection only ticket item.
     * @type {!print_preview.ticket_items.SelectionOnly}
     * @private
     */
    this.selectionOnly_ =
        new print_preview.ticket_items.SelectionOnly(this.documentInfo_);

    /**
     * Keeps track of event listeners for the print ticket store.
     * @type {!EventTracker}
     * @private
     */
    this.tracker_ = new EventTracker();

    this.addEventListeners_();
  };

  /**
   * Event types dispatched by the print ticket store.
   * @enum {string}
   */
  PrintTicketStore.EventType = {
    CAPABILITIES_CHANGE: 'print_preview.PrintTicketStore.CAPABILITIES_CHANGE',
    DOCUMENT_CHANGE: 'print_preview.PrintTicketStore.DOCUMENT_CHANGE',
    INITIALIZE: 'print_preview.PrintTicketStore.INITIALIZE',
    TICKET_CHANGE: 'print_preview.PrintTicketStore.TICKET_CHANGE'
  };

  PrintTicketStore.prototype = {
    __proto__: cr.EventTarget.prototype,

    get collate() {
      return this.collate_;
    },

    get color() {
      return this.color_;
    },

    get copies() {
      return this.copies_;
    },

    get cssBackground() {
      return this.cssBackground_;
    },

    get duplex() {
      return this.duplex_;
    },

    get fitToPage() {
      return this.fitToPage_;
    },

    get selectionOnly() {
      return this.selectionOnly_;
    },

    /**
     * @return {!print_preview.MeasurementSystem} Measurement system of the
     *     local system.
     */
    get measurementSystem() {
      return this.measurementSystem_;
    },

    /**
     * Initializes the print ticket store. Dispatches an INITIALIZE event.
     * @param {string} thousandsDelimeter Delimeter of the thousands place.
     * @param {string} decimalDelimeter Delimeter of the decimal point.
     * @param {!print_preview.MeasurementSystem.UnitType} unitType Type of unit
     *     of the local measurement system.
     * @param {boolean} selectionOnly Whether only selected content should be
     *     printed.
     */
    init: function(
        thousandsDelimeter, decimalDelimeter, unitType, selectionOnly) {
      this.measurementSystem_.setSystem(thousandsDelimeter, decimalDelimeter,
                                        unitType);
      this.selectionOnly_.updateValue(selectionOnly);

      // Initialize ticket with user's previous values.
      this.marginsType_.updateValue(this.appState_.marginsType);
      this.customMargins_.updateValue(this.appState_.customMargins);
      if (this.appState_.hasField(
          print_preview.AppState.Field.IS_COLOR_ENABLED)) {
        this.color_.updateValue(this.appState_.getField(
            print_preview.AppState.Field.IS_COLOR_ENABLED));
      }
      if (this.appState_.hasField(
          print_preview.AppState.Field.IS_DUPLEX_ENABLED)) {
        this.duplex_.updateValue(this.appState_.getField(
            print_preview.AppState.Field.IS_DUPLEX_ENABLED));
      }
      this.headerFooter_.updateValue(this.appState_.isHeaderFooterEnabled);
      this.landscape_.updateValue(this.appState_.isLandscapeEnabled);
      if (this.appState_.hasField(
          print_preview.AppState.Field.IS_COLLATE_ENABLED)) {
        this.collate_.updateValue(this.appState_.getField(
            print_preview.AppState.Field.IS_COLLATE_ENABLED));
      }
      if (this.appState_.hasField(
          print_preview.AppState.Field.IS_CSS_BACKGROUND_ENABLED)) {
        this.cssBackground_.updateValue(this.appState_.getField(
            print_preview.AppState.Field.IS_CSS_BACKGROUND_ENABLED));
      }
    },

    /** @return {boolean} Whether the header-footer capability is available. */
    hasHeaderFooterCapability: function() {
      return this.headerFooter_.isCapabilityAvailable();
    },

    /** @return {boolean} Whether the header-footer setting is enabled. */
    isHeaderFooterEnabled: function() {
      return this.headerFooter_.getValue();
    },

    /**
     * Updates the whether the header-footer setting is enabled. Dispatches a
     * TICKET_CHANGE event if the setting changed.
     * @param {boolean} isHeaderFooterEnabled Whether the header-footer setting
     *     is enabled.
     */
    updateHeaderFooter: function(isHeaderFooterEnabled) {
      if (this.headerFooter_.getValue() != isHeaderFooterEnabled) {
        this.headerFooter_.updateValue(isHeaderFooterEnabled);
        this.appState_.persistIsHeaderFooterEnabled(isHeaderFooterEnabled);
        cr.dispatchSimpleEvent(this, PrintTicketStore.EventType.TICKET_CHANGE);
      }
    },

    /**
     * @return {boolean} Whether the page orientation capability is available.
     */
    hasOrientationCapability: function() {
      return this.landscape_.isCapabilityAvailable();
    },

    /**
     * @return {boolean} Whether the document should be printed in landscape.
     */
    isLandscapeEnabled: function() {
      return this.landscape_.getValue();
    },

    /**
     * Updates whether the document should be printed in landscape. Dispatches
     * a TICKET_CHANGE event if the setting changes.
     * @param {boolean} isLandscapeEnabled Whether the document should be
     *     printed in landscape.
     */
    updateOrientation: function(isLandscapeEnabled) {
      if (this.landscape_.getValue() != isLandscapeEnabled) {
        this.landscape_.updateValue(isLandscapeEnabled);
        // Reset the user set margins.
        this.marginsType_.updateValue(
            print_preview.ticket_items.MarginsType.Value.DEFAULT);
        this.customMargins_.updateValue(null);
        this.appState_.persistMarginsType(
            print_preview.ticket_items.MarginsType.Value.DEFAULT);
        this.appState_.persistCustomMargins(null);
        this.appState_.persistIsLandscapeEnabled(isLandscapeEnabled);
        cr.dispatchSimpleEvent(this, PrintTicketStore.EventType.TICKET_CHANGE);
      }
    },

    /** @return {boolean} Whether the margins capability is available. */
    hasMarginsCapability: function() {
      return this.marginsType_.isCapabilityAvailable();
    },

    /**
     * @return {!print_preview.ticket_items.MarginsType.Value} Type of
     *     predefined margins.
     */
    getMarginsType: function() {
      return this.marginsType_.getValue();
    },

    /**
     * Updates the type of predefined margins. Dispatches a TICKET_CHANGE event
     * if the margins type changes.
     * @param {print_preview.ticket_items.MarginsType.Value} marginsType Type of
     *     predefined margins.
     */
    updateMarginsType: function(marginsType) {
      if (this.marginsType_.getValue() != marginsType) {
        this.marginsType_.updateValue(marginsType);
        this.appState_.persistMarginsType(marginsType);
        if (marginsType ==
            print_preview.ticket_items.MarginsType.Value.CUSTOM) {
          // If CUSTOM, set the value of the custom margins so that it won't be
          // overridden by the default value.
          this.customMargins_.updateValue(this.customMargins_.getValue());
          this.appState_.persistCustomMargins(this.customMargins_.getValue());
        }
        cr.dispatchSimpleEvent(this, PrintTicketStore.EventType.TICKET_CHANGE);
      }
    },

    /** @return {boolean} Whether all of the custom margins are valid. */
    isCustomMarginsValid: function() {
      return this.customMargins_.isValid();
    },

    /**
     * @return {!print_preview.Margins} Custom margins of the document in
     *     points.
     */
    getCustomMargins: function() {
      return this.customMargins_.getValue();
    },

    /**
     * @param {!print_preview.ticket_items.CustomMargins.Orientation}
     *     orientation Specifies the margin to get the maximum value for.
     * @return {number} Maximum value in points of the specified margin.
     */
    getCustomMarginMax: function(orientation) {
      return this.customMargins_.getMarginMax(orientation);
    },

    /**
     * Updates the custom margins of the document. Dispatches a TICKET_CHANGE
     * event if the margins have changed.
     * @param {!print_preview.Margins} margins New document page margins in
     *     points.
     */
    updateCustomMargins: function(margins) {
      if (!this.isCustomMarginsValid() ||
          !margins.equals(this.getCustomMargins())) {
        this.customMargins_.updateValue(margins);
        this.appState_.persistCustomMargins(margins);
        cr.dispatchSimpleEvent(this, PrintTicketStore.EventType.TICKET_CHANGE);
      }
    },

    /**
     * Updates a single custom margin's value in points.
     * @param {!print_preview.ticket_items.CustomMargins.Orientation}
     *     orientation Specifies the margin to update.
     * @param {number} value Updated margin in points.
     */
    updateCustomMargin: function(orientation, value) {
      if (this.customMargins_.getValue().get(orientation) != value) {
        this.customMargins_.updateMargin(orientation, value);
        this.appState_.persistCustomMargins(this.customMargins_.getValue());
        cr.dispatchSimpleEvent(this, PrintTicketStore.EventType.TICKET_CHANGE);
      }
    },

    /** @return {boolean} Whether the page range capability is available. */
    hasPageRangeCapability: function() {
      return this.pageRange_.isCapabilityAvailable();
    },

    /**
     * @return {boolean} Whether the current page range string is defines a
     *     valid page number set.
     */
    isPageRangeValid: function() {
      return this.pageRange_.isValid();
    },

    /** @return {string} String representation of the page range. */
    getPageRangeStr: function() {
      return this.pageRange_.getValue();
    },

    /**
     * @return {!Array.<object.<{from: number, to: number}>>} Page ranges
     *     represented by of the page range string.
     */
    getPageRanges: function() {
      return this.pageRange_.getPageRanges();
    },

    /**
     * @return {!Array.<object.<{from: number, to: number}>>} Page ranges
     *     represented by of the page range string and constraied by ducument
     *     page count.
     */
    getDocumentPageRanges: function() {
      return this.pageRange_.getDocumentPageRanges();
    },

    /**
     * @return {!print_preview.PageNumberSet} Page number set specified by the
     *     string representation of the page range string.
     */
    getPageNumberSet: function() {
      return this.pageRange_.getPageNumberSet();
    },

    /**
     * Updates the page range string. Dispatches a TICKET_CHANGE if the string
     * changed.
     * @param {string} pageRangeStr New page range string.
     */
    updatePageRange: function(pageRangeStr) {
      if (this.pageRange_.getValue() != pageRangeStr) {
        this.pageRange_.updateValue(pageRangeStr);
        cr.dispatchSimpleEvent(this, PrintTicketStore.EventType.TICKET_CHANGE);
      }
    },

    /**
     * @return {boolean} {@code true} if the stored print ticket is valid,
     *     {@code false} otherwise.
     */
    isTicketValid: function() {
      return this.isTicketValidForPreview() &&
          (!this.hasPageRangeCapability() || this.isPageRangeValid());
    },

    /** @return {boolean} Whether the ticket is valid for preview generation. */
    isTicketValidForPreview: function() {
      return (!this.copies.isCapabilityAvailable() || this.copies.isValid()) &&
          (!this.hasMarginsCapability() ||
              this.getMarginsType() !=
                  print_preview.ticket_items.MarginsType.Value.CUSTOM ||
              this.isCustomMarginsValid());
    },

    /**
     * Adds event listeners for the print ticket store.
     * @private
     */
    addEventListeners_: function() {
      this.tracker_.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.
              SELECTED_DESTINATION_CAPABILITIES_READY,
          this.onSelectedDestinationCapabilitiesReady_.bind(this));
      // TODO(rltoscano): Print ticket store shouldn't be re-dispatching these
      // events, the consumers of the print ticket store events should listen
      // for the events from document info instead. Will move this when
      // consumers are all migrated.
      this.tracker_.add(
          this.documentInfo_,
          print_preview.DocumentInfo.EventType.CHANGE,
          this.onDocumentInfoChange_.bind(this));
    },

    /**
     * Called when the capabilities of the selected destination are ready.
     * @private
     */
    onSelectedDestinationCapabilitiesReady_: function() {
      var caps = this.destinationStore_.selectedDestination.capabilities;
      var isFirstUpdate = this.capabilitiesHolder_.get() == null;
      this.capabilitiesHolder_.set(caps);
      if (isFirstUpdate) {
        cr.dispatchSimpleEvent(this, PrintTicketStore.EventType.INITIALIZE);
      } else {
        // Reset user selection for certain ticket items.
        this.customMargins_.updateValue(null);
        this.appState_.persistCustomMargins(null);

        if (this.marginsType_.getValue() ==
            print_preview.ticket_items.MarginsType.Value.CUSTOM) {
          this.marginsType_.updateValue(
              print_preview.ticket_items.MarginsType.Value.DEFAULT);
          this.appState_.persistMarginsType(
              print_preview.ticket_items.MarginsType.Value.DEFAULT);
        }
        cr.dispatchSimpleEvent(
            this, PrintTicketStore.EventType.CAPABILITIES_CHANGE);
      }
    },

    /**
     * Called when document data model has changed. Dispatches a print ticket
     * store event.
     * @private
     */
    onDocumentInfoChange_: function() {
      cr.dispatchSimpleEvent(this, PrintTicketStore.EventType.DOCUMENT_CHANGE);
    }
  };

  // Export
  return {
    PrintTicketStore: PrintTicketStore
  };
});

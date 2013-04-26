// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Object used to get and persist the print preview application state.
   * @constructor
   */
  function AppState() {
    /**
     * Internal representation of application state.
     * @type {Object.<string: Object>}
     * @private
     */
    this.state_ = {};
    this.state_[AppState.Field.VERSION] = AppState.VERSION_;
    this.state_[AppState.Field.IS_GCP_PROMO_DISMISSED] = true;

    /**
     * Whether the app state has been initialized. The app state will ignore all
     * writes until it has been initialized.
     * @type {boolean}
     * @private
     */
    this.isInitialized_ = false;
  };

  /**
   * Enumeration of field names for serialized app state.
   * @enum {string}
   */
  AppState.Field = {
    VERSION: 'version',
    SELECTED_DESTINATION_ID: 'selectedDestinationId',
    SELECTED_DESTINATION_ORIGIN: 'selectedDestinationOrigin',
    IS_SELECTED_DESTINATION_LOCAL: 'isSelectedDestinationLocal',  // Deprecated
    IS_GCP_PROMO_DISMISSED: 'isGcpPromoDismissed',
    MARGINS_TYPE: 'marginsType',
    CUSTOM_MARGINS: 'customMargins',
    IS_COLOR_ENABLED: 'isColorEnabled',
    IS_DUPLEX_ENABLED: 'isDuplexEnabled',
    IS_HEADER_FOOTER_ENABLED: 'isHeaderFooterEnabled',
    IS_LANDSCAPE_ENABLED: 'isLandscapeEnabled',
    IS_COLLATE_ENABLED: 'isCollateEnabled',
    IS_CSS_BACKGROUND_ENABLED: 'isCssBackgroundEnabled'
  };

  /**
   * Current version of the app state. This value helps to understand how to
   * parse earlier versions of the app state.
   * @type {number}
   * @const
   * @private
   */
  AppState.VERSION_ = 2;

  /**
   * Name of C++ layer function to persist app state.
   * @type {string}
   * @const
   * @private
   */
  AppState.NATIVE_FUNCTION_NAME_ = 'saveAppState';

  AppState.prototype = {
    /** @return {?string} ID of the selected destination. */
    get selectedDestinationId() {
      return this.state_[AppState.Field.SELECTED_DESTINATION_ID];
    },

    /** @return {?string} Origin of the selected destination. */
    get selectedDestinationOrigin() {
      return this.state_[AppState.Field.SELECTED_DESTINATION_ORIGIN];
    },

    /** @return {boolean} Whether the GCP promotion has been dismissed. */
    get isGcpPromoDismissed() {
      return this.state_[AppState.Field.IS_GCP_PROMO_DISMISSED];
    },

    /** @return {print_preview.ticket_items.MarginsType.Value} Margins type. */
    get marginsType() {
      return this.state_[AppState.Field.MARGINS_TYPE];
    },

    /** @return {print_preview.Margins} Custom margins. */
    get customMargins() {
      return this.state_[AppState.Field.CUSTOM_MARGINS] ?
          print_preview.Margins.parse(
              this.state_[AppState.Field.CUSTOM_MARGINS]) :
          null;
    },

    /** @return {?boolean} Whether the header-footer option is enabled. */
    get isHeaderFooterEnabled() {
      return this.state_[AppState.Field.IS_HEADER_FOOTER_ENABLED];
    },

    /** @return {?boolean} Whether landscape page orientation is selected. */
    get isLandscapeEnabled() {
      return this.state_[AppState.Field.IS_LANDSCAPE_ENABLED];
    },

    /**
     * @param {!print_preview.AppState.Field} field App state field to check if
     *     set.
     * @return {boolean} Whether a field has been set in the app state.
     */
    hasField: function(field) {
      return this.state_.hasOwnProperty(field);
    },

    /**
     * @param {!print_preview.AppState.Field} field App state field to get.
     * @return {Object} Value of the app state field.
     */
    getField: function(field) {
      return this.state_[field];
    },

    /**
     * Initializes the app state from a serialized string returned by the native
     * layer.
     * @param {?string} serializedAppStateStr Serialized string representation
     *     of the app state.
     */
    init: function(serializedAppStateStr) {
      if (serializedAppStateStr) {
        var state = JSON.parse(serializedAppStateStr);
        if (state[AppState.Field.VERSION] == AppState.VERSION_) {
          if (state.hasOwnProperty(
              AppState.Field.IS_SELECTED_DESTINATION_LOCAL)) {
            state[AppState.Field.SELECTED_DESTINATION_ORIGIN] =
                state[AppState.Field.IS_SELECTED_DESTINATION_LOCAL] ?
                print_preview.Destination.Origin.LOCAL :
                print_preview.Destination.Origin.COOKIES;
            delete state[AppState.Field.IS_SELECTED_DESTINATION_LOCAL];
          }
          this.state_ = state;
        }
      } else {
        // Set some state defaults.
        this.state_[AppState.Field.IS_GCP_PROMO_DISMISSED] = false;
      }
      this.isInitialized_ = true;
    },

    /**
     * Persists the given value for the given field.
     * @param {!print_preview.AppState.Field} field Field to persist.
     * @param {Object} value Value of field to persist.
     */
    persistField: function(field, value) {
      this.state_[field] = value;
      this.persist_();
    },

    /**
     * Persists the selected destination.
     * @param {!print_preview.Destination} dest Destination to persist.
     */
    persistSelectedDestination: function(dest) {
      this.state_[AppState.Field.SELECTED_DESTINATION_ID] = dest.id;
      this.state_[AppState.Field.SELECTED_DESTINATION_ORIGIN] = dest.origin;
      this.persist_();
    },

   /**
    * Persists whether the GCP promotion has been dismissed.
    * @param {boolean} isGcpPromoDismissed Whether the GCP promotion has been
    *     dismissed.
    */
   persistIsGcpPromoDismissed: function(isGcpPromoDismissed) {
     this.state_[AppState.Field.IS_GCP_PROMO_DISMISSED] = isGcpPromoDismissed;
     this.persist_();
   },

    /**
     * Persists the margins type.
     * @param {print_preview.ticket_items.MarginsType.Value} marginsType Margins
     *     type.
     */
    persistMarginsType: function(marginsType) {
      this.state_[AppState.Field.MARGINS_TYPE] = marginsType;
      this.persist_();
    },

    /**
     * Persists custom margins.
     * @param {print_preview.Margins} customMargins Custom margins.
     */
    persistCustomMargins: function(customMargins) {
      this.state_[AppState.Field.CUSTOM_MARGINS] =
          customMargins ? customMargins.serialize() : null;
      this.persist_();
    },

    /**
     * Persists whether header-footer is enabled.
     * @param {?boolean} Whether header-footer is enabled.
     */
    persistIsHeaderFooterEnabled: function(isHeaderFooterEnabled) {
      this.state_[AppState.Field.IS_HEADER_FOOTER_ENABLED] =
          isHeaderFooterEnabled;
      this.persist_();
    },

    /**
     * Persists whether landscape printing is enabled.
     * @param {?boolean} isLandscapeEnabled landscape printing is enabled.
     */
    persistIsLandscapeEnabled: function(isLandscapeEnabled) {
      this.state_[AppState.Field.IS_LANDSCAPE_ENABLED] = isLandscapeEnabled;
      this.persist_();
    },

    /**
     * Calls into the native layer to persist the application state.
     * @private
     */
    persist_: function() {
      if (this.isInitialized_) {
        chrome.send(AppState.NATIVE_FUNCTION_NAME_,
                    [JSON.stringify(this.state_)]);
      }
    }
  };

  return {
    AppState: AppState
  };
});

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
     * ID of the selected destination.
     * @type {?string}
     * @private
     */
    this.selectedDestinationId_ = null;

    /**
     * Whether the selected destination is a local destination.
     * @type {?boolean}
     * @private
     */
    this.isSelectedDestinationLocal_ = null;

    /**
     * Whether the GCP promotion has been dismissed.
     * @type {boolean}
     * @private
     */
    this.isGcpPromoDismissed_ = true;

    /**
     * Margins type.
     * @type {print_preview.ticket_items.MarginsType.Value}
     * @private
     */
    this.marginsType_ = null;

    /**
     * Custom margins.
     * @type {print_preview.Margins}
     * @private
     */
    this.customMargins_ = null;

    /**
     * Whether the color option is enabled.
     * @type {?boolean}
     * @private
     */
    this.isColorEnabled_ = null;

    /**
     * Whether duplex printing is enabled.
     * @type {?boolean}
     * @private
     */
    this.isDuplexEnabled_ = null;

    /**
     * Whether the header-footer option is enabled.
     * @type {?boolean}
     * @private
     */
    this.isHeaderFooterEnabled_ = null;

    /**
     * Whether landscape page orientation is selected.
     * @type {?boolean}
     * @private
     */
    this.isLandscapeEnabled_ = null;

    /**
     * Whether printing collation is enabled.
     * @type {?boolean}
     * @private
     */
    this.isCollateEnabled_ = null;

    /**
     * Whether printing CSS backgrounds is enabled.
     * @type {?boolean}
     * @private
     */
    this.isCssBackgroundEnabled_ = null;
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
   * Enumeration of field names for serialized app state.
   * @enum {string}
   * @private
   */
  AppState.Field_ = {
    VERSION: 'version',
    SELECTED_DESTINATION_ID: 'selectedDestinationId',
    IS_SELECTED_DESTINATION_LOCAL: 'isSelectedDestinationLocal',
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
   * Name of C++ layer function to persist app state.
   * @type {string}
   * @const
   * @private
   */
  AppState.NATIVE_FUNCTION_NAME_ = 'saveAppState';

  AppState.prototype = {
    /** @return {?string} ID of the selected destination. */
    get selectedDestinationId() {
      return this.selectedDestinationId_;
    },

    /** @return {?boolean} Whether the selected destination is local. */
    get isSelectedDestinationLocal() {
      return this.isSelectedDestinationLocal_;
    },

    /** @return {boolean} Whether the GCP promotion has been dismissed. */
    get isGcpPromoDismissed() {
      return this.isGcpPromoDismissed_;
    },

    /** @return {print_preview.ticket_items.MarginsType.Value} Margins type. */
    get marginsType() {
      return this.marginsType_;
    },

    /** @return {print_preview.Margins} Custom margins. */
    get customMargins() {
      return this.customMargins_;
    },

    /** @return {?boolean} Whether the color option is enabled. */
    get isColorEnabled() {
      return this.isColorEnabled_;
    },

    /** @return {?boolean} Whether duplex printing is enabled. */
    get isDuplexEnabled() {
      return this.isDuplexEnabled_;
    },

    /** @return {?boolean} Whether the header-footer option is enabled. */
    get isHeaderFooterEnabled() {
      return this.isHeaderFooterEnabled_;
    },

    /** @return {?boolean} Whether landscape page orientation is selected. */
    get isLandscapeEnabled() {
      return this.isLandscapeEnabled_;
    },

    /** @return {?boolean} Whether printing collation is enabled. */
    get isCollateEnabled() {
      return this.isCollateEnabled_;
    },

    /** @return {?boolean} Whether printing CSS backgrounds is enabled. */
    get isCssBackgroundEnabled() {
      return this.isCssBackgroundEnabled_;
    },

    /**
     * Initializes the app state from a serialized string returned by the native
     * layer.
     * @param {?string} serializedAppStateStr Serialized string representation
     *     of the app state.
     */
    init: function(serializedAppStateStr) {
      if (!serializedAppStateStr) {
        // Set some state defaults.
        this.isGcpPromoDismissed_ = false;
        return;
      }

      var state = JSON.parse(serializedAppStateStr);
      if (state[AppState.Field_.VERSION] == 2) {
        this.selectedDestinationId_ =
            state[AppState.Field_.SELECTED_DESTINATION_ID] || null;
        if (state.hasOwnProperty(
                AppState.Field_.IS_SELECTED_DESTINATION_LOCAL)) {
          this.isSelectedDestinationLocal_ =
              state[AppState.Field_.IS_SELECTED_DESTINATION_LOCAL];
        }
        this.isGcpPromoDismissed_ =
            state[AppState.Field_.IS_GCP_PROMO_DISMISSED] || false;
        if (state.hasOwnProperty(AppState.Field_.MARGINS_TYPE)) {
          this.marginsType_ = state[AppState.Field_.MARGINS_TYPE];
        }
        if (state[AppState.Field_.CUSTOM_MARGINS]) {
          this.customMargins_ = print_preview.Margins.parse(
              state[AppState.Field_.CUSTOM_MARGINS]);
        }
        if (state.hasOwnProperty(AppState.Field_.IS_COLOR_ENABLED)) {
          this.isColorEnabled_ = state[AppState.Field_.IS_COLOR_ENABLED];
        }
        if (state.hasOwnProperty(AppState.Field_.IS_DUPLEX_ENABLED)) {
          this.isDuplexEnabled_ = state[AppState.Field_.IS_DUPLEX_ENABLED];
        }
        if (state.hasOwnProperty(AppState.Field_.IS_HEADER_FOOTER_ENABLED)) {
          this.isHeaderFooterEnabled_ =
              state[AppState.Field_.IS_HEADER_FOOTER_ENABLED];
        }
        if (state.hasOwnProperty(AppState.Field_.IS_LANDSCAPE_ENABLED)) {
          this.isLandscapeEnabled_ =
              state[AppState.Field_.IS_LANDSCAPE_ENABLED];
        }
        if (state.hasOwnProperty(AppState.Field_.IS_COLLATE_ENABLED)) {
          this.isCollateEnabled_ = state[AppState.Field_.IS_COLLATE_ENABLED];
        }
        if (state.hasOwnProperty(AppState.Field_.IS_CSS_BACKGROUND_ENABLED)) {
          this.isCssBackgroundEnabled_ =
              state[AppState.Field_.IS_CSS_BACKGROUND_ENABLED];
        }
      }
    },

    /**
     * Persists the selected destination.
     * @param {!print_preview.Destination} dest Destination to persist.
     */
    persistSelectedDestination: function(dest) {
      this.selectedDestinationId_ = dest.id;
      this.isSelectedDestinationLocal_ = dest.isLocal;
      this.persist_();
    },

   /**
    * Persists whether the GCP promotion has been dismissed.
    * @param {boolean} isGcpPromoDismissed Whether the GCP promotion has been
    *     dismissed.
    */
   persistIsGcpPromoDismissed: function(isGcpPromoDismissed) {
     this.isGcpPromoDismissed_ = isGcpPromoDismissed;
     this.persist_();
   },

    /**
     * Persists the margins type.
     * @param {print_preview.ticket_items.MarginsType.Value} marginsType Margins
     *     type.
     */
    persistMarginsType: function(marginsType) {
      this.marginsType_ = marginsType;
      this.persist_();
    },

    /**
     * Persists custom margins.
     * @param {print_preview.Margins} customMargins Custom margins.
     */
    persistCustomMargins: function(customMargins) {
      this.customMargins_ = customMargins;
      this.persist_();
    },

    /**
     * Persists whether color printing is enabled.
     * @param {?boolean} isColorEnabled Whether color printing is enabled.
     */
    persistIsColorEnabled: function(isColorEnabled) {
      this.isColorEnabled_ = isColorEnabled;
      this.persist_();
    },

    /**
     * Persists whether duplexing is enabled.
     * @param {?boolean} isDuplexEnabled Whether duplex printing is enabled.
     */
    persistIsDuplexEnabled: function(isDuplexEnabled) {
      this.isDuplexEnabled_ = isDuplexEnabled;
      this.persist_();
    },

    /**
     * Persists whether header-footer is enabled.
     * @param {?boolean} Whether header-footer is enabled.
     */
    persistIsHeaderFooterEnabled: function(isHeaderFooterEnabled) {
      this.isHeaderFooterEnabled_ = isHeaderFooterEnabled;
      this.persist_();
    },

    /**
     * Persists whether landscape printing is enabled.
     * @param {?boolean} isLandscapeEnabled landscape printing is enabled.
     */
    persistIsLandscapeEnabled: function(isLandscapeEnabled) {
      this.isLandscapeEnabled_ = isLandscapeEnabled;
      this.persist_();
    },

    /**
     * Persists whether printing collation is enabled.
     * @param {?boolean} isCollateEnabled Whether printing collation is enabled.
     */
    persistIsCollateEnabled: function(isCollateEnabled) {
      this.isCollateEnabled_ = isCollateEnabled;
      this.persist_();
    },

    /**
     * Persists whether printing CSS backgrounds is enabled.
     * @param {?boolean} isCssBackgroundEnabled Whether printing CSS
     *     backgrounds is enabled.
     */
    persistIsCssBackgroundEnabled: function(isCssBackgroundEnabled) {
      this.isCssBackgroundEnabled_ = isCssBackgroundEnabled;
      this.persist_();
    },

    /**
     * Calls into the native layer to persist the application state.
     * @private
     */
    persist_: function() {
      var obj = {};
      obj[AppState.Field_.VERSION] = AppState.VERSION_;
      obj[AppState.Field_.SELECTED_DESTINATION_ID] =
          this.selectedDestinationId_;
      obj[AppState.Field_.IS_SELECTED_DESTINATION_LOCAL] =
          this.isSelectedDestinationLocal_;
      obj[AppState.Field_.IS_GCP_PROMO_DISMISSED] = this.isGcpPromoDismissed_;
      obj[AppState.Field_.MARGINS_TYPE] = this.marginsType_;
      if (this.customMargins_) {
        obj[AppState.Field_.CUSTOM_MARGINS] = this.customMargins_.serialize();
      }
      obj[AppState.Field_.IS_COLOR_ENABLED] = this.isColorEnabled_;
      obj[AppState.Field_.IS_DUPLEX_ENABLED] = this.isDuplexEnabled_;
      obj[AppState.Field_.IS_HEADER_FOOTER_ENABLED] =
          this.isHeaderFooterEnabled_;
      obj[AppState.Field_.IS_LANDSCAPE_ENABLED] = this.isLandscapeEnabled_;
      obj[AppState.Field_.IS_COLLATE_ENABLED] = this.isCollateEnabled_;
      obj[AppState.Field_.IS_CSS_BACKGROUND_ENABLED] =
          this.isCssBackgroundEnabled_;
      chrome.send(AppState.NATIVE_FUNCTION_NAME_, [JSON.stringify(obj)]);
    }
  };

  return {
    AppState: AppState
  };
});

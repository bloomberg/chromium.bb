// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview');

/**
 * Enumeration of field names for serialized app state.
 * @enum {string}
 */
print_preview.AppStateField = {
  VERSION: 'version',
  RECENT_DESTINATIONS: 'recentDestinations',
  DPI: 'dpi',
  MEDIA_SIZE: 'mediaSize',
  MARGINS_TYPE: 'marginsType',
  CUSTOM_MARGINS: 'customMargins',
  IS_COLOR_ENABLED: 'isColorEnabled',
  IS_DUPLEX_ENABLED: 'isDuplexEnabled',
  IS_HEADER_FOOTER_ENABLED: 'isHeaderFooterEnabled',
  IS_LANDSCAPE_ENABLED: 'isLandscapeEnabled',
  IS_COLLATE_ENABLED: 'isCollateEnabled',
  IS_FIT_TO_PAGE_ENABLED: 'isFitToPageEnabled',
  IS_CSS_BACKGROUND_ENABLED: 'isCssBackgroundEnabled',
  SCALING: 'scaling',
  VENDOR_OPTIONS: 'vendorOptions'
};

/**
 * Object used to represent a recent destination in the app state.
 * @constructor
 * @struct
 */
function RecentDestination(destination) {
  /**
   * ID of the RecentDestination.
   * @type {string}
   */
  this.id = destination.id;

  /**
   * Origin of the RecentDestination.
   * @type {!print_preview.DestinationOrigin}
   */
  this.origin = destination.origin;

  /**
   * Account the RecentDestination is registered for.
   * @type {string}
   */
  this.account = destination.account || '';

  /**
   * CDD of the RecentDestination.
   * @type {print_preview.Cdd}
   */
  this.capabilities = destination.capabilities;

  /**
   * Name of the RecentDestination.
   * @type {string}
   */
  this.name = destination.name || '';

  /**
   * Extension ID associated with the RecentDestination.
   * @type {string}
   */
  this.extensionId = destination.extension_id || '';

  /**
   * Extension name associated with the RecentDestination.
   * @type {string}
   */
  this.extensionName = destination.extension_name || '';
}

cr.define('print_preview', function() {
  'use strict';

  /**
   * Object used to get and persist the print preview application state.
   * @constructor
   */
  function AppState() {
    /**
     * Internal representation of application state.
     * @private {Object}
     */
    this.state_ = {};
    this.state_[print_preview.AppStateField.VERSION] = AppState.VERSION_;
    this.state_[print_preview.AppStateField.RECENT_DESTINATIONS] = [];

    /**
     * Whether the app state has been initialized. The app state will ignore all
     * writes until it has been initialized.
     * @private {boolean}
     */
    this.isInitialized_ = false;

    /**
     * Native Layer object to use for sending app state to C++ handler.
     * @private {!print_preview.NativeLayer}
     */
    this.nativeLayer_ = print_preview.NativeLayer.getInstance();
  }

  /**
   * Number of recent print destinations to store across browser sessions.
   * @const {number}
   */
  AppState.NUM_DESTINATIONS_ = 3;

  /**
   * Current version of the app state. This value helps to understand how to
   * parse earlier versions of the app state.
   * @type {number}
   * @const
   * @private
   */
  AppState.VERSION_ = 2;

  AppState.prototype = {
    /**
     * @return {?RecentDestination} The most recent destination,
     *     which is currently the selected destination.
     */
    get selectedDestination() {
      return (this.state_[print_preview.AppStateField.RECENT_DESTINATIONS]
                  .length > 0) ?
          this.state_[print_preview.AppStateField.RECENT_DESTINATIONS][0] :
          null;
    },

    /**
     * @return {boolean} Whether the selected destination is valid.
     */
    isSelectedDestinationValid: function() {
      return !!this.selectedDestination && !!this.selectedDestination.id &&
          !!this.selectedDestination.origin;
    },

    /**
     * @return {?Array<!RecentDestination>} The
     *     AppState.NUM_DESTINATIONS_ most recent destinations.
     */
    get recentDestinations() {
      return this.state_[print_preview.AppStateField.RECENT_DESTINATIONS];
    },

    /**
     * @param {!print_preview.AppStateField} field App state field to check if
     *     set.
     * @return {boolean} Whether a field has been set in the app state.
     */
    hasField: function(field) {
      return this.state_.hasOwnProperty(field);
    },

    /**
     * @param {!print_preview.AppStateField} field App state field to get.
     * @return {?} Value of the app state field.
     */
    getField: function(field) {
      if (field == print_preview.AppStateField.CUSTOM_MARGINS) {
        return this.state_[field] ?
            print_preview.Margins.parse(this.state_[field]) :
            null;
      }
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
        try {
          var state = JSON.parse(serializedAppStateStr);
          if (state[print_preview.AppStateField.VERSION] == AppState.VERSION_) {
            this.state_ = /** @type {Object} */ (state);
          }
        } catch (e) {
          console.error('Unable to parse state: ' + e);
          // Proceed with default state.
        }
      } else {
        // Set some state defaults.
        this.state_[print_preview.AppStateField.RECENT_DESTINATIONS] = [];
      }
      if (!this.state_[print_preview.AppStateField.RECENT_DESTINATIONS]) {
        this.state_[print_preview.AppStateField.RECENT_DESTINATIONS] = [];
      } else if (!(this.state_[print_preview.AppStateField
                                   .RECENT_DESTINATIONS] instanceof
                   Array)) {
        var tmp = this.state_[print_preview.AppStateField.RECENT_DESTINATIONS];
        this.state_[print_preview.AppStateField.RECENT_DESTINATIONS] = [tmp];
      } else if (
          !this.state_[print_preview.AppStateField.RECENT_DESTINATIONS][0] ||
          !this.state_[print_preview.AppStateField.RECENT_DESTINATIONS][0].id) {
        // read in incorrectly
        this.state_[print_preview.AppStateField.RECENT_DESTINATIONS] = [];
      } else if (
          this.state_[print_preview.AppStateField.RECENT_DESTINATIONS].length >
          AppState.NUM_DESTINATIONS_) {
        this.state_[print_preview.AppStateField.RECENT_DESTINATIONS].length =
            AppState.NUM_DESTINATIONS_;
      }
    },

    /**
     * Sets to initialized state. Now object will accept persist requests.
     */
    setInitialized: function() {
      this.isInitialized_ = true;
    },

    /**
     * Persists the given value for the given field.
     * @param {!print_preview.AppStateField} field Field to persist.
     * @param {?} value Value of field to persist.
     */
    persistField: function(field, value) {
      if (!this.isInitialized_)
        return;
      if (field == print_preview.AppStateField.CUSTOM_MARGINS) {
        this.state_[field] = value ? value.serialize() : null;
      } else {
        this.state_[field] = value;
      }
      this.persist_();
    },

    /**
     * Persists the selected destination.
     * @param {!print_preview.Destination} dest Destination to persist.
     */
    persistSelectedDestination: function(dest) {
      if (!this.isInitialized_)
        return;

      // Determine if this destination is already in the recent destinations,
      // and where in the array it is located.
      var newDestination = new RecentDestination(dest);
      var indexFound =
          this.state_[print_preview.AppStateField.RECENT_DESTINATIONS]
              .findIndex(function(recent) {
                return (
                    newDestination.id == recent.id &&
                    newDestination.origin == recent.origin);
              });

      // No change
      if (indexFound == 0) {
        this.persist_();
        return;
      }

      // Shift the array so that the nth most recent destination is located at
      // index n.
      if (indexFound == -1 &&
          this.state_[print_preview.AppStateField.RECENT_DESTINATIONS].length ==
              AppState.NUM_DESTINATIONS_) {
        indexFound = AppState.NUM_DESTINATIONS_ - 1;
      }
      if (indexFound != -1)
        this.state_[print_preview.AppStateField.RECENT_DESTINATIONS].splice(
            indexFound, 1);

      // Add the most recent destination
      this.state_[print_preview.AppStateField.RECENT_DESTINATIONS].splice(
          0, 0, newDestination);

      this.persist_();
    },

    /**
     * Calls into the native layer to persist the application state.
     * @private
     */
    persist_: function() {
      this.nativeLayer_.saveAppState(JSON.stringify(this.state_));
    }
  };

  return {AppState: AppState};
});

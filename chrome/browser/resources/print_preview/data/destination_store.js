// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * A data store that stores destinations and dispatches events when the data
   * store changes.
   * @param {!print_preview.NativeLayer} nativeLayer Used to fetch local print
   *     destinations.
   * @constructor
   * @extends {cr.EventTarget}
   */
  function DestinationStore(nativeLayer) {
    cr.EventTarget.call(this);

    /**
     * Used to fetch local print destinations.
     * @type {!print_preview.NativeLayer}
     * @private
     */
    this.nativeLayer_ = nativeLayer;

    /**
     * Internal backing store for the data store.
     * @type {!Array.<!print_preview.Destination>}
     * @private
     */
    this.destinations_ = [];

    /**
     * Cache used for constant lookup of printers.
     * @type {object.<string, !print_preview.Destination>}
     * @private
     */
    this.destinationMap_ = {};

    /**
     * Currently selected destination.
     * @type {print_preview.Destination}
     * @private
     */
    this.selectedDestination_ = null;

    /**
     * Initial destination ID used to auto-select the first inserted destination
     * that matches. If {@code null}, the first destination inserted into the
     * store will be selected.
     * @type {?string}
     * @private
     */
    this.initialDestinationId_ = null;

    /**
     * Whether the destination store will auto select the destination that
     * matches the initial destination.
     * @type {boolean}
     * @private
     */
    this.isInAutoSelectMode_ = false;

    /**
     * Event tracker used to track event listeners of the destination store.
     * @type {!EventTracker}
     * @private
     */
    this.tracker_ = new EventTracker();

    /**
     * Used to fetch cloud-based print destinations.
     * @type {print_preview.CloudPrintInterface}
     * @private
     */
    this.cloudPrintInterface_ = null;

    /**
     * Whether the destination store has already loaded or is loading all cloud
     * destinations.
     * @type {boolean}
     * @private
     */
    this.hasLoadedAllCloudDestinations_ = false;

    /**
     * ID of a timeout after the initial destination ID is set. If no inserted
     * destination matches the initial destination ID after the specified
     * timeout, the first destination in the store will be automatically
     * selected.
     * @type {?number}
     * @private
     */
    this.autoSelectTimeout_ = null;

    this.addEventListeners_();
    this.reset_();
  };

  /**
   * Event types dispatched by the data store.
   * @enum {string}
   */
  DestinationStore.EventType = {
    DESTINATIONS_INSERTED:
        'print_preview.DestinationStore.DESTINATIONS_INSERTED',
    DESTINATION_SELECT: 'print_preview.DestinationStore.DESTINATION_SELECT',
    SELECTED_DESTINATION_CAPABILITIES_READY:
        'print_preview.DestinationStore.SELECTED_DESTINATION_CAPABILITIES_READY'
  };

  /**
   * Delay in milliseconds before the destination store ignores the initial
   * destination ID and just selects any printer (since the initial destination
   * was not found).
   * @type {number}
   * @const
   * @private
   */
  DestinationStore.AUTO_SELECT_TIMEOUT_ = 2000;

  /**
   * Creates a local PDF print destination.
   * @return {!print_preview.Destination} Created print destination.
   * @private
   */
  DestinationStore.createLocalPdfPrintDestination_ = function() {
    var dest = new print_preview.Destination(
        print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
        print_preview.Destination.Type.LOCAL,
        localStrings.getString('printToPDF'),
        false /*isRecent*/,
        print_preview.Destination.ConnectionStatus.ONLINE);
    dest.capabilities = new print_preview.ChromiumCapabilities(
        false /*hasCopiesCapability*/,
        '1' /*defaultCopiesStr*/,
        false /*hasCollateCapability*/,
        false /*defaultIsCollateEnabled*/,
        false /*hasDuplexCapability*/,
        false /*defaultIsDuplexEnabled*/,
        true /*hasOrientationCapability*/,
        false /*defaultIsLandscapeEnabled*/,
        true /*hasColorCapability*/,
        true /*defaultIsColorEnabled*/);
    return dest;
  };

  DestinationStore.prototype = {
    __proto__: cr.EventTarget.prototype,

    /**
     * @return {!Array.<!print_preview.Destination>} List of destinations in
     *     the store.
     */
    get destinations() {
      return this.destinations_.slice(0);
    },

    /**
     * @return {print_preview.Destination} The currently selected destination or
     *     {@code null} if none is selected.
     */
    get selectedDestination() {
      return this.selectedDestination_;
    },

    /**
     * Sets the initially selected destination. If any inserted destinations
     * match this ID, that destination will be automatically selected. This
     * occurs only once for every time this setter is called or if the store is
     * cleared.
     * @param {?string} ID of the destination that should be selected
     *     automatically when added to the store or {@code null} if the first
     *     destination that is inserted should be selected.
     */
    setInitialDestinationId: function(initialDestinationId) {
      this.initialDestinationId_ = initialDestinationId;
      this.isInAutoSelectMode_ = true;
      if (this.initialDestinationId_ == null) {
        assert(this.destinations_.length > 0,
               'No destinations available to select');
        this.selectDestination(this.destinations_[0]);
      } else {
        var candidate = this.destinationMap_[this.initialDestinationId_];
        if (candidate != null) {
          this.selectDestination(candidate);
        } else {
          // Try and fetch the destination
          // TODO(rltoscano): Since we don't know if the initialDestinationId is
          // a local printer or a cloud printer, we are going to assume based on
          // platform. The C++ layer should be modified to return more
          // information about the initially selected printer so this assumption
          // does not have to be made. See http://crbug.com/132831.
          if (!cr.isChromeOS) {
            this.nativeLayer_.startGetLocalDestinationCapabilities(
                initialDestinationId);
          }
        }
      }
    },

    /**
     * Sets the destination store's Google Cloud Print interface.
     * @param {!print_preview.CloudPrintInterface} cloudPrintInterface Interface
     *     to set.
     */
    setCloudPrintInterface: function(cloudPrintInterface) {
      this.cloudPrintInterface_ = cloudPrintInterface;
      this.tracker_.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterface.EventType.SEARCH_DONE,
          this.onCloudPrintSearchDone_.bind(this));
      this.tracker_.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterface.EventType.PRINTER_DONE,
          this.onCloudPrintPrinterDone_.bind(this));
    },

    /** @param {!print_preview.Destination} Destination to select. */
    selectDestination: function(destination) {
      this.selectedDestination_ = destination;
      this.selectedDestination_.isRecent = true;
      this.isInAutoSelectMode_ = false;
      if (this.autoSelectTimeout_ != null) {
        clearTimeout(this.autoSelectTimeout_);
        this.autoSelectTimeout_ = null;
      }
      if (destination.id == print_preview.Destination.GooglePromotedId.FEDEX &&
          !destination.isTosAccepted) {
        assert(this.cloudPrintInterface_ != null,
               'Selected FedEx Office destination, but Google Cloud Print is ' +
               'not enabled');
        destination.isTosAccepted = true;
        this.cloudPrintInterface_.acceptPrinterTos(destination.id, true);
      }
      cr.dispatchSimpleEvent(
          this, DestinationStore.EventType.DESTINATION_SELECT);
      if (destination.capabilities == null) {
         if (destination.isLocal) {
          this.nativeLayer_.startGetLocalDestinationCapabilities(
              destination.id);
        } else {
          assert(this.cloudPrintInterface_ != null,
                 'Selected destination is a cloud destination, but Google ' +
                     'Cloud Print is not enabled');
          this.cloudPrintInterface_.printer(destination.id);
        }
      } else {
        cr.dispatchSimpleEvent(
            this,
            DestinationStore.EventType.SELECTED_DESTINATION_CAPABILITIES_READY);
      }
    },

    /**
     * Inserts a print destination to the data store and dispatches a
     * DESTINATIONS_INSERTED event. If the destination matches the initial
     * destination ID, then the destination will be automatically selected.
     * @param {!print_preview.Destination} destination Print destination to
     *     insert.
     */
    insertDestination: function(destination) {
      if (this.insertDestination_(destination)) {
        cr.dispatchSimpleEvent(
            this, DestinationStore.EventType.DESTINATIONS_INSERTED);
        if (this.isInAutoSelectMode_ &&
            (this.initialDestinationId_ == null ||
                destination.id == this.initialDestinationId_)) {
          this.selectDestination(destination);
        }
      }
    },

    /**
     * Inserts multiple print destinations to the data store and dispatches one
     * DESTINATIONS_INSERTED event. If any of the destinations match the initial
     * destination ID, then that destination will be automatically selected.
     * @param {!Array.<print_preview.Destination>} destinations Print
     *     destinations to insert.
     */
    insertDestinations: function(destinations) {
      var insertedDestination = false;
      var destinationToAutoSelect = null;
      destinations.forEach(function(dest) {
        if (this.insertDestination_(dest)) {
          insertedDestination = true;
          if (this.isInAutoSelectMode_ &&
              destinationToAutoSelect == null &&
              (this.initialDestinationId_ == null ||
                  dest.id == this.initialDestinationId_)) {
            destinationToAutoSelect = dest;
          }
        }
      }, this);
      if (insertedDestination) {
        cr.dispatchSimpleEvent(
            this, DestinationStore.EventType.DESTINATIONS_INSERTED);
      }
      if (destinationToAutoSelect != null) {
        this.selectDestination(destinationToAutoSelect);
      }
    },

    /**
     * Updates an existing print destination with capabilities information. If
     * the destination doesn't already exist, it will be added.
     * @param {!print_preview.Destination} destination Destination to update.
     * @return {!print_preview.Destination} The existing destination that was
     *     updated.
     */
    updateDestination: function(destination) {
      var existingDestination = this.destinationMap_[destination.id];
      if (existingDestination != null) {
        existingDestination.capabilities = destination.capabilities;
        return existingDestination;
      } else {
        this.insertDestination(destination);
      }
    },

    /** Initiates loading of local print destinations. */
    startLoadLocalDestinations: function() {
      this.nativeLayer_.startGetLocalDestinations();
    },

    /** Initiates loading of recent cloud destinations. */
    startLoadRecentCloudDestinations: function() {
      if (this.cloudPrintInterface_ != null) {
        this.cloudPrintInterface_.search(true /*isRecent*/);
      }
    },

    /** Initiates loading of all cloud destinations. */
    startLoadAllCloudDestinations: function() {
      if (this.cloudPrintInterface_ != null &&
          !this.hasLoadedAllCloudDestinations_) {
        this.cloudPrintInterface_.search(false /*isRecent*/);
        this.hasLoadedAllCloudDestinations_ = true;
      }
    },

    /**
     * Inserts a destination into the store without dispatching any events.
     * @return {boolean} Whether the inserted destination was not already in the
     *     store.
     * @private
     */
    insertDestination_: function(destination) {
      var existingDestination = this.destinationMap_[destination.id];
      if (existingDestination == null) {
        this.destinations_.push(destination);
        this.destinationMap_[destination.id] = destination;
        return true;
      } else if (existingDestination.connectionStatus ==
                     print_preview.Destination.ConnectionStatus.UNKNOWN &&
                 destination.connectionStatus !=
                     print_preview.Destination.ConnectionStatus.UNKNOWN) {
        existingDestination.connectionStatus = destination.connectionStatus;
        return true;
      } else {
        return false;
      }
    },

    /**
     * Binds handlers to events.
     * @private
     */
    addEventListeners_: function() {
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET,
          this.onLocalDestinationsSet_.bind(this));
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.CAPABILITIES_SET,
          this.onLocalDestinationCapabilitiesSet_.bind(this));
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.DESTINATIONS_RELOAD,
          this.onDestinationsReload_.bind(this));
    },

    /**
     * Resets the state of the destination store to its initial state.
     * @private
     */
    reset_: function() {
      this.destinations_ = [];
      this.destinationMap_ = {};
      this.selectedDestination_ = null;
      this.hasLoadedAllCloudDestinations_ = false;
      this.insertDestination(
          DestinationStore.createLocalPdfPrintDestination_());
      this.autoSelectTimeout_ = setTimeout(
          this.onAutoSelectTimeoutExpired_.bind(this),
          DestinationStore.AUTO_SELECT_TIMEOUT_);
    },

    /**
     * Called when the local destinations have been got from the native layer.
     * @param {cr.Event} Contains the local destinations.
     * @private
     */
    onLocalDestinationsSet_: function(event) {
      var localDestinations = event.destinationInfos.map(function(destInfo) {
        return print_preview.LocalDestinationParser.parse(destInfo);
      });
      this.insertDestinations(localDestinations);
    },

    /**
     * Called when the native layer retrieves the capabilities for the selected
     * local destination. Updates the destination with new capabilities if the
     * destination already exists, otherwise it creates a new destination and
     * then updates its capabilities.
     * @param {cr.Event} event Contains the capabilities of the local print
     *     destination.
     * @private
     */
    onLocalDestinationCapabilitiesSet_: function(event) {
      var destinationId = event.settingsInfo['printerId'];
      var destination = this.destinationMap_[destinationId];
      var capabilities = print_preview.LocalCapabilitiesParser.parse(
            event.settingsInfo);
      if (destination) {
        destination.capabilities = capabilities;
        if (this.selectedDestination_ &&
            this.selectedDestination_.id == destinationId) {
          cr.dispatchSimpleEvent(this,
                                 DestinationStore.EventType.
                                     SELECTED_DESTINATION_CAPABILITIES_READY);
        }
      } else {
        // TODO(rltoscano): This makes the assumption that the "deviceName" is
        // the same as "printerName". We should include the "printerName" in the
        // response. See http://crbug.com/132831.
        destination = print_preview.LocalDestinationParser.parse(
            {deviceName: destinationId, printerName: destinationId});
        destination.capabilities = capabilities;
        this.insertDestination(destination);
      }
    },

    /**
     * Called when the /search call completes. Adds the fetched printers to the
     * destination store.
     * @param {cr.Event} event Contains the fetched printers.
     * @private
     */
    onCloudPrintSearchDone_: function(event) {
      this.insertDestinations(event.printers);
    },

    /**
     * Called when /printer call completes. Updates the specified destination's
     * print capabilities.
     * @param {cr.Event} event Contains detailed information about the
     *     destination.
     * @private
     */
    onCloudPrintPrinterDone_: function(event) {
      var dest = this.updateDestination(event.printer);
      if (this.selectedDestination_ == dest) {
        cr.dispatchSimpleEvent(
            this,
            DestinationStore.EventType.SELECTED_DESTINATION_CAPABILITIES_READY);
      }
    },

    /**
     * Called from native layer after the user was requested to sign in, and did
     * so successfully.
     * @private
     */
    onDestinationsReload_: function() {
      this.reset_();
      this.isInAutoSelectMode_ = true;
      this.startLoadLocalDestinations();
      this.startLoadRecentCloudDestinations();
      this.startLoadAllCloudDestinations();
    },

    /**
     * Called when no destination was auto-selected after some timeout. Selects
     * the first destination in store.
     * @private
     */
    onAutoSelectTimeoutExpired_: function() {
      this.autoSelectTimeout_ = null;
      assert(this.destinations_.length > 0,
             'No destinations were loaded before auto-select timeout expired');
      this.selectDestination(this.destinations_[0]);
    }
  };

  // Export
  return {
    DestinationStore: DestinationStore
  };
});

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-app',

  behaviors: [SettingsBehavior],
  properties: {
    /**
     * Object containing current settings of Print Preview, for use by Polymer
     * controls.
     * @type {!Object}
     */
    settings: {
      type: Object,
      notify: true,
    },

    /** @private {print_preview.Destination} */
    destination_: {
      type: Object,
      notify: true,
    },

    /** @private {?print_preview.DestinationStore} */
    destinationStore_: {
      type: Object,
      notify: true,
      value: null,
    },

    /** @private {print_preview.DocumentInfo} */
    documentInfo_: {
      type: Object,
      notify: true,
    },

    /** @private {!Array<print_preview.RecentDestination>} */
    recentDestinations_: {
      type: Array,
      notify: true,
    },

    /** @private {!print_preview_new.State} */
    state_: {
      type: Object,
      notify: true,
      value: {
        previewLoading: false,
        previewFailed: false,
        cloudPrintError: '',
        privetExtensionError: '',
        invalidSettings: false,
        initialized: false,
        cancelled: false,
        printRequested: false,
        printFailed: false,
      },
    },

    /** @private {?print_preview.UserInfo} */
    userInfo_: {
      type: Object,
      notify: true,
      value: null,
    },
  },

  observers: [
    'onPreviewCancelled_(state_.cancelled)',
    'onPrintRequested_(state_.printRequested, state_.previewLoading)',
  ],

  /** @private {?WebUIListenerTracker} */
  listenerTracker_: null,

  /** @type {!print_preview.MeasurementSystem} */
  measurementSystem_: new print_preview.MeasurementSystem(
      ',', '.', print_preview.MeasurementSystemUnitType.IMPERIAL),

  /** @private {?print_preview.NativeLayer} */
  nativeLayer_: null,

  /** @private {!EventTracker} */
  tracker_: new EventTracker(),

  /** @override */
  attached: function() {
    this.nativeLayer_ = print_preview.NativeLayer.getInstance();
    this.documentInfo_ = new print_preview.DocumentInfo();
    this.userInfo_ = new print_preview.UserInfo();
    this.listenerTracker_ = new WebUIListenerTracker();
    this.destinationStore_ = new print_preview.DestinationStore(
        this.userInfo_, this.listenerTracker_);
    this.tracker_.add(
        this.destinationStore_,
        print_preview.DestinationStore.EventType.DESTINATION_SELECT,
        this.onDestinationSelect_.bind(this));
    this.tracker_.add(
        this.destinationStore_,
        print_preview.DestinationStore.EventType
            .SELECTED_DESTINATION_CAPABILITIES_READY,
        this.onDestinationUpdated_.bind(this));
    this.nativeLayer_.getInitialSettings().then(
        this.onInitialSettingsSet_.bind(this));
  },

  /** @override */
  detached: function() {
    this.listenerTracker_.removeAll();
    this.tracker_.removeAll();
  },

  /**
   * @param {!print_preview.NativeInitialSettings} settings
   * @private
   */
  onInitialSettingsSet_: function(settings) {
    this.documentInfo_.init(
        settings.previewModifiable, settings.documentTitle,
        settings.documentHasSelection);
    this.notifyPath('documentInfo_.isModifiable');
    this.notifyPath('documentInfo_.hasSelection');
    this.notifyPath('documentInfo_.title');
    this.notifyPath('documentInfo_.pageCount');
    this.$.model.updateFromStickySettings(settings.serializedAppStateStr);
    this.measurementSystem_.setSystem(
        settings.thousandsDelimeter, settings.decimalDelimeter,
        settings.unitType);
    this.setSetting('selectionOnly', settings.shouldPrintSelectionOnly);
    this.destinationStore_.init(
        settings.isInAppKioskMode, settings.printerName,
        settings.serializedDefaultDestinationSelectionRulesStr,
        this.recentDestinations_);
  },

  /** @private */
  onDestinationSelect_: function() {
    this.destination_ = this.destinationStore_.selectedDestination;
  },

  /** @private */
  onDestinationUpdated_: function() {
    this.set(
        'destination_.capabilities',
        this.destinationStore_.selectedDestination.capabilities);
    if (!this.state_.initialized)
      this.set('state_.initialized', true);
  },

  /** @private */
  onPreviewCancelled_: function() {
    if (!this.state_.cancelled)
      return;
    this.detached();
    this.nativeLayer_.dialogClose(true);
  },

  /**
   * @param {!CustomEvent} e Event containing the sticky settings string.
   * @private
   */
  onSaveStickySettings_: function(e) {
    this.nativeLayer_.saveAppState(/** @type {string} */ (e.detail));
  },

  /** @private */
  onPrintRequested_: function() {
    if (this.state_.previewLoading || !this.state_.printRequested)
      return;

    assert(this.settings.scaling.valid);
    assert(this.settings.pages.valid);
    assert(this.settings.copies.valid);

    const destination = assert(this.destinationStore_.selectedDestination);
    const whenPrintDone =
        this.nativeLayer_.print(this.$.model.createPrintTicket(destination));
    if (destination.isLocal) {
      const onError = destination.id ==
              print_preview.Destination.GooglePromotedId.SAVE_AS_PDF ?
          this.onFileSelectionCancel_.bind(this) :
          this.onPrintFailed_.bind(this);
      whenPrintDone.then(this.close_.bind(this), onError);
    } else {
      // Cloud print resolves when print data is returned to submit to cloud
      // print, or if print ticket cannot be read, no PDF data is found, or
      // PDF is oversized.
      whenPrintDone.then(
          this.onPrintToCloud_.bind(this), this.onPrintFailed_.bind(this));
    }
  },

  /** @private */
  onFileSelectionCancel_: function() {
    this.set('state_.printRequested', false);
  },

  /** @private */
  onPrintToCloud_: function() {
    // TODO (rbpotter): Actually print to cloud print once cloud print
    // interface is hooked up. Currently there are no cloud printers so
    // this should never happen.
    assertNotReached('Trying to print to cloud printer!');
  },

  /**
   * Called when printing to a privet, cloud, or extension printer fails.
   * @param {*} httpError The HTTP error code, or -1 or a string describing
   *     the error, if not an HTTP error.
   * @private
   */
  onPrintFailed_: function(httpError) {
    console.error('Printing failed with error code ' + httpError);
    this.set('state.printFailed', true);
  },

  /** @private */
  close_: function() {
    this.detached();
    this.nativeLayer_.dialogClose(false);
  },
});

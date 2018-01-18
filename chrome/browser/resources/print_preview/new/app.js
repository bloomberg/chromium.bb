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

    /** @private {print_preview.DocumentInfo} */
    documentInfo_: {
      type: Object,
      notify: true,
    },

    /** @private {print_preview.Destination} */
    destination_: {
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
      },
    },
  },

  /** @private {?print_preview.NativeLayer} */
  nativeLayer_: null,

  /** @private {?print_preview.UserInfo} */
  userInfo_: null,

  /** @private {?WebUIListenerTracker} */
  listenerTracker_: null,

  /** @private {?print_preview.DestinationStore} */
  destinationStore_: null,

  /** @private {!EventTracker} */
  tracker_: new EventTracker(),

  /** @type {!print_preview.MeasurementSystem} */
  measurementSystem_: new print_preview.MeasurementSystem(
      ',', '.', print_preview.MeasurementSystemUnitType.IMPERIAL),

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
});

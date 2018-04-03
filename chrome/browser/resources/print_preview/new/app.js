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

    /** @private {?print_preview.InvitationStore} */
    invitationStore_: {
      type: Object,
      notify: true,
      value: null,
    },

    /** @private {!Array<print_preview.RecentDestination>} */
    recentDestinations_: {
      type: Array,
      notify: true,
    },

    /** @type {!print_preview_new.State} */
    state: {
      type: Number,
      observer: 'onStateChanged_',
    },

    /** @private {?print_preview.UserInfo} */
    userInfo_: {
      type: Object,
      notify: true,
      value: null,
    },

    /** @private {string} */
    errorMessage_: {
      type: String,
      notify: true,
      value: '',
    },

    /** @private {boolean} */
    controlsDisabled_: {
      type: Boolean,
      notify: true,
      computed: 'computeControlsDisabled_(state)',
    },

    /** @private {?print_preview.MeasurementSystem} */
    measurementSystem_: {
      type: Object,
      notify: true,
      value: null,
    },

    /** @private {boolean} */
    isInAppKioskMode_: {
      type: Boolean,
      notify: true,
      value: false,
    },
  },

  /** @private {?WebUIListenerTracker} */
  listenerTracker_: null,

  /** @private {?print_preview.NativeLayer} */
  nativeLayer_: null,

  /** @private {?cloudprint.CloudPrintInterface} */
  cloudPrintInterface_: null,

  /** @private {!EventTracker} */
  tracker_: new EventTracker(),

  /** @private {boolean} */
  cancelled_: false,

  /** @private {boolean} */
  showSystemDialogBeforePrint_: false,

  /** @private {boolean} */
  openPdfInPreview_: false,

  /** @override */
  attached: function() {
    this.nativeLayer_ = print_preview.NativeLayer.getInstance();
    this.documentInfo_ = new print_preview.DocumentInfo();
    this.userInfo_ = new print_preview.UserInfo();
    this.listenerTracker_ = new WebUIListenerTracker();
    this.listenerTracker_.add(
        'use-cloud-print', this.onCloudPrintEnable_.bind(this));
    this.destinationStore_ = new print_preview.DestinationStore(
        this.userInfo_, this.listenerTracker_);
    this.invitationStore_ = new print_preview.InvitationStore(this.userInfo_);
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
   * @return {boolean} Whether the controls should be disabled.
   * @private
   */
  computeControlsDisabled_: function() {
    return this.state != print_preview_new.State.READY;
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
    this.$.model.setStickySettings(settings.serializedAppStateStr);
    this.measurementSystem_ = new print_preview.MeasurementSystem(
        settings.thousandsDelimeter, settings.decimalDelimeter,
        settings.unitType);
    this.setSetting('selectionOnly', settings.shouldPrintSelectionOnly);
    this.destinationStore_.init(
        settings.isInAppKioskMode, settings.printerName,
        settings.serializedDefaultDestinationSelectionRulesStr,
        this.recentDestinations_);
    this.isInAppKioskMode_ = settings.isInAppKioskMode;
  },

  /**
   * Called when Google Cloud Print integration is enabled by the
   * PrintPreviewHandler.
   * Fetches the user's cloud printers.
   * @param {string} cloudPrintUrl The URL to use for cloud print servers.
   * @param {boolean} appKioskMode Whether to print automatically for kiosk
   *     mode.
   * @private
   */
  onCloudPrintEnable_: function(cloudPrintUrl, appKioskMode) {
    assert(!this.cloudPrintInterface_);
    this.cloudPrintInterface_ = new cloudprint.CloudPrintInterface(
        cloudPrintUrl, assert(this.nativeLayer_), assert(this.userInfo_),
        appKioskMode);
    this.tracker_.add(
        assert(this.cloudPrintInterface_),
        cloudprint.CloudPrintInterfaceEventType.SUBMIT_DONE,
        this.close_.bind(this));
    [cloudprint.CloudPrintInterfaceEventType.SEARCH_FAILED,
     cloudprint.CloudPrintInterfaceEventType.SUBMIT_FAILED,
     cloudprint.CloudPrintInterfaceEventType.PRINTER_FAILED,
    ].forEach(eventType => {
      this.tracker_.add(
          assert(this.cloudPrintInterface_), eventType,
          this.onCloudPrintError_.bind(this));
    });

    this.destinationStore_.setCloudPrintInterface(this.cloudPrintInterface_);
    this.invitationStore_.setCloudPrintInterface(this.cloudPrintInterface_);
    if (this.$.destinationSettings.isDialogOpen()) {
      this.destinationStore_.startLoadCloudDestinations();
      this.invitationStore_.startLoadingInvitations();
    }
  },

  /** @private */
  onDestinationSelect_: function() {
    this.destination_ = this.destinationStore_.selectedDestination;
    this.$.state.transitTo(print_preview_new.State.NOT_READY);
  },

  /** @private */
  onDestinationUpdated_: function() {
    this.set(
        'destination_.capabilities',
        this.destinationStore_.selectedDestination.capabilities);
    if (this.state != print_preview_new.State.READY)
      this.$.state.transitTo(print_preview_new.State.READY);
    if (!this.$.model.initialized())
      this.$.model.applyStickySettings();
  },

  /**
   * @param {!CustomEvent} e Event containing the sticky settings string.
   * @private
   */
  onSaveStickySettings_: function(e) {
    this.nativeLayer_.saveAppState(/** @type {string} */ (e.detail));
  },

  /** @private */
  onStateChanged_: function() {
    if (this.state == print_preview_new.State.CLOSING) {
      this.remove();
      this.nativeLayer_.dialogClose(this.cancelled_);
    } else if (this.state == print_preview_new.State.HIDDEN) {
      this.nativeLayer_.hidePreview();
    } else if (this.state == print_preview_new.State.PRINTING) {
      const destination = assert(this.destinationStore_.selectedDestination);
      const whenPrintDone =
          this.nativeLayer_.print(this.$.model.createPrintTicket(
              destination, this.openPdfInPreview_,
              this.showSystemDialogBeforePrint_));
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
    }
  },

  /** @private */
  onPreviewLoaded_: function() {
    if (this.state == print_preview_new.State.HIDDEN)
      this.$.state.transitTo(print_preview_new.State.PRINTING);
  },

  /** @private */
  onPrintRequested_: function() {
    this.$.state.transitTo(
        this.$.previewArea.previewLoaded() ? print_preview_new.State.PRINTING :
                                             print_preview_new.State.HIDDEN);
  },

  /** @private */
  onCancelRequested_: function() {
    this.cancelled_ = true;
    this.$.state.transitTo(print_preview_new.State.CLOSING);
  },

  /**
   * @param {!CustomEvent} e The event containing the new validity.
   * @private
   */
  onSettingValidChanged_: function(e) {
    this.$.state.transitTo(
        /** @type {boolean} */ (e.detail) ?
            print_preview_new.State.READY :
            print_preview_new.State.INVALID_TICKET);
  },

  /** @private */
  onFileSelectionCancel_: function() {
    this.$.state.transitTo(print_preview_new.State.READY);
  },

  /**
   * Called when the native layer has retrieved the data to print to Google
   * Cloud Print.
   * @param {string} data The body to send in the HTTP request.
   * @private
   */
  onPrintToCloud_: function(data) {
    assert(
        this.cloudPrintInterface_ != null, 'Google Cloud Print is not enabled');
    const destination = assert(this.destinationStore_.selectedDestination);
    this.cloudPrintInterface_.submit(
        destination, this.$.model.createCloudJobTicket(destination),
        assert(this.documentInfo_), data);
  },

  // <if expr="not chromeos">
  /** @private */
  onPrintWithSystemDialog_: function() {
    assert(!cr.isChromeOS);
    if (cr.isWindows) {
      this.showSystemDialogBeforePrint_ = true;
      this.onPrintRequested_();
      return;
    }
    this.nativeLayer_.showSystemDialog();
    this.$.state.transitTo(print_preview_new.State.SYSTEM_DIALOG);
  },
  // </if>

  // <if expr="is_macosx">
  /** @private */
  onOpenPdfInPreview_: function() {
    this.openPdfInPreview_ = true;
    this.$.previewArea.setOpeningPdfInPreview();
    this.onPrintRequested_();
  },
  // </if>

  /**
   * Called when printing to a privet, cloud, or extension printer fails.
   * @param {*} httpError The HTTP error code, or -1 or a string describing
   *     the error, if not an HTTP error.
   * @private
   */
  onPrintFailed_: function(httpError) {
    console.error('Printing failed with error code ' + httpError);
    this.errorMessage_ = httpError.toString();
    this.$.state.transitTo(print_preview_new.State.FATAL_ERROR);
  },

  /** @private */
  onPreviewFailed_: function() {
    this.$.state.transitTo(print_preview_new.State.FATAL_ERROR);
  },

  /**
   * Called when there was an error communicating with Google Cloud print.
   * Displays an error message in the print header.
   * @param {!Event} event Contains the error message.
   * @private
   */
  onCloudPrintError_: function(event) {
    if (event.status == 0) {
      return;  // Ignore, the system does not have internet connectivity.
    }
    if (event.status == 403) {
      if (!this.isInAppKioskMode_) {
        this.$.destinationSettings.showCloudPrintPromo();
      }
    } else {
      this.set('state_.cloudPrintError', event.message);
    }
    if (event.status == 200) {
      console.error(
          `Google Cloud Print Error: (${event.errorCode}) ${event.message}`);
    } else {
      console.error(`Google Cloud Print Error: HTTP status ${event.status}`);
    }
  },

  /** @private */
  close_: function() {
    this.$.state.transitTo(print_preview_new.State.CLOSING);
  },
});

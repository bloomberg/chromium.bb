// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

/** @type {number} Number of recent destinations to save. */
const NUM_PERSISTED_DESTINATIONS = 3;

Polymer({
  is: 'print-preview-destination-settings',

  behaviors: [
    I18nBehavior,
    SettingsBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    appKioskMode: Boolean,

    /** @type {?print_preview.Destination} */
    destination: {
      type: Object,
      notify: true,
      value: null,
    },

    /** @type {!print_preview.DestinationState} */
    destinationState: {
      type: Number,
      notify: true,
      value: print_preview.DestinationState.INIT,
      observer: 'updateDestinationSelect_',
    },

    disabled: Boolean,

    /** @private {string} */
    activeUser_: {
      type: String,
      observer: 'onActiveUserChanged_',
    },

    /** @private {!print_preview.CloudPrintState} */
    cloudPrintState_: {
      type: Number,
      value: print_preview.CloudPrintState.DISABLED,
    },

    /** @private {?print_preview.DestinationStore} */
    destinationStore_: {
      type: Object,
      value: null,
    },

    /** @private {?print_preview.InvitationStore} */
    invitationStore_: {
      type: Object,
      value: null,
    },

    /** @private {!Array<!print_preview.Destination>} */
    recentDestinationList_: Array,

    /** @private */
    shouldHideSpinner_: {
      type: Boolean,
      computed: 'computeShouldHideSpinner_(destinationState)',
    },

    /** @private {string} */
    statusText_: {
      type: String,
      computed: 'computeStatusText_(destination)',
    },

    /** @private {!Array<string>} */
    users_: Array,
  },

  /** @private {!EventTracker} */
  tracker_: new EventTracker(),

  /** @override */
  attached: function() {
    this.destinationStore_ =
        new print_preview.DestinationStore(this.addWebUIListener.bind(this));
    this.invitationStore_ = new print_preview.InvitationStore();
    this.tracker_.add(
        this.destinationStore_,
        print_preview.DestinationStore.EventType.DESTINATION_SELECT,
        this.onDestinationSelect_.bind(this));
    this.tracker_.add(
        this.destinationStore_,
        print_preview.DestinationStore.EventType
            .SELECTED_DESTINATION_CAPABILITIES_READY,
        this.onDestinationCapabilitiesReady_.bind(this));
    this.tracker_.add(
        this.destinationStore_, print_preview.DestinationStore.EventType.ERROR,
        this.onDestinationError_.bind(this));
    // Need to update the recent list when the destination store inserts
    // destinations, in case any recent destinations have been added to the
    // store. At startup, recent destinations can be in the sticky settings,
    // but they should not be displayed in the dropdown until they have been
    // fetched by the DestinationStore, to ensure that they still exist.
    this.tracker_.add(
        assert(this.destinationStore_),
        print_preview.DestinationStore.EventType.DESTINATIONS_INSERTED,
        this.updateDropdownDestinations_.bind(this));
  },

  /** @param {!cloudprint.CloudPrintInterface} cloudPrintInterface */
  setCloudPrintInterface: function(cloudPrintInterface) {
    [cloudprint.CloudPrintInterfaceEventType.SEARCH_FAILED,
     cloudprint.CloudPrintInterfaceEventType.PRINTER_FAILED,
    ].forEach(eventType => {
      this.tracker_.add(
          cloudPrintInterface.getEventTarget(), eventType,
          this.checkCloudPrintStatus_.bind(this));
    });
    this.$.userInfo.setCloudPrintInterface(cloudPrintInterface);
    this.destinationStore_.setCloudPrintInterface(cloudPrintInterface);
    this.invitationStore_.setCloudPrintInterface(cloudPrintInterface);
    assert(this.cloudPrintState_ === print_preview.CloudPrintState.DISABLED);
    this.cloudPrintState_ = print_preview.CloudPrintState.ENABLED;
  },

  /**
   * @param {string} defaultPrinter The system default printer ID.
   * @param {string} serializedDefaultDestinationRulesStr String with rules for
   *     selecting a default destination.
   */
  initDestinationStore: function(
      defaultPrinter, serializedDefaultDestinationRulesStr) {
    this.destinationStore_.init(
        this.appKioskMode, defaultPrinter, serializedDefaultDestinationRulesStr,
        /** @type {!Array<print_preview.RecentDestination>} */
        (this.getSettingValue('recentDestinations')));
  },

  /** @private */
  onActiveUserChanged_: function() {
    if (!this.activeUser_) {
      return;
    }

    assert(this.cloudPrintState_ !== print_preview.CloudPrintState.DISABLED);
    this.cloudPrintState_ = print_preview.CloudPrintState.SIGNED_IN;

    // Load docs, in case the user was not signed in previously and signed in
    // from the destinations dialog.
    this.destinationStore_.startLoadCookieDestination(
        print_preview.Destination.GooglePromotedId.DOCS);

    // Load any recent cloud destinations for the dropdown.
    const recentDestinations = this.getSettingValue('recentDestinations');
    recentDestinations.forEach(destination => {
      if (destination.origin === print_preview.DestinationOrigin.COOKIES &&
          (destination.account === this.activeUser_ ||
           destination.account === '')) {
        this.destinationStore_.startLoadCookieDestination(destination.id);
      }
    });

    if (this.destinationState === print_preview.DestinationState.SELECTED &&
        this.destination.origin === print_preview.DestinationOrigin.COOKIES) {
      this.destinationState = this.destination.capabilities ?
          print_preview.DestinationState.UPDATED :
          print_preview.DestinationState.SET;
    }
    this.updateDropdownDestinations_();
  },

  /** @private */
  onDestinationSelect_: function() {
    this.destinationState = print_preview.DestinationState.SELECTED;
    this.destination = this.destinationStore_.selectedDestination;
    this.updateRecentDestinations_();
    if (this.cloudPrintState_ === print_preview.CloudPrintState.ENABLED) {
      // Only try to load the docs destination for now. If this request
      // succeeds, it will trigger a transition to SIGNED_IN, and we can
      // load the remaining destinations. Otherwise, it will transition to
      // NOT_SIGNED_IN, so we will not do this more than once.
      this.destinationStore_.startLoadCookieDestination(
          print_preview.Destination.GooglePromotedId.DOCS);
    }
    if (this.cloudPrintState_ === print_preview.CloudPrintState.SIGNED_IN ||
        this.destination.origin !== print_preview.DestinationOrigin.COOKIES) {
      this.destinationState = print_preview.DestinationState.SET;
    }
  },

  /** @private */
  onDestinationCapabilitiesReady_: function() {
    this.notifyPath('destination.capabilities');
    if (this.destinationState === print_preview.DestinationState.SET) {
      this.destinationState = print_preview.DestinationState.UPDATED;
    }
    this.updateRecentDestinations_();
  },

  /**
   * @param {!CustomEvent<!print_preview.DestinationErrorType>} e
   * @private
   */
  onDestinationError_: function(e) {
    switch (e.detail) {
      case print_preview.DestinationErrorType.INVALID:
        this.destinationState = print_preview.DestinationState.INVALID;
        break;
      case print_preview.DestinationErrorType.UNSUPPORTED:
        this.destinationState = print_preview.DestinationState.UNSUPPORTED;
        break;
      // <if expr="chromeos">
      case print_preview.DestinationErrorType.NO_DESTINATIONS:
        this.noDestinationsFound_ = true;
        this.destinationState = print_preview.DestinationState.NO_DESTINATIONS;
        break;
      // </if>
      default:
        break;
    }
  },

  /**
   * Updates the cloud print status to NOT_SIGNED_IN if there is an
   * authentication error.
   * @param {!CustomEvent<!cloudprint.CloudPrintInterfaceErrorEventDetail>}
   *     event Contains the error status
   * @private
   */
  checkCloudPrintStatus_: function(event) {
    if (event.detail.status != 403 || this.appKioskMode) {
      return;
    }

    // Should not have sent a message to Cloud Print if cloud print is
    // disabled.
    assert(this.cloudPrintState_ !== print_preview.CloudPrintState.DISABLED);
    this.cloudPrintState_ = print_preview.CloudPrintState.NOT_SIGNED_IN;
    console.warn('Google Cloud Print Error: HTTP status 403');
  },

  /**
   * @param {!print_preview.RecentDestination} destination
   * @return {boolean} Whether the destination is Save as PDF or Save to Drive.
   */
  destinationIsDriveOrPdf_: function(destination) {
    return destination.id ===
        print_preview.Destination.GooglePromotedId.SAVE_AS_PDF ||
        destination.id === print_preview.Destination.GooglePromotedId.DOCS;
  },

  /** @private */
  updateRecentDestinations_: function() {
    if (!this.destination) {
      return;
    }

    // Determine if this destination is already in the recent destinations,
    // and where in the array it is located.
    const newDestination =
        print_preview.makeRecentDestination(assert(this.destination));
    const recentDestinations = this.getSettingValue('recentDestinations');
    let indexFound = recentDestinations.findIndex(function(recent) {
      return (
          newDestination.id == recent.id &&
          newDestination.origin == recent.origin);
    });

    // No change
    if (indexFound == 0 &&
        recentDestinations[0].capabilities == newDestination.capabilities) {
      return;
    }
    const isNew = indexFound == -1;

    // Shift the array so that the nth most recent destination is located at
    // index n.
    if (isNew && recentDestinations.length == NUM_PERSISTED_DESTINATIONS) {
      indexFound = NUM_PERSISTED_DESTINATIONS - 1;
    }
    if (indexFound != -1) {
      this.setSettingSplice('recentDestinations', indexFound, 1, null);
    }

    // Add the most recent destination
    this.setSettingSplice('recentDestinations', 0, 0, newDestination);
    if (!this.destinationIsDriveOrPdf_(newDestination) && isNew) {
      this.updateDropdownDestinations_();
    }
  },

  /** @private */
  updateDropdownDestinations_: function() {
    const recentDestinations = this.getSettingValue('recentDestinations');

    const dropdownDestinations = [];
    recentDestinations.forEach(recentDestination => {
      const key = print_preview.createRecentDestinationKey(recentDestination);
      const destination = this.destinationStore_.getDestinationByKey(key);
      if (destination && !this.destinationIsDriveOrPdf_(recentDestination) &&
          (!destination.account || destination.account == this.activeUser_)) {
        dropdownDestinations.push(destination);
      }
    });

    this.recentDestinationList_ = dropdownDestinations;
  },

  /**
   * @return {boolean} Whether the destinations dropdown should be disabled.
   * @private
   */
  shouldDisableDropdown_: function() {
    // <if expr="chromeos">
    if (this.destinationState ===
        print_preview.DestinationState.NO_DESTINATIONS) {
      return true;
    }
    // </if>

    return this.destinationState === print_preview.DestinationState.INIT ||
        this.destinationState === print_preview.DestinationState.SELECTED ||
        (this.disabled &&
         (this.destinationState === print_preview.DestinationState.SET ||
          this.destinationState === print_preview.DestinationState.UPDATED));
  },

  /** @private */
  computeShouldHideSpinner_: function() {
    return this.destinationState !== print_preview.DestinationState.INIT &&
        this.destinationState !== print_preview.DestinationState.SELECTED;
  },

  /**
   * @return {string} The connection status text to display.
   * @private
   */
  computeStatusText_: function() {
    // |destination| can be either undefined, or null here.
    if (!this.destination) {
      return '';
    }

    return this.destination.shouldShowInvalidCertificateError ?
        this.i18n('noLongerSupportedFragment') :
        this.destination.connectionStatusText;
  },

  /**
   * @param {!CustomEvent<string>} e Event containing the new selected value.
   * @private
   */
  onSelectedDestinationOptionChange_: function(e) {
    const value = e.detail;
    if (value === 'seeMore') {
      this.destinationStore_.startLoadAllDestinations();
      if (this.activeUser_) {
        this.invitationStore_.startLoadingInvitations(this.activeUser_);
      }
      this.$.destinationDialog.get().show();
    } else {
      this.destinationStore_.selectDestinationByKey(value);
    }
  },

  /**
   * @param {!CustomEvent<string>} e Event containing the new active user
   *     account.
   * @private
   */
  onAccountChange_: function(e) {
    this.$.userInfo.updateActiveUser(e.detail);
  },

  /** @private */
  onDialogClose_: function() {
    // Reset the select value if the user dismissed the dialog without
    // selecting a new destination.
    this.updateDestinationSelect_();
    this.$.destinationSelect.focus();
  },

  /** @private */
  updateDestinationSelect_: function() {
    // <if expr="chromeos">
    if (this.destinationState ===
        print_preview.DestinationState.NO_DESTINATIONS) {
      return;
    }
    // </if>

    if (this.destinationState === print_preview.DestinationState.INIT ||
        this.destinationState === print_preview.DestinationState.SELECTED) {
      return;
    }

    // TODO (rbpotter): Remove this conditional when the Polymer 2 migration
    // is completed.
    if (Polymer.DomIf) {
      Polymer.RenderStatus.beforeNextRender(this.$.destinationSelect, () => {
        this.$.destinationSelect.updateDestination();
      });
    } else {
      this.$.destinationSelect.async(() => {
        this.$.destinationSelect.updateDestination();
      });
    }
  },
});
})();

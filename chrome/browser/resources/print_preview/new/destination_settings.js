// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-destination-settings',

  behaviors: [I18nBehavior],

  properties: {
    activeUser: String,

    appKioskMode: Boolean,

    /** @type {!print_preview.CloudPrintState} */
    cloudPrintState: {
      type: Number,
      observer: 'onCloudPrintStateChanged_',
    },

    /** @type {!print_preview.Destination} */
    destination: {
      type: Object,
      observer: 'onDestinationSet_',
    },

    /** @type {?print_preview.DestinationStore} */
    destinationStore: {
      type: Object,
      observer: 'onDestinationStoreSet_',
    },

    disabled: Boolean,

    /** @type {?print_preview.InvitationStore} */
    invitationStore: Object,

    noDestinationsFound: {
      type: Boolean,
      value: false,
    },

    /** @type {!Array<!print_preview.RecentDestination>} */
    recentDestinations: Array,

    /** @type {!print_preview_new.State} */
    state: Number,

    /** @type {!Array<string>} */
    users: Array,

    /** @private {!Array<!print_preview.Destination>} */
    recentDestinationList_: Array,

    /** @private */
    shouldHideSpinner_: {
      type: Boolean,
      computed: 'computeShouldHideSpinner_(' +
          'destination, noDestinationsFound, cloudPrintState)',
    },

    /** @private {string} */
    statusText_: {
      type: String,
      computed: 'computeStatusText_(destination)',
    },
  },

  observers: [
    'updateRecentDestinationList_(' +
        'recentDestinations.*, activeUser, destinationStore)',
    'updateDestinationSelect_(' +
        'destination, noDestinationsFound, cloudPrintState)',
  ],

  /** @private {!EventTracker} */
  tracker_: new EventTracker(),

  /** @private */
  onDestinationStoreSet_: function() {
    if (!this.destinationStore) {
      return;
    }

    // Need to update the recent list when the destination store inserts
    // destinations, in case any recent destinations have been added to the
    // store. At startup, recent destinations can be in the sticky settings,
    // but they should not be displayed in the dropdown until they have been
    // fetched by the DestinationStore, to ensure that they still exist.
    this.tracker_.add(
        assert(this.destinationStore),
        print_preview.DestinationStore.EventType.DESTINATIONS_INSERTED,
        this.updateRecentDestinationList_.bind(this));
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
  updateRecentDestinationList_: function() {
    if (!this.recentDestinations || !this.destinationStore) {
      return;
    }

    const recentDestinations = [];
    let update = false;
    const existingKeys = this.recentDestinationList_ ?
        this.recentDestinationList_.map(listItem => listItem.key) :
        [];
    this.recentDestinations.forEach(recentDestination => {
      const key = print_preview.createRecentDestinationKey(recentDestination);
      const destination = this.destinationStore.getDestinationByKey(key);
      if (destination && !this.destinationIsDriveOrPdf_(recentDestination) &&
          (!destination.account || destination.account == this.activeUser)) {
        recentDestinations.push(destination);
        update = update || !existingKeys.includes(key);
      }
    });

    // Only update the list if new destinations have been added to it.
    // Re-ordering the dropdown items every time the selected item changes is
    // a bad experience for keyboard users.
    if (update) {
      this.recentDestinationList_ = recentDestinations;
    }
  },

  /**
   * @return {boolean} Whether the destinations dropdown should be disabled.
   * @private
   */
  shouldDisableDropdown_: function() {
    return !this.destinationStore || this.noDestinationsFound ||
        !this.shouldHideSpinner_ ||
        (this.disabled && this.state != print_preview_new.State.NOT_READY &&
         this.state != print_preview_new.State.INVALID_PRINTER);
  },

  /** @private */
  onCloudPrintStateChanged_: function() {
    if (this.cloudPrintState !== print_preview.CloudPrintState.SIGNED_IN) {
      return;
    }

    // Load docs, in case the user was not signed in previously and signed in
    // from the destinations dialog.
    this.destinationStore.startLoadCookieDestination(
        print_preview.Destination.GooglePromotedId.DOCS);

    // Load any recent cloud destinations for the dropdown.
    this.recentDestinations.forEach(destination => {
      if (destination.origin === print_preview.DestinationOrigin.COOKIES &&
          (destination.account === this.activeUser ||
           destination.account === '')) {
        this.destinationStore.startLoadCookieDestination(destination.id);
      }
    });
  },

  /** @private */
  onDestinationSet_: function() {
    if (this.cloudPrintState === print_preview.CloudPrintState.ENABLED) {
      // Only try to load the docs destination for now. If this request
      // succeeds, it will trigger a transition to SIGNED_IN, and we can
      // load the remaining destinations. Otherwise, it will transition to
      // NOT_SIGNED_IN, so we will not do this more than once.
      this.destinationStore.startLoadCookieDestination(
          print_preview.Destination.GooglePromotedId.DOCS);
    }
  },

  /** @private */
  computeShouldHideSpinner_: function() {
    if (this.noDestinationsFound) {
      return true;
    }

    return !!this.destination &&
        (this.destination.origin !== print_preview.DestinationOrigin.COOKIES ||
         this.cloudPrintState === print_preview.CloudPrintState.SIGNED_IN);
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
      this.destinationStore.startLoadAllDestinations();
      if (this.activeUser) {
        this.invitationStore.startLoadingInvitations(this.activeUser);
      }
      this.$.destinationDialog.get().show();
    } else {
      this.destinationStore.selectDestinationByKey(value);
    }
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
    if (!this.destination ||
        (this.destination.origin === print_preview.DestinationOrigin.COOKIES &&
         this.cloudPrintState !== print_preview.CloudPrintState.SIGNED_IN) ||
        this.noDestinationsFound) {
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

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
    destination: Object,

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

    /** @private {boolean} */
    loadingDestination_: {
      type: Boolean,
      value: true,
    },

    /** @private {!Array<!print_preview.Destination>} */
    recentDestinationList_: Array,

    /** @private {string} */
    statusText_: {
      type: String,
      computed: 'computeStatusText_(destination.connectionStatus, ' +
          'destination.shouldShowInvalidCertificateError)',
    },
  },

  observers: [
    'updateRecentDestinationList_(' +
        'recentDestinations.*, activeUser, destinationStore)',
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

  /** @private */
  updateRecentDestinationList_: function() {
    if (!this.recentDestinations || !this.destinationStore) {
      return;
    }

    const recentDestinations = [];
    let filterAccount = this.activeUser;
    // Fallback to the account for the current destination, in case activeUser
    // is not known yet from cloudprint.
    if (!filterAccount) {
      filterAccount = this.destination ? this.destination.account : '';
    }
    this.recentDestinations.forEach(recentDestination => {
      const destination = this.destinationStore.getDestinationByKey(
          print_preview.createRecentDestinationKey(recentDestination));
      if (destination &&
          (!destination.account || destination.account == filterAccount)) {
        recentDestinations.push(destination);
      }
    });
    this.recentDestinationList_ = recentDestinations;
    // Update the select value only after re-rendering the dropdown items.
    // Otherwise, the select will sometimes display the wrong printer value
    // (even though |destination| is correctly updated). Note that changing
    // |destination| will always result in a subsequent change to
    // |recentDestinations|.
    Polymer.RenderStatus.afterNextRender(this, () => {
      this.loadingDestination_ = !this.destination || !this.destination.id;
      if (!this.loadingDestination_) {
        this.$.destinationSelect.updateDestination();
      }
    });
  },

  /**
   * @return {boolean} Whether the destinations dropdown should be disabled.
   * @private
   */
  shouldDisableDropdown_: function() {
    return !this.destinationStore || this.noDestinationsFound ||
        (this.disabled &&
         this.state != print_preview_new.State.INVALID_PRINTER);
  },

  /** @private */
  onCloudPrintStateChanged_: function() {
    if (this.cloudPrintState !== print_preview.CloudPrintState.ENABLED &&
        this.cloudPrintState !== print_preview.CloudPrintState.SIGNED_IN) {
      return;
    }

    this.loadDropdownCloudDestinations_();
    if (this.cloudPrintState === print_preview.CloudPrintState.SIGNED_IN) {
      return;
    }

    const destinationDialog = this.$$('print-preview-destination-dialog');
    if (destinationDialog && destinationDialog.isOpen()) {
      this.destinationStore.startLoadCloudDestinations();
      if (this.activeUser) {
        this.invitationStore.startLoadingInvitations(this.activeUser);
      }
    }
  },

  /**
   * @return {boolean} Whether to show the spinner.
   * @private
   */
  shouldShowSpinner_: function() {
    return this.loadingDestination_ && !this.noDestinationsFound;
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

  /** @private */
  loadDropdownCloudDestinations_: function() {
    this.destinationStore.startLoadCookieDestination(
        print_preview.Destination.GooglePromotedId.DOCS);
    this.recentDestinations.forEach(destination => {
      if (destination.origin === print_preview.DestinationOrigin.COOKIES &&
          (destination.account === this.activeUser ||
           destination.account === '')) {
        this.destinationStore.startLoadCookieDestination(destination.id);
      }
    });
  },

  /**
   * @param {!CustomEvent<string>} e Event containing the new selected value.
   * @private
   */
  onSelectedDestinationOptionChange_: function(e) {
    const value = e.detail;
    if (value === 'selectDestination') {
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
    // Reset the select value in case the user dismissed the dialog without
    // selecting a new destination.
    if (this.destination) {
      this.$.destinationSelect.updateDestination();
    }
    this.$.destinationSelect.focus();
  },
});

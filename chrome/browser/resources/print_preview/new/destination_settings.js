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
      observer: 'onLoadingDestinationChange_',
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

    this.loadingDestination_ = !this.destination || !this.destination.id;
  },

  /**
   * @return {boolean} Whether the destinations dropdown should be disabled.
   * @private
   */
  shouldDisableDropdown_: function() {
    return !this.destinationStore || this.noDestinationsFound ||
        this.loadingDestination_ ||
        (this.disabled && this.state != print_preview_new.State.NOT_READY &&
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
    // Reset the select value in case the user dismissed the dialog without
    // selecting a new destination.
    if (this.destination) {
      this.$.destinationSelect.updateDestination();
    }
    this.$.destinationSelect.focus();
  },

  /** @private */
  onLoadingDestinationChange_: function() {
    if (this.loadingDestination_) {
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

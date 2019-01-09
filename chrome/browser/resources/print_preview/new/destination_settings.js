// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-destination-settings',

  behaviors: [I18nBehavior],

  properties: {
    activeUser: String,

    /** @type {!print_preview.CloudPrintState} */
    cloudPrintState: {
      type: Number,
      observer: 'onCloudPrintStateChanged_',
    },

    /** @type {!print_preview.Destination} */
    destination: Object,

    /** @type {?print_preview.DestinationStore} */
    destinationStore: Object,

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

    /** @private {boolean} */
    stale_: {
      type: Boolean,
      computed: 'computeStale_(destination)',
      reflectToAttribute: true,
    },
  },

  observers: ['onDestinationSet_(destination, destination.id)'],

  /**
   * @return {boolean} Whether the destination change button should be disabled.
   * @private
   */
  shouldDisableButton_: function() {
    return !this.destinationStore || this.noDestinationsFound ||
        (this.disabled &&
         this.state != print_preview_new.State.INVALID_PRINTER);
  },

  /** @private */
  onDestinationSet_: function() {
    this.loadingDestination_ = !this.destination || !this.destination.id;
  },

  /** @private */
  onCloudPrintStateChanged_: function() {
    if (this.cloudPrintState !== print_preview.CloudPrintState.ENABLED) {
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
  getStatusText_: function() {
    // |destination| can be either undefined, or null here.
    if (!this.destination) {
      return '';
    }

    return this.destination.shouldShowInvalidCertificateError ?
        this.i18n('noLongerSupportedFragment') :
        this.destination.connectionStatusText;
  },

  /** @private */
  onChangeButtonClick_: function() {
    this.destinationStore.startLoadAllDestinations();
    if (this.activeUser) {
      this.invitationStore.startLoadingInvitations(this.activeUser);
    }
    const dialog = this.$.destinationDialog.get();
    dialog.show();
  },

  /**
   * @return {boolean} Whether the destination is offline or invalid, indicating
   *     that "stale" styling should be applied.
   * @private
   */
  computeStale_: function() {
    return !!this.destination &&
        (this.destination.isOffline ||
         this.destination.shouldShowInvalidCertificateError);
  },

  /** @private */
  onDialogClose_: function() {
    this.$$('paper-button').focus();
  },
});

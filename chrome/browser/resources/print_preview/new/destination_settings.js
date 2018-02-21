// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-destination-settings',

  properties: {
    /** @type {!print_preview.Destination} */
    destination: Object,

    /** @type {?print_preview.DestinationStore} */
    destinationStore: Object,

    /** @type {!Array<!print_preview.RecentDestination>} */
    recentDestinations: Array,

    /** @type {!print_preview.UserInfo} */
    userInfo: {
      type: Object,
      notify: true,
    },

    disabled: Boolean,

    /** @type {!print_preview_new.State} */
    state: Number,

    /** @private {boolean} */
    showCloudPrintPromo_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    loadingDestination_: {
      type: Boolean,
      value: true,
    },
  },

  observers: ['onDestinationSet_(destination, destination.id)'],

  /**
   * @return {boolean} Whether the destination change button should be disabled.
   * @private
   */
  shouldDisableButton_: function() {
    return !this.destinationStore ||
        (this.disabled &&
         this.state != print_preview_new.State.INVALID_PRINTER);
  },

  /** @private */
  onDestinationSet_: function() {
    if (this.destination && this.destination.id)
      this.loadingDestination_ = false;
  },

  /** @private */
  onChangeButtonClick_: function() {
    this.destinationStore.startLoadAllDestinations();
    const dialog = this.$.destinationDialog.get();
    // This async() call is a workaround to prevent a DCHECK - see
    // https://crbug.com/804047.
    this.async(() => {
      dialog.show();
    }, 1);
  },

  showCloudPrintPromo: function() {
    this.showCloudPrintPromo_ = true;
  },

  /** @return {boolean} Whether the destinations dialog is open. */
  isDialogOpen: function() {
    const destinationDialog = this.$$('print-preview-destination-dialog');
    return destinationDialog && destinationDialog.isOpen();
  },
});

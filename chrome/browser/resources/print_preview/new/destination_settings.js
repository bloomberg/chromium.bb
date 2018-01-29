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

    /** @type {!print_preview.UserInfo} */
    userInfo: Object,

    /** @private {boolean} */
    loadingDestination_: {
      type: Boolean,
      value: true,
    },
  },

  observers: ['onDestinationSet_(destination, destination.id)'],

  /** @private */
  onDestinationSet_: function() {
    if (this.destination && this.destination.id)
      this.loadingDestination_ = false;
  },

  /** @private */
  onChangeButtonTap_: function() {
    this.destinationStore.startLoadAllDestinations();
    const dialog = this.$.destinationDialog.get();
    // This async() call is a workaround to prevent a DCHECK - see
    // https://crbug.com/804047.
    this.async(() => {
      dialog.show();
    }, 1);
  },
});

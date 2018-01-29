// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-destination-dialog',

  properties: {
    /** @type {?print_preview.DestinationStore} */
    destinationStore: {
      type: Object,
      observer: 'onDestinationStoreSet_',
    },

    /** @type {!print_preview.UserInfo} */
    userInfo: Object,

    /** @private {Array<!print_preview.Destination>} */
    destinations_: {
      type: Array,
      notify: true,
      value: [],
    },

    /** @private {boolean} */
    loadingDestinations_: {
      type: Boolean,
      value: false,
    },
  },

  /** @private {!EventTracker} */
  tracker_: new EventTracker(),

  /** @private */
  onDestinationStoreSet_: function() {
    assert(this.destinations_.length == 0);
    const destinationStore = assert(this.destinationStore);
    this.tracker_.add(
        destinationStore,
        print_preview.DestinationStore.EventType.DESTINATIONS_INSERTED,
        this.updateDestinations_.bind(this));
    this.tracker_.add(
        destinationStore,
        print_preview.DestinationStore.EventType.DESTINATION_SEARCH_DONE,
        this.updateDestinations_.bind(this));
  },

  /** @private */
  updateDestinations_: function() {
    this.destinations_ = this.userInfo ?
        this.destinationStore.destinations(this.userInfo.activeUser) :
        [];
    this.loadingDestinations_ =
        this.destinationStore.isPrintDestinationSearchInProgress;
  },

  /**
   * @return {string} The cloud print promotion HTML.
   * @private
   */
  getCloudPrintPromotion_: function() {
    return loadTimeData.getStringF(
        'cloudPrintPromotion', '<a is="action-link" class="sign-in">', '</a>');
  },

  /** @private */
  onCancelButtonTap_: function() {
    this.$.dialog.cancel();
  },

  show: function() {
    this.loadingDestinations_ =
        this.destinationStore.isPrintDestinationSearchInProgress;
    this.$.dialog.showModal();
  },
});

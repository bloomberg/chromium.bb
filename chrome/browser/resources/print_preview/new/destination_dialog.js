// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-destination-dialog',

  behaviors: [I18nBehavior],

  properties: {
    /** @type {?print_preview.DestinationStore} */
    destinationStore: {
      type: Object,
      observer: 'onDestinationStoreSet_',
    },

    /** @type {!print_preview.UserInfo} */
    userInfo: {
      type: Object,
      notify: true,
    },

    /** @type {boolean} */
    showCloudPrintPromo: {
      type: Boolean,
      notify: true,
    },

    /** @private {!Array<!print_preview.Destination>} */
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

    /** @type {!Array<!print_preview.RecentDestination>} */
    recentDestinations: Array,

    /** @private {!Array<!print_preview.Destination>} */
    recentDestinationList_: {
      type: Array,
      notify: true,
      computed: 'computeRecentDestinationList_(' +
          'destinationStore, recentDestinations, recentDestinations.*, ' +
          'userInfo, destinations_.*)',
    },

    /** @private {?RegExp} */
    searchQuery_: {
      type: Object,
      value: null,
    },
  },

  /** @private {!EventTracker} */
  tracker_: new EventTracker(),

  /** @override */
  ready: function() {
    this.$$('.promo-text').innerHTML =
        this.i18nAdvanced('cloudPrintPromotion', {
          substitutions: ['<a is="action-link" class="sign-in">', '</a>'],
          attrs: {
            'is': (node, v) => v == 'action-link',
            'class': (node, v) => v == 'sign-in',
          },
        });
  },

  /** @override */
  attached: function() {
    this.tracker_.add(
        assert(this.$$('.sign-in')), 'click', this.onSignInClick_.bind(this));
    this.tracker_.add(
        assert(this.$$('.cloudprint-promo > .close-button')), 'click',
        this.onCloudPrintPromoDismissed_.bind(this));
  },

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
    this.notifyPath('userInfo.users');
    this.notifyPath('userInfo.activeUser');
    this.notifyPath('userInfo.loggedIn');
    if (this.userInfo.loggedIn)
      this.showCloudPrintPromo = false;

    this.destinations_ = this.userInfo ?
        this.destinationStore.destinations(this.userInfo.activeUser) :
        [];
    this.loadingDestinations_ =
        this.destinationStore.isPrintDestinationSearchInProgress;
  },

  /**
   * @return {!Array<!print_preview.Destination>}
   * @private
   */
  computeRecentDestinationList_: function() {
    let recentDestinations = [];
    const filterAccount = this.userInfo.activeUser;
    this.recentDestinations.forEach((recentDestination) => {
      const destination = this.destinationStore.getDestination(
          recentDestination.origin, recentDestination.id,
          recentDestination.account || '');
      if (destination &&
          (!destination.account || destination.account == filterAccount)) {
        recentDestinations.push(destination);
      }
    });
    return recentDestinations;
  },

  /** @private */
  onCloseOrCancel_: function() {
    if (this.searchQuery_)
      this.$.searchBox.setValue('');
  },

  /** @private */
  onCancelButtonClick_: function() {
    this.$.dialog.cancel();
  },

  /**
   * @param {!CustomEvent} e Event containing the selected destination.
   * @private
   */
  onDestinationSelected_: function(e) {
    this.destinationStore.selectDestination(
        /** @type {!print_preview.Destination} */ (e.detail));
    this.$.dialog.close();
  },

  show: function() {
    this.loadingDestinations_ =
        this.destinationStore.isPrintDestinationSearchInProgress;
    this.$.dialog.showModal();
  },

  /** @return {boolean} Whether the dialog is open. */
  isOpen: function() {
    return this.$.dialog.hasAttribute('open');
  },

  /** @private */
  isSelected_: function(account) {
    return account == this.userInfo.activeUser;
  },

  /** @private */
  onSignInClick_: function() {
    print_preview.NativeLayer.getInstance().signIn(false).then(() => {
      this.destinationStore.onDestinationsReload();
    });
  },

  /** @private */
  onCloudPrintPromoDismissed_: function() {
    this.showCloudPrintPromo = false;
  },

  /** @private */
  onUserChange_: function() {
    const select = this.$$('select');
    const account = select.value;
    if (account) {
      this.showCloudPrintPromo = false;
      this.userInfo.activeUser = account;
      this.notifyPath('userInfo.activeUser');
      this.notifyPath('userInfo.loggedIn');
      this.destinationStore.reloadUserCookieBasedDestinations();
    } else {
      print_preview.NativeLayer.getInstance().signIn(true).then(
          this.destinationStore.onDestinationsReload.bind(
              this.destinationStore));
      const options = select.querySelectorAll('option');
      for (let i = 0; i < options.length; i++) {
        if (options[i].value == this.userInfo.activeUser) {
          select.selectedIndex = i;
          break;
        }
      }
    }
  },
});

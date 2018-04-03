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

    /** @type {?print_preview.InvitationStore} */
    invitationStore: {
      type: Object,
      observer: 'onInvitationStoreSet_',
    },

    /** @private {?print_preview.Invitation} */
    invitation_: {
      type: Object,
      value: null,
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
        assert(this.$$('#cloudprintPromo > .close-button')), 'click',
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
        this.onDestinationSearchDone_.bind(this));
  },

  /** @private */
  onInvitationStoreSet_: function() {
    const invitationStore = assert(this.invitationStore);
    this.tracker_.add(
        invitationStore,
        print_preview.InvitationStore.EventType.INVITATION_SEARCH_DONE,
        this.updateInvitations_.bind(this));
    this.tracker_.add(
        invitationStore,
        print_preview.InvitationStore.EventType.INVITATION_PROCESSED,
        this.updateInvitations_.bind(this));
  },

  /** @private */
  onDestinationSearchDone_: function() {
    this.updateDestinations_();
    this.invitationStore.startLoadingInvitations();
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

  /**
   * Updates printer sharing invitations UI.
   * @private
   */
  updateInvitations_: function() {
    const invitations = this.userInfo.activeUser ?
        this.invitationStore.invitations(this.userInfo.activeUser) :
        [];
    this.invitation_ = invitations.length > 0 ? invitations[0] : null;
  },

  /**
   * @return {string} The text show show on the "accept" button in the
   *     invitation promo. 'Accept', 'Accept for group', or empty if there is no
   *     invitation.
   * @private
   */
  getAcceptButtonText_: function() {
    if (!this.invitation_)
      return '';

    return this.invitation_.asGroupManager ? this.i18n('acceptForGroup') :
                                             this.i18n('accept');
  },

  /**
   * @return {string} The formatted text to show for the invitation promo.
   * @private
   */
  getInvitationText_: function() {
    if (!this.invitation_)
      return '';

    if (this.invitation_.asGroupManager) {
      return this.i18nAdvanced('groupPrinterSharingInviteText', {
        substitutions: [
          this.invitation_.sender, this.invitation_.destination.displayName,
          this.invitation_.receiver
        ]
      });
    }

    return this.i18nAdvanced('printerSharingInviteText', {
      substitutions:
          [this.invitation_.sender, this.invitation_.destination.displayName]
    });
  },

  /** @private */
  onInvitationAcceptClick_: function() {
    this.invitationStore.processInvitation(assert(this.invitation_), true);
    this.updateInvitations_();
  },

  /** @private */
  onInvitationRejectClick_: function() {
    this.invitationStore.processInvitation(assert(this.invitation_), false);
    this.updateInvitations_();
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
      this.invitationStore.startLoadingInvitations();
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

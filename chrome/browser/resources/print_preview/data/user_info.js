// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview');

/** @enum {number} */
print_preview.CloudPrintState = {
  DISABLED: 0,
  ENABLED: 1,
  SIGNED_IN: 2,
  NOT_SIGNED_IN: 3,
};

(function() {
'use strict';

/**
 * @typedef {{ activeUser: string,
 *             users: (!Array<string> | undefined) }}
 */
let UpdateUsersPayload;

Polymer({
  is: 'print-preview-user-info',

  properties: {
    activeUser: {
      type: String,
      notify: true,
    },

    /** @type {?print_preview.DestinationStore} */
    destinationStore: Object,

    /** @type {?print_preview.InvitationStore} */
    invitationStore: Object,

    /** @type {!Array<string>} */
    users: {
      type: Array,
      notify: true,
      value: function() {
        return [];
      },
    },
  },

  /** @private {!EventTracker} */
  tracker_: new EventTracker(),

  /** @override */
  detached: function() {
    this.tracker_.removeAll();
  },

  /** @param {!cloudprint.CloudPrintInterface} cloudPrintInterface */
  setCloudPrintInterface: function(cloudPrintInterface) {
    this.tracker_.add(
        cloudPrintInterface.getEventTarget(),
        cloudprint.CloudPrintInterfaceEventType.UPDATE_USERS,
        this.onCloudPrintUpdateUsers_.bind(this));
  },

  /**
   * @param {!CustomEvent<!UpdateUsersPayload>} e Event containing the new
   *     active user and users.
   * @private
   */
  onCloudPrintUpdateUsers_: function(e) {
    this.updateActiveUser(e.detail.activeUser, false);
    if (e.detail.users) {
      this.updateUsers(e.detail.users);
    }
  },

  /** @param {!Array<string>} users The full list of signed in users. */
  updateUsers: function(users) {
    this.users = users;
  },

  /**
   * @param {string} user The new active user.
   * @param {boolean} reloadCookies Whether to reload cookie based destinations
   *    and invitations.
   */
  updateActiveUser: function(user, reloadCookies) {
    this.destinationStore.setActiveUser(user);
    this.activeUser = user;
    if (!reloadCookies) {
      return;
    }

    this.destinationStore.reloadUserCookieBasedDestinations(user);
    this.invitationStore.startLoadingInvitations(user);
  },
});
})();

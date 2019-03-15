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
      // The initialization below is needed only in Polymer 1, to allow
      // observers to fire.
      // TODO (rbpotter): Remove when migration to Polymer 2 is complete.
      value: '',
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

  /** @param {!cloudprint.CloudPrintInterface} cloudPrintInterface */
  setCloudPrintInterface: function(cloudPrintInterface) {
    this.tracker_.add(
        cloudPrintInterface.getEventTarget(),
        cloudprint.CloudPrintInterfaceEventType.UPDATE_USERS,
        this.updateUsers_.bind(this));
  },

  /**
   * @param {string} user The new active user.
   * @private
   */
  setActiveUser_: function(user) {
    this.destinationStore.setActiveUser(user);
    this.activeUser = user;
  },

  /**
   * @param {!CustomEvent<!UpdateUsersPayload>} e Event containing the new
   *     active user and users.
   * @private
   */
  updateUsers_: function(e) {
    this.setActiveUser_(e.detail.activeUser);
    if (e.detail.users) {
      this.users = e.detail.users;
    }
  },

  /** @param {string} user The new active user. */
  updateActiveUser: function(user) {
    this.setActiveUser_(user);
    this.destinationStore.reloadUserCookieBasedDestinations(user);
    this.invitationStore.startLoadingInvitations(user);
  },
});
})();

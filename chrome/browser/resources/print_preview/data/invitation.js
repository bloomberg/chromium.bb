// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Printer sharing invitation data object.
   * @param {string} sender Text identifying invitation sender.
   * @param {string} receiver Text identifying invitation receiver. Empty in
   *     case of a personal invitation. Identifies a group or domain in case
   *     of an invitation received by a group manager.
   * @param {!print_preview.Destination} destination Shared destination.
   * @param {!Object} aclEntry JSON representation of the ACL entry this
   *     invitation was sent to.
   * @param {string} account User account this invitation is sent for.
   * @constructor
   */
  function Invitation(sender, receiver, destination, aclEntry, account) {
    /**
     * Text identifying invitation sender.
     * @private {string}
     */
    this.sender_ = sender;

    /**
     * Text identifying invitation receiver. Empty in case of a personal
     * invitation. Identifies a group or domain in case of an invitation
     * received by a group manager.
     * @private {string}
     */
    this.receiver_ = receiver;

    /**
     * Shared destination.
     * @private {!print_preview.Destination}
     */
    this.destination_ = destination;

    /**
     * JSON representation of the ACL entry this invitation was sent to.
     * @private {!Object}
     */
    this.aclEntry_ = aclEntry;

    /**
     * Account this invitation is sent for.
     * @private {string}
     */
    this.account_ = account;
  }

  Invitation.prototype = {
    /** @return {string} Text identifying invitation sender. */
    get sender() {
      return this.sender_;
    },

    /** @return {string} Text identifying invitation receiver. */
    get receiver() {
      return this.receiver_;
    },

    /**
     * @return {boolean} Whether this user acts as a manager for a group of
     * users.
     */
    get asGroupManager() {
      return !!this.receiver_;
    },

    /** @return {!print_preview.Destination} Shared destination. */
    get destination() {
      return this.destination_;
    },

    /** @return {string} Scope (account) this invitation was sent to. */
    get scopeId() {
      return this.aclEntry_['scope'] || '';
    },

    /** @return {string} Account this invitation is sent for. */
    get account() {
      return this.account_;
    }
  };

  // Export
  return {Invitation: Invitation};
});

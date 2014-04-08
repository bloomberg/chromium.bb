// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Repository which stores information about the user. Events are dispatched
   * when the information changes.
   * @constructor
   * @extends {cr.EventTarget}
   */
  function UserInfo() {
    cr.EventTarget.call(this);

    /**
     * Email address of the logged in user or {@code null} if no user is logged
     * in. In case of Google multilogin, can be changed by the user.
     * @private {?string}
     */
    this.activeUser_ = null;

    /**
     * Email addresses of the logged in users or empty array if no user is
     * logged in.
     * @private {!Array.<string>}
     */
    this.users_ = [];
  };

  /**
   * Enumeration of event types dispatched by the user info.
   * @enum {string}
   */
  UserInfo.EventType = {
    ACTIVE_USER_CHANGED: 'print_preview.UserInfo.ACTIVE_USER_CHANGED',
    USERS_CHANGED: 'print_preview.UserInfo.USERS_CHANGED'
  };

  UserInfo.prototype = {
    __proto__: cr.EventTarget.prototype,

    /**
     * @return {?string} Email address of the logged in user or {@code null} if
     *     no user is logged.
     */
    get activeUser() {
      return this.activeUser_;
    },

    /** Changes active user. */
    set activeUser(activeUser) {
      if (this.activeUser_ != activeUser) {
        this.activeUser_ = activeUser;
        cr.dispatchSimpleEvent(this, UserInfo.EventType.ACTIVE_USER_CHANGED);
      }
    },


    /**
     * @return {!Array.<string>} Email addresses of the logged in users or
     *     empty array if no user is logged in.
     */
    get users() {
      return this.users_;
    },

    /**
     * Sets logged in user accounts info.
     * @param {string} user Currently logged in user (email).
     * @param {!Array.<string>} users List of currently logged in accounts.
     */
    setUsers: function(activeUser, users) {
      this.activeUser_ = activeUser;
      this.users_ = users || [];
      cr.dispatchSimpleEvent(this, UserInfo.EventType.USERS_CHANGED);
    },
  };

  return {
    UserInfo: UserInfo
  };
});

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
     * Tracker used to keep track of event listeners.
     * @type {!EventTracker}
     * @private
     */
    this.tracker_ = new EventTracker();

    /**
     * Email address of the logged in user or {@code null} if no user is logged
     * in.
     * @type {?string}
     * @private
     */
    this.userEmail_ = null;
  };

  /**
   * Enumeration of event types dispatched by the user info.
   * @enum {string}
   */
  UserInfo.EventType = {
    EMIAL_CHANGE: 'print_preview.UserInfo.EMAIL_CHANGE'
  };

  UserInfo.prototype = {
    __proto__: cr.EventTarget.prototype,

    /**
     * @return {?string} Email address of the logged in user or {@code null} if
     *     no user is logged.
     */
    getUserEmail: function() {
      return this.userEmail_;
    },

    /**
     * @param {!cloudprint.CloudPrintInterface} cloudPrintInterface Interface
     *     to Google Cloud Print that the print preview uses.
     */
    setCloudPrintInterface: function(cloudPrintInterface) {
      this.tracker_.add(
          cloudPrintInterface,
          cloudprint.CloudPrintInterface.EventType.SEARCH_DONE,
          this.onCloudPrintSearchDone_.bind(this));
    },

    /** Removes all event listeners. */
    removeEventListeners: function() {
      this.tracker_.removeAll();
    },

    /**
     * Called when a Google Cloud Print printer search completes. Updates user
     * information.
     * @type {cr.Event} event Contains information about the logged in user.
     * @private
     */
    onCloudPrintSearchDone_: function(event) {
      if (event.origin == print_preview.Destination.Origin.COOKIES) {
        this.userEmail_ = event.email;
        cr.dispatchSimpleEvent(this, UserInfo.EventType.EMAIL_CHANGE);
      }
    }
  };

  return {
    UserInfo: UserInfo
  };
});

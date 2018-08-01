// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * A mock chrome.notifications API for tests.
 * @constructor.
 */
let MockNotifications = function() {
  /** @type {NotificationOptions} */
  this.options;

  /** @type {function<NotificationOptions> */
  this.callback_;
};

MockNotifications.prototype = {
  /**
   * Mock notification create function.
   * @param {string} notificationId
   * @param {NotificationOptions} options
   * @param {function=} opt_callback This is not used by STS so it's not mocked.
   */
  create: function(notificationId, options, opt_callback) {
    this.options_ = options;

    // Call the test listener.
    this.callback_ && this.callback_(options);
  },

  /**
   * Stub onButtonClicked.
   */
  onButtonClicked: {
    addListener: function(callback) {
      // Unimplemented
    },
  },

  /**
   * For testing, not part of API.
   * @param {function(NotificationOptions)} A callback for the set options.
   */
  onNotificationCreated: function(callback) {
    this.callback_ = callback;
  },

  /**
   * For testing, not part of API.
   * @return {NotificationOptions} The most recently set notification options.
   */
  getOptions: function() {
    return this.options_;
  },
};
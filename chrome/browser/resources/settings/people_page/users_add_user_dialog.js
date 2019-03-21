// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-users-add-user-dialog' is the dialog shown for adding new allowed
 * users to a ChromeOS device.
 */
(function() {

/**
 * Regular expression for adding a user where the string provided is just
 * the part before the "@".
 * Email alias only, assuming it's a gmail address.
 *     e.g. 'john'
 * @type {!RegExp}
 */
const NAME_ONLY_REGEX =
    new RegExp('^\\s*([\\w\\.!#\\$%&\'\\*\\+-\\/=\\?\\^`\\{\\|\\}~]+)\\s*$');

/**
 * Regular expression for adding a user where the string provided is a full
 * email address.
 *     e.g. 'john@chromium.org'
 * @type {!RegExp}
 */
const EMAIL_REGEX = new RegExp(
    '^\\s*([\\w\\.!#\\$%&\'\\*\\+-\\/=\\?\\^`\\{\\|\\}~]+)@' +
    '([A-Za-z0-9\-]{2,63}\\..+)\\s*$');

Polymer({
  is: 'settings-users-add-user-dialog',

  properties: {
    /** @private */
    isEmail_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    isEmpty_: {
      type: Boolean,
      value: true,
    },
  },

  open: function() {
    this.$.addUserInput.value = '';
    this.onInput_();
    this.$.dialog.showModal();
    // Set to valid initially since the user has not typed anything yet.
    this.$.addUserInput.invalid = false;
  },

  /** @private */
  addUser_: function() {
    // May be submitted by the Enter key even if the input value is invalid.
    if (this.$.addUserInput.disabled) {
      return;
    }

    const input = this.$.addUserInput.value;

    const nameOnlyMatches = NAME_ONLY_REGEX.exec(input);
    let userEmail;
    if (nameOnlyMatches) {
      userEmail = nameOnlyMatches[1] + '@gmail.com';
    } else {
      const emailMatches = EMAIL_REGEX.exec(input);
      // Assuming the input validated, one of these two must match.
      assert(emailMatches);
      userEmail = emailMatches[1] + '@' + emailMatches[2];
    }

    chrome.usersPrivate.addWhitelistedUser(
        userEmail,
        /* callback */ function(success) {});
    this.$.addUserInput.value = '';
    this.$.dialog.close();
  },

  /**
   * @return {boolean}
   * @private
   */
  canAddUser_: function() {
    return this.isEmail_ && !this.isEmpty_;
  },

  /** @private */
  onCancelTap_: function() {
    this.$.dialog.cancel();
  },

  /** @private */
  onInput_: function() {
    const input = this.$.addUserInput.value;
    this.isEmail_ = NAME_ONLY_REGEX.test(input) || EMAIL_REGEX.test(input);
    this.isEmpty_ = input.length == 0;
  },

  /**
   * @private
   * @return {boolean}
   */
  shouldShowError_: function() {
    return !this.isEmail_ && !this.isEmpty_;
  },
});

})();

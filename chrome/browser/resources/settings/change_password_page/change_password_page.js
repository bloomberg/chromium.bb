// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'change-password-page' is the settings page containing change password
 * settings.
 */
Polymer({
  is: 'settings-change-password-page',

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },
  },

  /** @private {?settings.ChangePasswordBrowserProxy} */
  browserProxy_: null,

  /** @override */
  attached: function() {
    this.browserProxy_ = settings.ChangePasswordBrowserProxyImpl.getInstance();
    this.browserProxy_.onChangePasswordPageShown();
  },

  /** @private */
  changePassword_: function() {
    listenOnce(this, 'transitionend', () => {
      this.browserProxy_.changePassword();
    });
    this.fire('change-password-clicked');
  },
});

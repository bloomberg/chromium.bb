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

  behaviors: [WebUIListenerBehavior],

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
    this.addWebUIListener(
        'change-password-on-dismiss', this.onDismiss_.bind(this));
  },

  /** @private */
  changePassword_: function() {
    listenOnce(this, 'transitionend', () => {
      this.browserProxy_.changePassword();
    });
  },

  /**
   * Listener of event "change-password-on-dismiss".
   * This function hides the change password card.
   * @private
   */
  onDismiss_: function() {
    this.fire('change-password-dismissed');
  }
});

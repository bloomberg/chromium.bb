// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-startup-url-dialog' is a component for adding
 * or editing a startup URL entry.
 */
Polymer({
  is: 'settings-startup-url-dialog',

  properties: {
    /** @private {string} */
    url_: String,
  },

  /** @private {!settings.SearchEnginesBrowserProxy} */
  browserProxy_: null,

  /** @override */
  ready: function() {
    this.browserProxy_ = settings.StartupUrlsPageBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached: function() {
    this.$.dialog.open();
  },

  /** @private */
  onCancelTap_: function() {
    this.$.dialog.close();
  },

  /** @private */
  onAddTap_: function() {
    this.browserProxy_.addStartupPage(this.url_).then(function(success) {
      if (success)
        this.$.dialog.close();
      // If the URL was invalid, there is nothing to do, just leave the dialog
      // open and let the user fix the URL or cancel.
    }.bind(this));
  },

  /** @private */
  validate_: function() {
    this.browserProxy_.validateStartupPage(this.url_).then(function(isValid) {
      this.$.add.disabled = !isValid;
    }.bind(this));
  },
});

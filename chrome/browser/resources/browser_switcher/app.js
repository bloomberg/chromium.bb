// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(function() {

/** @enum {string} */
const LaunchError = {
  GENERIC_ERROR: 'genericError',
  PROTOCOL_ERROR: 'protocolError',
};

Polymer({
  is: 'browser-switcher-app',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * URL to launch in the alternative browser.
     * @private
     */
    url_: {
      type: String,
      value: function() {
        return (new URLSearchParams(window.location.search)).get('url') || '';
      },
    },

    /**
     * Error message, or empty string if no error has occurred (yet).
     * @private
     */
    error_: {
      type: String,
      value: '',
    },
  },

  /** @override */
  attached: function() {
    const proxy = browser_switcher.BrowserSwitcherProxyImpl.getInstance();

    // If '?done=...' is specified in the URL, this tab was-reopened, or the
    // entire browser was closed by LBS and re-opened. In that case, go to NTP
    // instead.
    const done = (new URLSearchParams(window.location.search)).has('done');
    if (done) {
      proxy.gotoNewTabPage();
      return;
    }

    // Sanity check the URL to make sure nothing really funky is going on.
    const anchor = document.createElement('a');
    anchor.href = this.url_;
    if (!/^(file|http|https):$/.test(anchor.protocol)) {
      this.error_ = LaunchError.PROTOCOL_ERROR;
      return;
    }

    // Mark this page with '?done=...' so that restoring the tab doesn't
    // immediately re-trigger LBS.
    history.pushState({}, '', '/?done=true');

    proxy.launchAlternativeBrowserAndCloseTab(this.url_).catch(() => {
      this.error_ = LaunchError.GENERIC_ERROR;
    });
  },

  /**
   * @return {string}
   * @private
   */
  computeTitle_: function() {
    if (this.error_) {
      return this.i18n('errorTitle');
    }
    return this.i18n('openingTitle');
  },

  /**
   * @return {string}
   * @private
   */
  computeDescription_: function() {
    if (this.error_) {
      return this.i18n(this.error_, getUrlHostname(this.url_));
    }
    return this.i18n('description');
  },
});

function getUrlHostname(url) {
  const anchor = document.createElement('a');
  anchor.href = url;
  // Return entire url if parsing failed (which means the URL is bogus).
  return anchor.hostname || url;
}
})();

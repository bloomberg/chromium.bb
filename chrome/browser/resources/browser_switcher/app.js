// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(function() {

Polymer({
  is: 'browser-switcher-app',

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
  },

  /** @override */
  attached: function() {
    // Sanity check the URL to make sure nothing really funky is going on.
    const anchor = document.createElement('a');
    anchor.href = this.url_;
    if (!/^(file|http|https):$/.test(anchor.protocol)) {
      return;
    }

    const proxy = browser_switcher.BrowserSwitcherProxyImpl.getInstance();
    proxy.launchAlternativeBrowserAndCloseTab(this.url_);
  },
});
})();

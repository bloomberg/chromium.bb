// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the People section to interact with
 * the browser.
 */

cr.exportPath('settings');

cr.define('settings', function() {
  /** @interface */
  class PeopleBrowserProxy {
    // TODO(dpapad): Create a simple OpenWindowProxy class that replaces the
    // need for this method and similar methods in other BrowserProxy classes.
    /**
     * Opens the specified URL in a new tab.
     * @param {string} url
     */
    openURL(url) {}
  }

  /** @implements {settings.PeopleBrowserProxy} */
  class PeopleBrowserProxyImpl {
    /** @override */
    openURL(url) {
      window.open(url);
    }
  }

  cr.addSingletonGetter(PeopleBrowserProxyImpl);

  return {
    PeopleBrowserProxy: PeopleBrowserProxy,
    PeopleBrowserProxyImpl: PeopleBrowserProxyImpl,
  };
});

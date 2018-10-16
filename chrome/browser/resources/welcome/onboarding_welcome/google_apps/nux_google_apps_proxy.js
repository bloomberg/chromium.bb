// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('nux', function() {
  /** @interface */
  class NuxGoogleAppsProxy {
    /**
     * Adds the selected apps to the bookmark bar.
     * @param {!Array<boolean>} selectedApps
     */
    addGoogleApps(selectedApps) {}

    /**
     * Returns a promise for an array of Google apps.
     * @return {!Promise<!Array<!nux.BookmarkListItem>>}
     */
    getGoogleAppsList() {}
  }

  /** @implements {nux.NuxGoogleAppsProxy} */
  class NuxGoogleAppsProxyImpl {
    /** @override */
    addGoogleApps(selectedApps) {
      chrome.send('addGoogleApps', selectedApps);
    }

    /** @override */
    getGoogleAppsList() {
      return cr.sendWithPromise('getGoogleAppsList');
    }
  }

  cr.addSingletonGetter(NuxGoogleAppsProxyImpl);

  return {
    NuxGoogleAppsProxy: NuxGoogleAppsProxy,
    NuxGoogleAppsProxyImpl: NuxGoogleAppsProxyImpl,
  };
});
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /** @interface */
  function StartupUrlsPageBrowserProxy() {}

  StartupUrlsPageBrowserProxy.prototype = {
    loadStartupPages: assertNotReached,

    useCurrentPages: assertNotReached,

    /** @param {string} url */
    canAddPage: assertNotReached,

    /** @param {string} url */
    addStartupPage: assertNotReached,

    /** @param {number} index */
    removeStartupPage: assertNotReached,
  };

  /**
   * @implements {settings.StartupUrlsPageBrowserProxy}
   * @constructor
   */
  function StartupUrlsPageBrowserProxyImpl() {}

  cr.addSingletonGetter(StartupUrlsPageBrowserProxyImpl);

  StartupUrlsPageBrowserProxyImpl.prototype = {
    loadStartupPages: function() {
      chrome.send('onStartupPrefsPageLoad');
    },

    useCurrentPages: function() {
      chrome.send('setStartupPagesToCurrentPages');
    },

    canAddPage: function(url) {
      // TODO(dbeam): hook up to C++ for deeper validation.
      return url.trim().length > 0;
    },

    addStartupPage: function(url) {
      chrome.send('addStartupPage', [url.trim()]);
    },

    removeStartupPage: function(index) {
      chrome.send('removeStartupPage', [index]);
    },
  };

  return {
    StartupUrlsPageBrowserProxy: StartupUrlsPageBrowserProxy,
    StartupUrlsPageBrowserProxyImpl: StartupUrlsPageBrowserProxyImpl,
  };
});

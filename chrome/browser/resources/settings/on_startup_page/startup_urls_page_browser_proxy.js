// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /** @interface */
  function StartupUrlsPageBrowserProxy() {}

  StartupUrlsPageBrowserProxy.prototype = {
    loadStartupPages: assertNotReached,

    useCurrentPages: assertNotReached,

    /**
     * @param {string} url
     * @return {!PromiseResolver<boolean>} Whether the URL is valid.
     */
    validateStartupPage: assertNotReached,

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
    /** @override */
    loadStartupPages: function() {
      chrome.send('onStartupPrefsPageLoad');
    },

    /** @override */
    useCurrentPages: function() {
      chrome.send('setStartupPagesToCurrentPages');
    },

    /** @override */
    validateStartupPage: function(url) {
      var resolver = new PromiseResolver();
      resolver.promise = url.trim().length == 0 ? Promise.resolve(false) :
          cr.sendWithPromise('validateStartupPage', url);
      return resolver;
    },

    /** @override */
    addStartupPage: function(url) {
      chrome.send('addStartupPage', [url.trim()]);
    },

    /** @override */
    removeStartupPage: function(index) {
      chrome.send('removeStartupPage', [index]);
    },
  };

  return {
    StartupUrlsPageBrowserProxy: StartupUrlsPageBrowserProxy,
    StartupUrlsPageBrowserProxyImpl: StartupUrlsPageBrowserProxyImpl,
  };
});

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /** @interface */
  function DownloadsBrowserProxy() {}

  DownloadsBrowserProxy.prototype = {
    initializeDownloads: assertNotReached,

    selectDownloadLocation: assertNotReached,

    resetAutoOpenFileTypes: assertNotReached,
  };

  /**
   * @implements {settings.DownloadsBrowserProxy}
   * @constructor
   */
  function DownloadsBrowserProxyImpl() {}

  cr.addSingletonGetter(DownloadsBrowserProxyImpl);

  DownloadsBrowserProxyImpl.prototype = {
    /** @override */
    initializeDownloads: function() {
      chrome.send('initializeDownloads');
    },

    /** @override */
    selectDownloadLocation: function() {
      chrome.send('selectDownloadLocation');
    },

    /** @override */
    resetAutoOpenFileTypes: function() {
      chrome.send('resetAutoOpenFileTypes');
    },
  };

  return {
    DownloadsBrowserProxy: DownloadsBrowserProxy,
    DownloadsBrowserProxyImpl: DownloadsBrowserProxyImpl,
  };
});

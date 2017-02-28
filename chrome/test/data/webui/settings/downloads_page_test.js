// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @implements {settings.DownloadsBrowserProxy}
 * @extends {settings.TestBrowserProxy}
 */
var TestDownloadsBrowserProxy = function() {
  settings.TestBrowserProxy.call(this, [
    'initializeDownloads',
    'selectDownloadLocation',
    'resetAutoOpenFileTypes',
  ]);
};

TestDownloadsBrowserProxy.prototype = {
  __proto__: settings.TestBrowserProxy.prototype,

  /** @override */
  initializeDownloads: function() {
    this.methodCalled('initializeDownloads');
  },

  /** @override */
  selectDownloadLocation: function() {
    this.methodCalled('selectDownloadLocation');
  },

  /** @override */
  resetAutoOpenFileTypes: function() {
    this.methodCalled('resetAutoOpenFileTypes');
  },
};

var downloadsPage = null;

/** @type {?TestDownloadsBrowserProxy} */
var DownloadsBrowserProxy = null;

suite('DownloadsHandler', function() {
  setup(function() {
    DownloadsBrowserProxy = new TestDownloadsBrowserProxy();
    settings.DownloadsBrowserProxyImpl.instance_ = DownloadsBrowserProxy;

    PolymerTest.clearBody();

    downloadsPage = document.createElement('settings-downloads-page');
    document.body.appendChild(downloadsPage);

    // Page element must call 'initializeDownloads' upon attachment to the DOM.
    return DownloadsBrowserProxy.whenCalled('initializeDownloads');
  });

  teardown(function() {
    downloadsPage.remove();
  });

  test('select downloads location', function() {
    var button = downloadsPage.$$('#changeDownloadsPath');
    assertTrue(!!button);
    MockInteractions.tap(button);
    return DownloadsBrowserProxy.whenCalled('selectDownloadLocation');
  });

  test('openAdvancedDownloadsettings', function() {
    var button = downloadsPage.$$('#resetAutoOpenFileTypes');
    assertTrue(!button);

    cr.webUIListenerCallback('auto-open-downloads-changed', true);
    Polymer.dom.flush();
    var button = downloadsPage.$$('#resetAutoOpenFileTypes');
    assertTrue(!!button);

    MockInteractions.tap(button);
    return DownloadsBrowserProxy.whenCalled('resetAutoOpenFileTypes')
        .then(function() {
          cr.webUIListenerCallback('auto-open-downloads-changed', false);
          Polymer.dom.flush();
          var button = downloadsPage.$$('#resetAutoOpenFileTypes');
          assertTrue(!button);
        });
  });
});

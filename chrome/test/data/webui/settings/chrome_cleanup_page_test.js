// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @implements {settings.ChromeCleanupProxy}
 * @extends {TestBrowserProxy}
 */
var TestChromeCleanupProxy = function() {
  TestBrowserProxy.call(this, [
    'dismissCleanupPage',
    'registerChromeCleanerObserver',
    'restartComputer',
    'startCleanup',
  ]);
};

TestChromeCleanupProxy.prototype = {
  __proto__: TestBrowserProxy.prototype,

  /** @override */
  dismissCleanupPage: function() {
    this.methodCalled('dismissCleanupPage');
  },

  /** @override */
  registerChromeCleanerObserver: function() {
    this.methodCalled('registerChromeCleanerObserver');
  },

  /** @override */
  restartComputer: function() {
    this.methodCalled('restartComputer');
  },

  /** @override */
  startCleanup: function() {
    this.methodCalled('startCleanup');
  },
};

var chromeCleanupPage = null;

/** @type {?TestDownloadsBrowserProxy} */
var ChromeCleanupProxy = null;

suite('ChromeCleanupHandler', function() {
  setup(function() {
    ChromeCleanupProxy = new TestChromeCleanupProxy();
    settings.ChromeCleanupProxyImpl.instance_ = ChromeCleanupProxy;

    PolymerTest.clearBody();

    chromeCleanupPage = document.createElement('settings-chrome-cleanup-page');
    document.body.appendChild(chromeCleanupPage);
  });

  teardown(function() {
    chromeCleanupPage.remove();
  });

  test('startCleanupFromInfected', function() {
    cr.webUIListenerCallback(
        'chrome-cleanup-on-infected', ['file 1', 'file 2', 'file 3']);
    Polymer.dom.flush();

    var showFilesButton = chromeCleanupPage.$$('#show-files-button');
    assertTrue(!!showFilesButton);
    MockInteractions.tap(showFilesButton);
    filesToRemove = chromeCleanupPage.$$('#files-to-remove-list');
    assertTrue(!!filesToRemove);
    assertEquals(filesToRemove.getElementsByTagName('li').length, 3);

    var actionButton = chromeCleanupPage.$$('#action-button');
    assertTrue(!!actionButton);
    MockInteractions.tap(actionButton);
    ChromeCleanupProxy.whenCalled('startCleanup').then(function() {
      cr.webUIListenerCallback('chrome-cleanup-on-cleaning', false);
      Polymer.dom.flush();

      var spinner = chromeCleanupPage.$$('#cleaning-spinner');
      assertTrue(spinner.active);
    })
  });

  test('rebootFromRebootRequired', function() {
    cr.webUIListenerCallback('chrome-cleanup-on-reboot-required');
    Polymer.dom.flush();

    var actionButton = chromeCleanupPage.$$('#action-button');
    assertTrue(!!actionButton);
    MockInteractions.tap(actionButton);
    return ChromeCleanupProxy.whenCalled('restartComputer');
  });

  test('dismissFromIdleCleaningFailure', function() {
    cr.webUIListenerCallback('chrome-cleanup-on-cleaning', false);
    cr.webUIListenerCallback(
        'chrome-cleanup-on-idle',
        settings.ChromeCleanupIdleReason.CLEANING_FAILED);
    Polymer.dom.flush();

    var actionButton = chromeCleanupPage.$$('#action-button');
    assertTrue(!!actionButton);
    MockInteractions.tap(actionButton);
    return ChromeCleanupProxy.whenCalled('dismissCleanupPage');
  });

  test('dismissFromIdleCleaningSuccess', function() {
    cr.webUIListenerCallback('chrome-cleanup-on-cleaning', false);
    cr.webUIListenerCallback(
        'chrome-cleanup-on-idle',
        settings.ChromeCleanupIdleReason.CLEANING_SUCCEEDED);
    Polymer.dom.flush();

    var actionButton = chromeCleanupPage.$$('#action-button');
    assertTrue(!!actionButton);
    MockInteractions.tap(actionButton);
    return ChromeCleanupProxy.whenCalled('dismissCleanupPage');
  });
});

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {settings.ChromeCleanupProxy} */
class TestChromeCleanupProxy extends TestBrowserProxy {
  constructor() {
    super([
      'dismissCleanupPage',
      'registerChromeCleanerObserver',
      'restartComputer',
      'setLogsUploadPermission',
      'startCleanup',
      'notifyShowDetails',
      'notifyLearnMoreClicked',
    ]);
  }

  /** @override */
  dismissCleanupPage(source) {
    this.methodCalled('dismissCleanupPage', source);
  }

  /** @override */
  registerChromeCleanerObserver() {
    this.methodCalled('registerChromeCleanerObserver');
  }

  /** @override */
  restartComputer() {
    this.methodCalled('restartComputer');
  }

  /** @override */
  setLogsUploadPermission(enabled) {
    this.methodCalled('setLogsUploadPermission', enabled);
  }

  /** @override */
  startCleanup(logsUploadEnabled) {
    this.methodCalled('startCleanup', logsUploadEnabled);
  }

  /** @override */
  notifyShowDetails(enabled) {
    this.methodCalled('notifyShowDetails', enabled);
  }

  /** @override */
  notifyLearnMoreClicked() {
    this.methodCalled('notifyLearnMoreClicked');
  }
}

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
    cr.webUIListenerCallback('chrome-cleanup-upload-permission-change', false);
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
    ChromeCleanupProxy.whenCalled('startCleanup').then(
      function(logsUploadEnabled) {
        assertFalse(logsUploadEnabled);
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

  test('setLogsUploadPermission', function() {
    cr.webUIListenerCallback(
        'chrome-cleanup-on-infected', ['file 1', 'file 2', 'file 3']);
    Polymer.dom.flush();

    var control = chromeCleanupPage.$$('#chromeCleanupLogsUploadControl');
    assertTrue(!!control);

    cr.webUIListenerCallback('chrome-cleanup-upload-permission-change', true);
    Polymer.dom.flush();
    assertTrue(control.checked);

    cr.webUIListenerCallback('chrome-cleanup-upload-permission-change', false);
    Polymer.dom.flush();
    assertFalse(control.checked);

    // TODO(proberge): Mock tapping on |control| and verify that
    // |setLogsUploadPermission| is called with the right argument.
  });
});

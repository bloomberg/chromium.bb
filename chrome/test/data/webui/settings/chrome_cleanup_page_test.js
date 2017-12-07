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
      'startScanning',
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
  startScanning(logsUploadEnabled) {
    this.methodCalled('startScanning', logsUploadEnabled);
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

function initParametrizedTest(userInitiatedCleanupsEnabled) {
  ChromeCleanupProxy = new TestChromeCleanupProxy();
  settings.ChromeCleanupProxyImpl.instance_ = ChromeCleanupProxy;

  PolymerTest.clearBody();

  loadTimeData.overrideValues({
    userInitiatedCleanupsEnabled: userInitiatedCleanupsEnabled,
  });

  chromeCleanupPage = document.createElement('settings-chrome-cleanup-page');
  document.body.appendChild(chromeCleanupPage);
}

function startCleanupFromInfected() {
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
  return ChromeCleanupProxy.whenCalled('startCleanup')
      .then(function(logsUploadEnabled) {
        assertFalse(logsUploadEnabled);
        cr.webUIListenerCallback('chrome-cleanup-on-cleaning', false);
        Polymer.dom.flush();

        var spinner = chromeCleanupPage.$$('#waiting-spinner');
        assertTrue(spinner.active);
      });
}

function rebootFromRebootRequired() {
  cr.webUIListenerCallback('chrome-cleanup-on-reboot-required');
  Polymer.dom.flush();

  var actionButton = chromeCleanupPage.$$('#action-button');
  assertTrue(!!actionButton);
  MockInteractions.tap(actionButton);
  return ChromeCleanupProxy.whenCalled('restartComputer');
}

function cleanupFailure(userInitiatedCleanupsEnabled) {
  cr.webUIListenerCallback('chrome-cleanup-upload-permission-change', false);
  cr.webUIListenerCallback('chrome-cleanup-on-cleaning', false);
  cr.webUIListenerCallback(
      'chrome-cleanup-on-idle',
      settings.ChromeCleanupIdleReason.CLEANING_FAILED);
  Polymer.dom.flush();

  var actionButton = chromeCleanupPage.$$('#action-button');
  if (userInitiatedCleanupsEnabled) {
    assertFalse(!!actionButton);
  } else {
    assertTrue(!!actionButton);
    MockInteractions.tap(actionButton);
    return ChromeCleanupProxy.whenCalled('dismissCleanupPage');
  }
}

function cleanupSuccess(userInitiatedCleanupsEnabled) {
  cr.webUIListenerCallback('chrome-cleanup-on-cleaning', false);
  cr.webUIListenerCallback(
      'chrome-cleanup-on-idle',
      settings.ChromeCleanupIdleReason.CLEANING_SUCCEEDED);
  Polymer.dom.flush();

  var actionButton = chromeCleanupPage.$$('#action-button');
  if (userInitiatedCleanupsEnabled) {
    assertFalse(!!actionButton);
  } else {
    assertTrue(!!actionButton);
    MockInteractions.tap(actionButton);
    return ChromeCleanupProxy.whenCalled('dismissCleanupPage');
  }
}

function testLogsUploading(testingScanOffered) {
  if (testingScanOffered) {
    cr.webUIListenerCallback(
        'chrome-cleanup-on-infected', ['file 1', 'file 2', 'file 3']);
  } else {
    cr.webUIListenerCallback(
        'chrome-cleanup-on-idle', settings.ChromeCleanupIdleReason.INITIAL);
  }
  Polymer.dom.flush();

  var logsControl = chromeCleanupPage.$$('#chromeCleanupLogsUploadControl');
  assertTrue(!!logsControl);

  cr.webUIListenerCallback('chrome-cleanup-upload-permission-change', true);
  Polymer.dom.flush();
  assertTrue(logsControl.checked);

  cr.webUIListenerCallback('chrome-cleanup-upload-permission-change', false);
  Polymer.dom.flush();
  assertFalse(logsControl.checked);

  MockInteractions.tap(logsControl.$.control);
  return ChromeCleanupProxy.whenCalled('setLogsUploadPermission')
      .then(function(logsUploadEnabled) {
        assertTrue(logsUploadEnabled);
      });
}

suite('ChromeCleanupHandler_UserInitiatedCleanupsDisabled', function() {
  setup(function() {
    initParametrizedTest(false);
  });

  test('startCleanupFromInfected', function() {
    return startCleanupFromInfected();
  });

  test('rebootFromRebootRequired', function() {
    return rebootFromRebootRequired();
  });

  test('cleanupFailure', function() {
    return cleanupFailure(false);
  });

  test('cleanupSuccess', function() {
    return cleanupSuccess(false);
  });

  test('logsUploadingOnInfected', function() {
    return testLogsUploading(false);
  });
});

suite('ChromeCleanupHandler_UserInitiatedCleanupsEnabled', function() {
  setup(function() {
    initParametrizedTest(true);
  });

  function scanOfferedOnInitiallyIdle(idleReason) {
    cr.webUIListenerCallback('chrome-cleanup-on-idle', idleReason);
    Polymer.dom.flush();

    var actionButton = chromeCleanupPage.$$('#action-button');
    assertTrue(!!actionButton);
  }

  test('scanOfferedOnInitiallyIdle_ReporterFoundNothing', function() {
    scanOfferedOnInitiallyIdle(
        settings.ChromeCleanupIdleReason.REPORTER_FOUND_NOTHING);
  });

  test('scanOfferedOnInitiallyIdle_ReporterFailed', function() {
    scanOfferedOnInitiallyIdle(
        settings.ChromeCleanupIdleReason.REPORTER_FAILED);
  });

  test('scanOfferedOnInitiallyIdle_ScanningFoundNothing', function() {
    scanOfferedOnInitiallyIdle(
        settings.ChromeCleanupIdleReason.SCANNING_FOUND_NOTHING);
  });

  test('scanOfferedOnInitiallyIdle_ScanningFailed', function() {
    scanOfferedOnInitiallyIdle(
        settings.ChromeCleanupIdleReason.SCANNING_FAILED);
  });

  test('scanOfferedOnInitiallyIdle_ConnectionLost', function() {
    scanOfferedOnInitiallyIdle(
        settings.ChromeCleanupIdleReason.CONNECTION_LOST);
  });

  test('scanOfferedOnInitiallyIdle_UserDeclinedCleanup', function() {
    scanOfferedOnInitiallyIdle(
        settings.ChromeCleanupIdleReason.USER_DECLINED_CLEANUP);
  });

  test('scanOfferedOnInitiallyIdle_CleaningFailed', function() {
    scanOfferedOnInitiallyIdle(
        settings.ChromeCleanupIdleReason.CLEANING_FAILED);
  });

  test('scanOfferedOnInitiallyIdle_CleaningSucceeded', function() {
    scanOfferedOnInitiallyIdle(
        settings.ChromeCleanupIdleReason.CLEANING_SUCCEEDED);
  });

  test('reporterFoundNothing', function() {
    cr.webUIListenerCallback('chrome-cleanup-on-reporter-running');
    cr.webUIListenerCallback(
        'chrome-cleanup-on-idle',
        settings.ChromeCleanupIdleReason.REPORTER_FOUND_NOTHING);
    Polymer.dom.flush();

    var actionButton = chromeCleanupPage.$$('#action-button');
    assertFalse(!!actionButton);
  });

  test('reporterFoundNothing', function() {
    cr.webUIListenerCallback('chrome-cleanup-on-reporter-running');
    cr.webUIListenerCallback(
        'chrome-cleanup-on-idle',
        settings.ChromeCleanupIdleReason.REPORTER_FOUND_NOTHING);
    Polymer.dom.flush();

    var actionButton = chromeCleanupPage.$$('#action-button');
    assertFalse(!!actionButton);
  });

  test('startScanFromIdle', function() {
    cr.webUIListenerCallback('chrome-cleanup-upload-permission-change', false);
    cr.webUIListenerCallback(
        'chrome-cleanup-on-idle', settings.ChromeCleanupIdleReason.INITIAL);
    Polymer.dom.flush();

    var actionButton = chromeCleanupPage.$$('#action-button');
    assertTrue(!!actionButton);
    MockInteractions.tap(actionButton);
    return ChromeCleanupProxy.whenCalled('startScanning')
        .then(function(logsUploadEnabled) {
          assertFalse(logsUploadEnabled);
          cr.webUIListenerCallback('chrome-cleanup-on-scanning', false);
          Polymer.dom.flush();

          var spinner = chromeCleanupPage.$$('#waiting-spinner');
          assertTrue(spinner.active);
        });
  });

  test('scanFoundNothing', function() {
    cr.webUIListenerCallback('chrome-cleanup-on-scanning', false);
    cr.webUIListenerCallback(
        'chrome-cleanup-on-idle',
        settings.ChromeCleanupIdleReason.SCANNING_FOUND_NOTHING);
    Polymer.dom.flush();

    var actionButton = chromeCleanupPage.$$('#action-button');
    assertFalse(!!actionButton);
  });

  test('scanFailure', function() {
    cr.webUIListenerCallback('chrome-cleanup-on-scanning', false);
    cr.webUIListenerCallback(
        'chrome-cleanup-on-idle',
        settings.ChromeCleanupIdleReason.SCANNING_FAILED);
    Polymer.dom.flush();

    var actionButton = chromeCleanupPage.$$('#action-button');
    assertFalse(!!actionButton);
  });

  test('startCleanupFromInfected', function() {
    return startCleanupFromInfected();
  });

  test('rebootFromRebootRequired', function() {
    return rebootFromRebootRequired();
  });

  test('cleanupFailure', function() {
    return cleanupFailure(true);
  });

  test('cleanupSuccess', function() {
    return cleanupSuccess(true);
  });

  test('logsUploadingOnScanOffered', function() {
    return testLogsUploading(true);
  });

  test('logsUploadingOnInfected', function() {
    return testLogsUploading(false);
  });
});

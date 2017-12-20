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
      'getMoreItemsPluralString',
      'getItemsToRemovePluralString',
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

  /** @override */
  getMoreItemsPluralString(numHiddenItems) {
    this.methodCalled('getMoreItemsPluralString', numHiddenItems);
    return Promise.resolve('');
  }

  /** @override */
  getItemsToRemovePluralString(numItems) {
    this.methodCalled('getItemsToRemovePluralString', numItems);
    return Promise.resolve('');
  }
}

var chromeCleanupPage = null;

/** @type {?TestDownloadsBrowserProxy} */
var ChromeCleanupProxy = null;

var shortFileList = ['file 1', 'file 2', 'file 3'];
var exactSizeFileList = ['file 1', 'file 2', 'file 3', 'file 4'];
var longFileList = ['file 1', 'file 2', 'file 3', 'file 4', 'file 5'];
var shortRegistryKeysList = ['key 1', 'key 2'];
var exactSizeRegistryKeysList = ['key 1', 'key 2', 'key 3', 'key 4'];
var longRegistryKeysList =
    ['key 1', 'key 2', 'key 3', 'key 4', 'key 5', 'key 6'];

var defaultScannerResults = {
  'files': shortFileList,
  'registryKeys': shortRegistryKeysList,
};

/**
 * @param {boolean} userInitiatedCleanupsEnabled Whether the user initiated
 *     cleanup feature is enabled.
 */
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

/**
 * @param {!Array} originalItems
 * @param {!Array} visibleItems
 * @param {boolean} listCanBeShortened
 */
function validateVisibleItemsList(
    originalItems, visibleItems, listCanBeShortened) {
  var visibleItemsList = visibleItems.querySelectorAll('.visible-item');
  var showMoreItems = visibleItems.querySelector('#show-more-items');

  if (!listCanBeShortened ||
      originalItems.length <= settings.CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW) {
    assertEquals(visibleItemsList.length, originalItems.length);
    assertTrue(showMoreItems.hidden);
  } else {
    assertEquals(
        visibleItemsList.length,
        settings.CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW - 1);
    assertFalse(showMoreItems.hidden);

    // Tapping on the "show more" link should expand the list.
    var link = showMoreItems.querySelector('a');
    MockInteractions.tap(link);
    Polymer.dom.flush();

    visibleItemsList = visibleItems.querySelectorAll('.visible-item');
    assertEquals(visibleItemsList.length, originalItems.length);
    assertTrue(showMoreItems.hidden);
  }
}

/**
 * @param {boolean} userInitiatedCleanupsEnabled Whether the user initiated
 *     cleanup feature is enabled.
 * @param {!Array} files The list of files to be cleaned
 * @param {!Array} registryKeys The list of registry entires to be cleaned.
 */
function startCleanupFromInfected(
    userInitiatedCleanupsEnabled, files, registryKeys) {
  var scannerResults = {'files': files, 'registryKeys': registryKeys};

  cr.webUIListenerCallback('chrome-cleanup-upload-permission-change', false);
  cr.webUIListenerCallback('chrome-cleanup-on-infected', scannerResults);
  Polymer.dom.flush();

  var showItemsButton = chromeCleanupPage.$$('#show-items-button');
  assertTrue(!!showItemsButton);
  MockInteractions.tap(showItemsButton);

  var filesToRemoveList =
      chromeCleanupPage.$$('#files-to-remove-list').$$('#list');
  assertTrue(!!filesToRemoveList);
  validateVisibleItemsList(
      files, filesToRemoveList,
      userInitiatedCleanupsEnabled /* listCanBeShortened */);

  var registryKeysListContainer = chromeCleanupPage.$$('#registry-keys-list');
  assertTrue(!!registryKeysListContainer);
  if (userInitiatedCleanupsEnabled && registryKeys.length > 0) {
    assertFalse(registryKeysListContainer.hidden);
    var registryKeysList = registryKeysListContainer.$$('#list');
    assertTrue(!!registryKeysList);
    validateVisibleItemsList(
        registryKeys, registryKeysList,
        true /* listCanBeShortened */);
  } else {
    assertTrue(registryKeysListContainer.hidden);
  }

  var actionButton = chromeCleanupPage.$$('#action-button');
  assertTrue(!!actionButton);
  MockInteractions.tap(actionButton);
  return ChromeCleanupProxy.whenCalled('startCleanup')
      .then(function(logsUploadEnabled) {
        assertFalse(logsUploadEnabled);
        cr.webUIListenerCallback(
            'chrome-cleanup-on-cleaning', defaultScannerResults);
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

/**
 * @param {boolean} userInitiatedCleanupsEnabled Whether the user initiated
 *     cleanup feature is enabled.
 */
function cleanupFailure(userInitiatedCleanupsEnabled) {
  cr.webUIListenerCallback('chrome-cleanup-upload-permission-change', false);
  cr.webUIListenerCallback('chrome-cleanup-on-cleaning', defaultScannerResults);
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

/**
 * @param {boolean} userInitiatedCleanupsEnabled Whether the user initiated
 *     cleanup feature is enabled.
 */
function cleanupSuccess(userInitiatedCleanupsEnabled) {
  cr.webUIListenerCallback('chrome-cleanup-on-cleaning', defaultScannerResults);
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

/**
 * @param {boolean} testingScanOffered Whether to test the case where scanning
 *     is offered to the user.
 */
function testLogsUploading(testingScanOffered) {
  if (testingScanOffered) {
    cr.webUIListenerCallback(
        'chrome-cleanup-on-infected', defaultScannerResults);
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
    initParametrizedTest(false /* userInitiatedCleanupsEnabled */);
  });

  test('startCleanupFromInfected_FewFilesNoRegistryKeys', function() {
    return startCleanupFromInfected(
        false /* userInitiatedCleanupsEnabled */, shortFileList, []);
  });

  test('startCleanupFromInfected_FewFilesFewRegistryKeys', function() {
    return startCleanupFromInfected(
        false /* userInitiatedCleanupsEnabled */, shortFileList,
        shortRegistryKeysList);
  });

  test('startCleanupFromInfected_FewFilesExactSizeRegistryKeys', function() {
    return startCleanupFromInfected(
        false /* userInitiatedCleanupsEnabled */, shortFileList,
        exactSizeRegistryKeysList);
  });

  test('startCleanupFromInfected_FewFilesManyRegistryKeys', function() {
    return startCleanupFromInfected(
        false /* userInitiatedCleanupsEnabled */, shortFileList,
        longRegistryKeysList);
  });

  test('startCleanupFromInfected_ExactSizeFilesNoRegistryKeys', function() {
    return startCleanupFromInfected(
        false /* userInitiatedCleanupsEnabled */, exactSizeFileList, []);
  });

  test('startCleanupFromInfected_ExactSizeFilesFewRegistryKeys', function() {
    return startCleanupFromInfected(
        false /* userInitiatedCleanupsEnabled */, exactSizeFileList,
        shortRegistryKeysList);
  });

  test(
      'startCleanupFromInfected_ExactSizeFilesExactSizeRegistryKeys',
      function() {
        return startCleanupFromInfected(
            false /* userInitiatedCleanupsEnabled */, exactSizeFileList,
            exactSizeRegistryKeysList);
      });

  test('startCleanupFromInfected_ExactSizeFilesManyRegistryKeys', function() {
    return startCleanupFromInfected(
        false /* userInitiatedCleanupsEnabled */, exactSizeFileList,
        longRegistryKeysList);
  });

  test('startCleanupFromInfected_ManyFilesNoRegistryKeys', function() {
    return startCleanupFromInfected(
        false /* userInitiatedCleanupsEnabled */, longFileList, []);
  });

  test('startCleanupFromInfected_ManyFilesFewRegistryKeys', function() {
    return startCleanupFromInfected(
        false /* userInitiatedCleanupsEnabled */, longFileList,
        shortRegistryKeysList);
  });

  test('startCleanupFromInfected_ManyFilesExactSizeRegistryKeys', function() {
    return startCleanupFromInfected(
        false /* userInitiatedCleanupsEnabled */, longFileList,
        exactSizeRegistryKeysList);
  });

  test('startCleanupFromInfected_ManyFilesManyRegistryKeys', function() {
    return startCleanupFromInfected(
        false /* userInitiatedCleanupsEnabled */, longFileList,
        longRegistryKeysList);
  });

  test('rebootFromRebootRequired', function() {
    return rebootFromRebootRequired();
  });

  test('cleanupFailure', function() {
    return cleanupFailure(false /* userInitiatedCleanupsEnabled */);
  });

  test('cleanupSuccess', function() {
    return cleanupSuccess(false /* userInitiatedCleanupsEnabled */);
  });

  test('logsUploadingOnInfected', function() {
    return testLogsUploading(false /* testingScanOffered */);
  });
});

suite('ChromeCleanupHandler_UserInitiatedCleanupsEnabled', function() {
  setup(function() {
    initParametrizedTest(true /* userInitiatedCleanupsEnabled */);
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

  test('startCleanupFromInfected_FewFilesNoRegistryKeys', function() {
    return startCleanupFromInfected(
        true /* userInitiatedCleanupsEnabled */, shortFileList, []);
  });

  test('startCleanupFromInfected_FewFilesFewRegistryKeys', function() {
    return startCleanupFromInfected(
        true /* userInitiatedCleanupsEnabled */, shortFileList,
        shortRegistryKeysList);
  });

  test('startCleanupFromInfected_FewFilesExactSizeRegistryKeys', function() {
    return startCleanupFromInfected(
        true /* userInitiatedCleanupsEnabled */, shortFileList,
        exactSizeRegistryKeysList);
  });

  test('startCleanupFromInfected_FewFilesManyRegistryKeys', function() {
    return startCleanupFromInfected(
        true /* userInitiatedCleanupsEnabled */, shortFileList,
        longRegistryKeysList);
  });

  test('startCleanupFromInfected_ExactSizeFilesNoRegistryKeys', function() {
    return startCleanupFromInfected(
        true /* userInitiatedCleanupsEnabled */, exactSizeFileList, []);
  });

  test('startCleanupFromInfected_ExactSizeFilesFewRegistryKeys', function() {
    return startCleanupFromInfected(
        true /* userInitiatedCleanupsEnabled */, exactSizeFileList,
        shortRegistryKeysList);
  });

  test(
      'startCleanupFromInfected_ExactSizeFilesExactSizeRegistryKeys',
      function() {
        return startCleanupFromInfected(
            true /* userInitiatedCleanupsEnabled */, exactSizeFileList,
            exactSizeRegistryKeysList);
      });

  test('startCleanupFromInfected_ExactSizeFilesManyRegistryKeys', function() {
    return startCleanupFromInfected(
        true /* userInitiatedCleanupsEnabled */, exactSizeFileList,
        longRegistryKeysList);
  });

  test('startCleanupFromInfected_ManyFilesNoRegistryKeys', function() {
    return startCleanupFromInfected(
        true /* userInitiatedCleanupsEnabled */, longFileList, []);
  });

  test('startCleanupFromInfected_ManyFilesFewRegistryKeys', function() {
    return startCleanupFromInfected(
        true /* userInitiatedCleanupsEnabled */, longFileList,
        shortRegistryKeysList);
  });

  test('startCleanupFromInfected_ManyFilesExactSizeRegistryKeys', function() {
    return startCleanupFromInfected(
        true /* userInitiatedCleanupsEnabled */, longFileList,
        exactSizeRegistryKeysList);
  });

  test('startCleanupFromInfected_ManyFilesManyRegistryKeys', function() {
    return startCleanupFromInfected(
        true /* userInitiatedCleanupsEnabled */, longFileList,
        longRegistryKeysList);
  });

  test('rebootFromRebootRequired', function() {
    return rebootFromRebootRequired();
  });

  test('cleanupFailure', function() {
    return cleanupFailure(true /* userInitiatedCleanupsEnabled */);
  });

  test('cleanupSuccess', function() {
    return cleanupSuccess(true /* userInitiatedCleanupsEnabled */);
  });

  test('logsUploadingOnScanOffered', function() {
    return testLogsUploading(true /* testingScanOffered */);
  });

  test('logsUploadingOnInfected', function() {
    return testLogsUploading(false /* testingScanOffered */);
  });
});

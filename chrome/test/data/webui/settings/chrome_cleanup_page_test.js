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

let chromeCleanupPage = null;

/** @type {?TestDownloadsBrowserProxy} */
let chromeCleanupProxy = null;

const shortFileList = ['file 1', 'file 2', 'file 3'];
const exactSizeFileList = ['file 1', 'file 2', 'file 3', 'file 4'];
const longFileList = ['file 1', 'file 2', 'file 3', 'file 4', 'file 5'];
const shortRegistryKeysList = ['key 1', 'key 2'];
const exactSizeRegistryKeysList = ['key 1', 'key 2', 'key 3', 'key 4'];
const longRegistryKeysList =
    ['key 1', 'key 2', 'key 3', 'key 4', 'key 5', 'key 6'];

const defaultScannerResults = {
  'files': shortFileList,
  'registryKeys': shortRegistryKeysList,
};

/**
 * @param {boolean} userInitiatedCleanupsEnabled Whether the user initiated
 *     cleanup feature is enabled.
 */
function initParametrizedTest(userInitiatedCleanupsEnabled) {
  chromeCleanupProxy = new TestChromeCleanupProxy();
  settings.ChromeCleanupProxyImpl.instance_ = chromeCleanupProxy;

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
  let visibleItemsList =
      visibleItems.querySelectorAll('* /deep/ .visible-item');
  const moreItemsLink = visibleItems.$$('#more-items-link');

  if (!listCanBeShortened ||
      originalItems.length <= settings.CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW) {
    assertEquals(visibleItemsList.length, originalItems.length);
    assertTrue(moreItemsLink.hidden);
  } else {
    assertEquals(
        visibleItemsList.length,
        settings.CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW - 1);
    assertFalse(moreItemsLink.hidden);

    // Tapping on the "show more" link should expand the list.
    MockInteractions.tap(moreItemsLink);
    Polymer.dom.flush();

    visibleItemsList =
        visibleItems.querySelectorAll('* /deep/ .visible-item');
    assertEquals(visibleItemsList.length, originalItems.length);
    assertTrue(moreItemsLink.hidden);
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
  const scannerResults = {'files': files, 'registryKeys': registryKeys};

  cr.webUIListenerCallback('chrome-cleanup-upload-permission-change', false);
  cr.webUIListenerCallback(
      'chrome-cleanup-on-infected', true /* isPoweredByPartner */,
      scannerResults);
  Polymer.dom.flush();

  const showItemsButton = chromeCleanupPage.$$('#show-items-button');
  assertTrue(!!showItemsButton);
  MockInteractions.tap(showItemsButton);

  const filesToRemoveList = chromeCleanupPage.$$('#files-to-remove-list');
  assertTrue(!!filesToRemoveList);
  validateVisibleItemsList(
      files, filesToRemoveList,
      userInitiatedCleanupsEnabled /* listCanBeShortened */);

  const registryKeysListContainer = chromeCleanupPage.$$('#registry-keys-list');
  assertTrue(!!registryKeysListContainer);
  if (userInitiatedCleanupsEnabled && registryKeys.length > 0) {
    assertFalse(registryKeysListContainer.hidden);
    assertTrue(!!registryKeysListContainer);
    validateVisibleItemsList(
        registryKeys, registryKeysListContainer, true /* listCanBeShortened */);
  } else {
    assertTrue(registryKeysListContainer.hidden);
  }

  const actionButton = chromeCleanupPage.$$('#action-button');
  assertTrue(!!actionButton);
  MockInteractions.tap(actionButton);
  return chromeCleanupProxy.whenCalled('startCleanup')
      .then(function(logsUploadEnabled) {
        assertFalse(logsUploadEnabled);
        cr.webUIListenerCallback(
            'chrome-cleanup-on-cleaning', true /* isPoweredByPartner */,
            defaultScannerResults);
        Polymer.dom.flush();

        const spinner = chromeCleanupPage.$$('#waiting-spinner');
        assertTrue(spinner.active);
      });
}

function rebootFromRebootRequired() {
  cr.webUIListenerCallback('chrome-cleanup-on-reboot-required');
  Polymer.dom.flush();

  const actionButton = chromeCleanupPage.$$('#action-button');
  assertTrue(!!actionButton);
  MockInteractions.tap(actionButton);
  return chromeCleanupProxy.whenCalled('restartComputer');
}

/**
 * @param {boolean} userInitiatedCleanupsEnabled Whether the user initiated
 *     cleanup feature is enabled.
 */
function cleanupFailure(userInitiatedCleanupsEnabled) {
  cr.webUIListenerCallback('chrome-cleanup-upload-permission-change', false);
  cr.webUIListenerCallback(
      'chrome-cleanup-on-cleaning', true /* isPoweredByPartner */,
      defaultScannerResults);
  cr.webUIListenerCallback(
      'chrome-cleanup-on-idle',
      settings.ChromeCleanupIdleReason.CLEANING_FAILED);
  Polymer.dom.flush();

  const actionButton = chromeCleanupPage.$$('#action-button');
  if (userInitiatedCleanupsEnabled) {
    assertFalse(!!actionButton);
  } else {
    assertTrue(!!actionButton);
    MockInteractions.tap(actionButton);
    return chromeCleanupProxy.whenCalled('dismissCleanupPage');
  }
}

/**
 * @param {boolean} userInitiatedCleanupsEnabled Whether the user initiated
 *     cleanup feature is enabled.
 */
function cleanupSuccess(userInitiatedCleanupsEnabled) {
  cr.webUIListenerCallback(
      'chrome-cleanup-on-cleaning', true /* isPoweredByPartner */,
      defaultScannerResults);
  cr.webUIListenerCallback(
      'chrome-cleanup-on-idle',
      settings.ChromeCleanupIdleReason.CLEANING_SUCCEEDED);
  Polymer.dom.flush();

  const actionButton = chromeCleanupPage.$$('#action-button');
  if (userInitiatedCleanupsEnabled) {
    assertFalse(!!actionButton);
  } else {
    assertTrue(!!actionButton);
    MockInteractions.tap(actionButton);
    return chromeCleanupProxy.whenCalled('dismissCleanupPage');
  }
}

/**
 * @param {boolean} testingScanOffered Whether to test the case where scanning
 *     is offered to the user.
 */
function testLogsUploading(testingScanOffered) {
  if (testingScanOffered) {
    cr.webUIListenerCallback(
        'chrome-cleanup-on-infected', true /* isPoweredByPartner */,
        defaultScannerResults);
  } else {
    cr.webUIListenerCallback(
        'chrome-cleanup-on-idle', settings.ChromeCleanupIdleReason.INITIAL);
  }
  Polymer.dom.flush();

  const logsControl = chromeCleanupPage.$$('#chromeCleanupLogsUploadControl');
  assertTrue(!!logsControl);

  cr.webUIListenerCallback('chrome-cleanup-upload-permission-change', true);
  Polymer.dom.flush();
  assertTrue(logsControl.checked);

  cr.webUIListenerCallback('chrome-cleanup-upload-permission-change', false);
  Polymer.dom.flush();
  assertFalse(logsControl.checked);

  MockInteractions.tap(logsControl.$.control);
  return chromeCleanupProxy.whenCalled('setLogsUploadPermission')
      .then(function(logsUploadEnabled) {
        assertTrue(logsUploadEnabled);
      });
}

/**
 * @param {boolean} onInfected Whether to test the case where current state is
 *     INFECTED, as opposed to CLEANING.
 * @param {boolean} isPoweredByPartner Whether to test the case when scan
 *     results are provided by a partner.
 */
function testPartnerLogoShown(onInfected, isPoweredByPartner) {
  cr.webUIListenerCallback(
      onInfected ? 'chrome-cleanup-on-infected' : 'chrome-cleanup-on-cleaning',
      isPoweredByPartner, defaultScannerResults);
  Polymer.dom.flush();

  const poweredByContainerControl =
      chromeCleanupPage.$$('#powered-by-container');
  assertTrue(!!poweredByContainerControl);
  assertNotEquals(poweredByContainerControl.hidden, isPoweredByPartner);
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

  test('onInfectedResultsProvidedByPartner_True', function() {
    return testPartnerLogoShown(
        true /* onInfected */, true /* isPoweredByPartner */);
  });

  test('onInfectedResultsProvidedByPartner_False', function() {
    return testPartnerLogoShown(
        true /* onInfected */, false /* isPoweredByPartner */);
  });

  test('onCleaningResultsProvidedByPartner_True', function() {
    return testPartnerLogoShown(
        false /* onInfected */, true /* isPoweredByPartner */);
  });

  test('onCleaningResultsProvidedByPartner_False', function() {
    return testPartnerLogoShown(
        false /* onInfected */, false /* isPoweredByPartner */);
  });
});

suite('ChromeCleanupHandler_UserInitiatedCleanupsEnabled', function() {
  setup(function() {
    initParametrizedTest(true /* userInitiatedCleanupsEnabled */);
  });

  function scanOfferedOnInitiallyIdle(idleReason) {
    cr.webUIListenerCallback('chrome-cleanup-on-idle', idleReason);
    Polymer.dom.flush();

    const actionButton = chromeCleanupPage.$$('#action-button');
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

  test('scanOfferedOnInitiallyIdle_CleanerDownloadFailed', function() {
    scanOfferedOnInitiallyIdle(
        settings.ChromeCleanupIdleReason.CLEANER_DOWNLOAD_FAILED);
  });

  test('cleanerDownloadFailure', function() {
    cr.webUIListenerCallback('chrome-cleanup-on-reporter-running');
    cr.webUIListenerCallback(
        'chrome-cleanup-on-idle',
        settings.ChromeCleanupIdleReason.CLEANER_DOWNLOAD_FAILED);
    Polymer.dom.flush();

    const actionButton = chromeCleanupPage.$$('#action-button');
    assertTrue(!!actionButton);
    MockInteractions.tap(actionButton);
    return chromeCleanupProxy.whenCalled('startScanning');
  });

  test('reporterFoundNothing', function() {
    cr.webUIListenerCallback('chrome-cleanup-on-reporter-running');
    cr.webUIListenerCallback(
        'chrome-cleanup-on-idle',
        settings.ChromeCleanupIdleReason.REPORTER_FOUND_NOTHING);
    Polymer.dom.flush();

    const actionButton = chromeCleanupPage.$$('#action-button');
    assertFalse(!!actionButton);
  });

  test('reporterFoundNothing', function() {
    cr.webUIListenerCallback('chrome-cleanup-on-reporter-running');
    cr.webUIListenerCallback(
        'chrome-cleanup-on-idle',
        settings.ChromeCleanupIdleReason.REPORTER_FOUND_NOTHING);
    Polymer.dom.flush();

    const actionButton = chromeCleanupPage.$$('#action-button');
    assertFalse(!!actionButton);
  });

  test('startScanFromIdle', function() {
    cr.webUIListenerCallback('chrome-cleanup-upload-permission-change', false);
    cr.webUIListenerCallback(
        'chrome-cleanup-on-idle', settings.ChromeCleanupIdleReason.INITIAL);
    Polymer.dom.flush();

    const actionButton = chromeCleanupPage.$$('#action-button');
    assertTrue(!!actionButton);
    MockInteractions.tap(actionButton);
    return chromeCleanupProxy.whenCalled('startScanning')
        .then(function(logsUploadEnabled) {
          assertFalse(logsUploadEnabled);
          cr.webUIListenerCallback('chrome-cleanup-on-scanning', false);
          Polymer.dom.flush();

          const spinner = chromeCleanupPage.$$('#waiting-spinner');
          assertTrue(spinner.active);
        });
  });

  test('scanFoundNothing', function() {
    cr.webUIListenerCallback('chrome-cleanup-on-scanning', false);
    cr.webUIListenerCallback(
        'chrome-cleanup-on-idle',
        settings.ChromeCleanupIdleReason.SCANNING_FOUND_NOTHING);
    Polymer.dom.flush();

    const actionButton = chromeCleanupPage.$$('#action-button');
    assertFalse(!!actionButton);
  });

  test('scanFailure', function() {
    cr.webUIListenerCallback('chrome-cleanup-on-scanning', false);
    cr.webUIListenerCallback(
        'chrome-cleanup-on-idle',
        settings.ChromeCleanupIdleReason.SCANNING_FAILED);
    Polymer.dom.flush();

    const actionButton = chromeCleanupPage.$$('#action-button');
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

  test('onInfectedResultsProvidedByPartner_True', function() {
    return testPartnerLogoShown(
        true /* onInfected */, true /* isPoweredByPartner */);
  });

  test('onInfectedResultsProvidedByPartner_False', function() {
    return testPartnerLogoShown(
        true /* onInfected */, false /* isPoweredByPartner */);
  });

  test('onCleaningResultsProvidedByPartner_True', function() {
    return testPartnerLogoShown(
        false /* onInfected */, true /* isPoweredByPartner */);
  });

  test('onCleaningResultsProvidedByPartner_False', function() {
    return testPartnerLogoShown(
        false /* onInfected */, false /* isPoweredByPartner */);
  });
});

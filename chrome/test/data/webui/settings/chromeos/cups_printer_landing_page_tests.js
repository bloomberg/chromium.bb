// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {!HTMLElement} printerEntry
 * @private
 */
function clickThreeDotMenu(printerEntry) {
  // Click on three dot menu on an item entry.
  const threeDot = printerEntry.$$('.icon-more-vert');
  threeDot.click();

  Polymer.dom.flush();
}

/**
 * @param {!HTMLElement} printerEntry
 * @private
 */
function clickAddAutomaticButton(printerEntry) {
  // Click on add button on an item entry.
  const addButton = printerEntry.$$('#savePrinterButton');
  assertTrue(!!addButton);
  addButton.click();
  Polymer.dom.flush();
}

/**
 * @param {!HTMLElement} printerEntry
 * @private
 */
function clickSetupButton(printerEntry) {
  // Click on setup button on an item entry.
  const setupButton = printerEntry.$$('#setupPrinterButton');
  assertTrue(!!setupButton);
  setupButton.click();
  Polymer.dom.flush();
}

/**
 * @param {!HTMLElement} dialog
 * @private
 */
function clickSaveButton(dialog) {
  const saveButton = dialog.$$('.action-button');

  assertTrue(!!saveButton);
  assertTrue(!saveButton.disabled);

  saveButton.click();

  Polymer.dom.flush();
}

/**
 * Helper method to pull an array of CupsPrinterEntry out of a
 * |printersElement|.
 * @param {!HTMLElement} printersElement
 * @return {!Array<!HTMLElement>}
 * @private
 */
function getPrinterEntries(printersElement) {
  const entryList = printersElement.$$('#printerEntryList');
  return entryList.querySelectorAll(
      'settings-cups-printers-entry:not([hidden])');
}

/**
 * @param {!HTMLElement} page
 * @return {!HTMLElement}
 * @private
 */
function initializeEditDialog(page) {
  const editDialog = page.$$('#editPrinterDialog');
  assertTrue(!!editDialog);

  // Edit dialog will reset |ppdModel| when |ppdManufacturer| is set. Must
  // set values separately.
  editDialog.pendingPrinter_.ppdManufacturer = 'make';
  editDialog.pendingPrinter_.ppdModel = 'model';

  // Initializing |activePrinter| in edit dialog will set
  // |needsReconfigured_| to true. Reset it so that any changes afterwards
  // mimics user input.
  editDialog.needsReconfigured_ = false;

  return editDialog;
}

/**
 * @param {string} expectedMessage
 * @param {!HTMLElement} toast
 * @private
 */
function verifyErrorToastMessage(expectedMessage, toast) {
  assertTrue(toast.open);
  assertEquals(expectedMessage, toast.textContent.trim());
}

/**
 * Helper function that verifies that |printerList| matches the printers in
 * |entryList|.
 * @param {!HTMLElement} entryList
 * @param {!Array<!CupsPrinterInfo>} printerList
 * @private
 */
function verifyPrintersList(entryList, printerList) {
  for (let index = 0; index < printerList.length; ++index) {
    const entryInfo = entryList[index].printerEntry.printerInfo;
    const printerInfo = printerList[index];

    assertEquals(printerInfo.printerName, entryInfo.printerName);
    assertEquals(printerInfo.printerAddress, entryInfo.printerAddress);
    assertEquals(printerInfo.printerId, entryInfo.printerId);
    assertEquals(entryList.length, printerList.length);
  }
}

/**
 * Helper function to verify that printers in |printerListEntries| that contain
 * |searchTerm| are not in |hiddenEntries|.
 * @param {!Element} printerEntryListTestElement
 * @param {string} searchTerm
 */
function verifyFilteredPrinters(printerEntryListTestElement, searchTerm) {
  const printerListEntries =
      Array.from(printerEntryListTestElement.querySelectorAll(
          'settings-cups-printers-entry'));
  const hiddenEntries = Array.from(printerEntryListTestElement.querySelectorAll(
      'settings-cups-printers-entry[hidden]'));

  for (let i = 0; i < printerListEntries.length; ++i) {
    const entry = printerListEntries[i];
    if (hiddenEntries.indexOf(entry) == -1) {
      assertTrue(
          entry.printerEntry.printerInfo.printerName.toLowerCase().includes(
              searchTerm.toLowerCase()));
    }
  }
}

/**
 * Helper function to verify that the actual visible printers match the
 * expected printer list.
 * @param {!Element} printerEntryListTestElement
 * @param {!Array<!PrinterListEntry>} expectedVisiblePrinters
 */
function verifyVisiblePrinters(
    printerEntryListTestElement, expectedVisiblePrinters) {
  const actualPrinterList =
      Array.from(printerEntryListTestElement.querySelectorAll(
          'settings-cups-printers-entry:not([hidden])'));

  assertEquals(expectedVisiblePrinters.length, actualPrinterList.length);
  for (let i = 0; i < expectedVisiblePrinters.length; i++) {
    const expectedPrinter = expectedVisiblePrinters[i].printerInfo;
    const actualPrinter = actualPrinterList[i].printerEntry.printerInfo;

    assertEquals(actualPrinter.printerName, expectedPrinter.printerName);
    assertEquals(actualPrinter.printerAddress, expectedPrinter.printerAddress);
    assertEquals(actualPrinter.printerId, expectedPrinter.printerId);
  }
}

/**
 * Helper function to verify that printers are hidden accordingly if they do not
 * match the search query. Also checks if the no search results section is shown
 * when appropriate.
 * @param {!Element} printersElement
 * @param {!Array<!PrinterListEntry>} expectedVisiblePrinters
 * @param {string} searchTerm
 */
function verifySearchQueryResults(
    printersElement, expectedVisiblePrinters, searchTerm) {
  const printerEntryListTestElement = printersElement.$$('#printerEntryList');

  verifyVisiblePrinters(printerEntryListTestElement, expectedVisiblePrinters);
  verifyFilteredPrinters(printerEntryListTestElement, searchTerm);

  if (expectedVisiblePrinters.length) {
    assertTrue(printersElement.$$('#no-search-results').hidden);
  } else {
    assertFalse(printersElement.$$('#no-search-results').hidden);
  }
}

/**
 * Removes a saved printer located at |index|.
 * @param {!TestCupsPrintersBrowserProxy} cupsPrintersBrowserProxy
 * @param {!HTMLElement} savedPrintersElement
 * @param {number} index
 * @return {!Promise}
 */
function removePrinter(cupsPrintersBrowserProxy, savedPrintersElement, index) {
  let printerList = cupsPrintersBrowserProxy.printerList.printerList;
  let savedPrinterEntries = getPrinterEntries(savedPrintersElement);

  clickThreeDotMenu(savedPrinterEntries[index]);
  savedPrintersElement.$$('#removeButton').click();

  return cupsPrintersBrowserProxy.whenCalled('removeCupsPrinter')
      .then(function() {
        // Simulate removing the printer from |cupsPrintersBrowserProxy|.
        printerList.splice(index, 1);

        // Simuluate saved printer changes.
        cr.webUIListenerCallback(
            'on-printers-changed', cupsPrintersBrowserProxy.printerList);
        Polymer.dom.flush();
      });
}

/**
 * Removes all saved printers through recursion.
 * @param {!TestCupsPrintersBrowserProxy} cupsPrintersBrowserProxy
 * @param {!HTMLElement} savedPrintersElement
 * @return {!Promise}
 */
function removeAllPrinters(cupsPrintersBrowserProxy, savedPrintersElement) {
  let printerList = cupsPrintersBrowserProxy.printerList.printerList;
  let savedPrinterEntries = getPrinterEntries(savedPrintersElement);

  if (!printerList.length) {
    return Promise.resolve();
  }

  return removePrinter(
             cupsPrintersBrowserProxy, savedPrintersElement, 0 /* index */)
      .then(test_util.flushTasks)
      .then(removeAllPrinters.bind(
          this, cupsPrintersBrowserProxy, savedPrintersElement));
}

/**
 * @param {string} printerName
 * @param {string} printerAddress
 * @param {string} printerId
 * @return {!CupsPrinterInfo}
 * @private
 */
function createCupsPrinterInfo(printerName, printerAddress, printerId) {
  const printer = {
    ppdManufacturer: 'make',
    ppdModel: 'model',
    printerAddress: printerAddress,
    printerDescription: '',
    printerId: printerId,
    printerManufacturer: 'make',
    printerModel: 'model',
    printerMakeAndModel: '',
    printerName: printerName,
    printerPPDPath: '',
    printerPpdReference: {
      userSuppliedPpdUrl: '',
      effectiveMakeAndModel: '',
      autoconf: false,
    },
    printerProtocol: 'ipp',
    printerQueue: 'moreinfohere',
    printerStatus: '',
  };
  return printer;
}

/**
 * Helper function that creates a new PrinterListEntry.
 * @param {string} printerName
 * @param {string} printerAddress
 * @param {string} printerId
 * @param {string} printerType
 * @return {!PrinterListEntry}
 */
function createPrinterListEntry(
    printerName, printerAddress, printerId, printerType) {
  const entry = {
    printerInfo: {
      ppdManufacturer: '',
      ppdModel: '',
      printerAddress: printerAddress,
      printerDescription: '',
      printerId: printerId,
      printerManufacturer: '',
      printerModel: '',
      printerMakeAndModel: '',
      printerName: printerName,
      printerPPDPath: '',
      printerPpdReference: {
        userSuppliedPpdUrl: '',
        effectiveMakeAndModel: '',
        autoconf: false,
      },
      printerProtocol: 'ipp',
      printerQueue: 'moreinfohere',
      printerStatus: '',
    },
    printerType: printerType,
  };
  return entry;
}

suite('CupsSavedPrintersTests', function() {
  let page = null;
  let savedPrintersElement = null;

  /** @type {?settings.TestCupsPrintersBrowserProxy} */
  let cupsPrintersBrowserProxy = null;

  /** @type {?Array<!CupsPrinterInfo>} */
  let printerList = null;

  /** @type {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  let mojoApi_;

  suiteSetup(function() {
    mojoApi_ = new FakeNetworkConfig();
    network_config.MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;
  });

  setup(function() {
    const mojom = chromeos.networkConfig.mojom;

    cupsPrintersBrowserProxy =
        new printerBrowserProxy.TestCupsPrintersBrowserProxy;
    // Simulate internet connection.
    mojoApi_.resetForTest();
    mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kWiFi, true);
    const wifi1 =
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kWiFi, 'wifi');
    wifi1.connectionState = mojom.ConnectionStateType.kConnected;
    mojoApi_.addNetworksForTest([wifi1]);

    PolymerTest.clearBody();
    settings.navigateTo(settings.routes.CUPS_PRINTERS);
  });

  teardown(function() {
    mojoApi_.resetForTest();
    cupsPrintersBrowserProxy.reset();
    page.remove();
    savedPrintersElement = null;
    printerList = null;
    page = null;
  });

  /** @param {!Array<!CupsPrinterInfo>} printerList */
  function createCupsPrinterPage(printers) {
    printerList = printers;
    // |cupsPrinterBrowserProxy| needs to have a list of saved printers before
    // initializing the landing page.
    cupsPrintersBrowserProxy.printerList = {printerList: printerList};
    settings.CupsPrintersBrowserProxyImpl.instance_ = cupsPrintersBrowserProxy;

    page = document.createElement('settings-cups-printers');
    // Enable feature flag to show the new saved printers list.
    // TODO(jimmyxgong): Remove this line when the feature flag is removed.
    page.enableUpdatedUi_ = true;
    document.body.appendChild(page);
    assertTrue(!!page);

    Polymer.dom.flush();
  }

  /** @param {!CupsPrinterInfo} printer*/
  function addNewSavedPrinter(printer) {
    printerList.push(printer);
    updateSavedPrinters();
  }

  /** @param {number} id*/
  function removeSavedPrinter(id) {
    const idx = printerList.findIndex(p => p.printerId == id);
    printerList.splice(idx, 1);
    updateSavedPrinters();
  }

  function updateSavedPrinters() {
    cupsPrintersBrowserProxy.printerList = {printerList: printerList};
    cr.webUIListenerCallback(
        'on-printers-changed', cupsPrintersBrowserProxy.printerList);
    Polymer.dom.flush();
  }

  test('SavedPrintersSuccessfullyPopulates', function() {
    createCupsPrinterPage([
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrintersElement = page.$$('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          // List component contained by CupsSavedPrinters.
          const savedPrintersList =
              savedPrintersElement.$$('settings-cups-printers-entry-list');

          let printerListEntries = getPrinterEntries(savedPrintersElement);

          verifyPrintersList(printerListEntries, printerList);
        });
  });

  test('SuccessfullyRemoveMultipleSavedPrinters', function() {
    let savedPrinterEntries = [];

    createCupsPrinterPage([
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrintersElement = page.$$('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          return removeAllPrinters(
              cupsPrintersBrowserProxy, savedPrintersElement);
        })
        .then(() => {
          let entryList = getPrinterEntries(savedPrintersElement);
          verifyPrintersList(entryList, printerList);
        });
  });

  test('HideSavedPrintersWhenEmpty', function() {
    // List component contained by CupsSavedPrinters.
    let savedPrintersList = [];
    let savedPrinterEntries = [];

    createCupsPrinterPage([
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrintersElement = page.$$('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          savedPrintersList =
              savedPrintersElement.$$('settings-cups-printers-entry-list');
          savedPrinterEntries = getPrinterEntries(savedPrintersElement);

          verifyPrintersList(savedPrinterEntries, printerList);

          assertTrue(!!page.$$('#savedPrinters'));

          return removeAllPrinters(
              cupsPrintersBrowserProxy, savedPrintersElement);
        })
        .then(() => {
          assertFalse(!!page.$$('#savedPrinters'));
        });
  });

  test('UpdateSavedPrinter', function() {
    const expectedName = 'edited name';

    let editDialog = null;
    let savedPrinterEntries = null;

    createCupsPrinterPage([
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrintersElement = page.$$('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          savedPrinterEntries = getPrinterEntries(savedPrintersElement);

          // Update the printer name of the first entry.
          clickThreeDotMenu(savedPrinterEntries[0]);
          savedPrintersElement.$$('#editButton').click();

          Polymer.dom.flush();

          editDialog = initializeEditDialog(page);

          // Change name of printer and save the change.
          const nameField = editDialog.$$('.printer-name-input');
          assertTrue(!!nameField);
          nameField.value = expectedName;
          nameField.fire('input');

          Polymer.dom.flush();

          clickSaveButton(editDialog);

          return cupsPrintersBrowserProxy.whenCalled('updateCupsPrinter');
        })
        .then(() => {
          assertEquals(expectedName, editDialog.activePrinter.printerName);

          // Mimic changes to |cupsPrintersBrowserProxy.printerList|.
          printerList[0].printerName = expectedName;

          verifyPrintersList(savedPrinterEntries, printerList);
        });
  });

  test('ReconfigureSavedPrinter', function() {
    const expectedName = 'edited name';
    const expectedAddress = '1.1.1.1';

    let savedPrinterEntries = null;
    let editDialog = null;

    createCupsPrinterPage([
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrintersElement = page.$$('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          savedPrinterEntries = getPrinterEntries(savedPrintersElement);

          // Edit the first entry.
          clickThreeDotMenu(savedPrinterEntries[0]);
          savedPrintersElement.$$('#editButton').click();

          Polymer.dom.flush();

          editDialog = initializeEditDialog(page);

          const nameField = editDialog.$$('.printer-name-input');
          assertTrue(!!nameField);
          nameField.value = expectedName;
          nameField.fire('input');

          const addressField = editDialog.$$('#printerAddress');
          assertTrue(!!addressField);
          addressField.value = expectedAddress;
          addressField.fire('input');

          Polymer.dom.flush();

          clickSaveButton(editDialog);

          return cupsPrintersBrowserProxy.whenCalled('reconfigureCupsPrinter');
        })
        .then(() => {
          assertEquals(expectedName, editDialog.activePrinter.printerName);
          assertEquals(
              expectedAddress, editDialog.activePrinter.printerAddress);

          // Mimic changes to |cupsPrintersBrowserProxy.printerList|.
          printerList[0].printerName = expectedName;
          printerList[0].printerAddress = expectedAddress;

          verifyPrintersList(savedPrinterEntries, printerList);
        });
  });

  test('SavedPrintersSearchTermFiltersCorrectPrinters', function() {
    createCupsPrinterPage([
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrintersElement = page.$$('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          let printerListEntries = getPrinterEntries(savedPrintersElement);
          verifyPrintersList(printerListEntries, printerList);

          searchTerm = 'google';
          savedPrintersElement.searchTerm = searchTerm;
          Polymer.dom.flush();

          // Filtering "google" should result in one visible entry and two
          // hidden entries.
          verifySearchQueryResults(
              savedPrintersElement,
              [createPrinterListEntry('google', '4', 'id4', PrinterType.SAVED)],
              searchTerm);

          // Change the search term and assert that entries are filtered
          // correctly. Filtering "test" should result in three visible entries
          // and one hidden entry.
          searchTerm = 'test';
          savedPrintersElement.searchTerm = searchTerm;
          Polymer.dom.flush();

          verifySearchQueryResults(
              savedPrintersElement,
              [
                createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED),
                createPrinterListEntry('test2', '2', 'id2', PrinterType.SAVED),
              ],
              searchTerm);

          // Add more printers and assert that they are correctly filtered.
          addNewSavedPrinter(createCupsPrinterInfo('test3', '3', 'id3'));
          addNewSavedPrinter(createCupsPrinterInfo('google2', '6', 'id6'));
          Polymer.dom.flush();

          verifySearchQueryResults(
              savedPrintersElement,
              [
                createPrinterListEntry('test3', '3', 'id3', PrinterType.SAVED),
                createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED),
                createPrinterListEntry('test2', '2', 'id2', PrinterType.SAVED)
              ],
              searchTerm);
        });
  });

  test('SavedPrintersNoSearchFound', function() {
    createCupsPrinterPage([
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrintersElement = page.$$('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          let printerListEntries = getPrinterEntries(savedPrintersElement);
          verifyPrintersList(printerListEntries, printerList);

          searchTerm = 'google';
          savedPrintersElement.searchTerm = searchTerm;
          Polymer.dom.flush();

          // Filtering "google" should result in one visible entry and three
          // hidden entries.
          verifySearchQueryResults(
              savedPrintersElement,
              [createPrinterListEntry('google', '4', 'id4', PrinterType.SAVED)],
              searchTerm);

          // Change search term to something that has no matches.
          searchTerm = 'noSearchFound';
          savedPrintersElement.searchTerm = searchTerm;
          Polymer.dom.flush();

          verifySearchQueryResults(savedPrintersElement, [], searchTerm);

          // Change search term back to "google" and verify that the No search
          // found message is no longer there.
          searchTerm = 'google';
          savedPrintersElement.searchTerm = searchTerm;
          Polymer.dom.flush();

          verifySearchQueryResults(
              savedPrintersElement,
              [createPrinterListEntry('google', '4', 'id4', PrinterType.SAVED)],
              searchTerm);
        });
  });

  test('ShowMoreButtonIsInitiallyHiddenAndANewPrinterIsAdded', function() {
    createCupsPrinterPage([
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrintersElement = page.$$('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          let printerEntryListTestElement =
              savedPrintersElement.$$('#printerEntryList');

          verifyVisiblePrinters(printerEntryListTestElement, [
            createPrinterListEntry('google', '4', 'id4', PrinterType.SAVED),
            createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED),
            createPrinterListEntry('test2', '2', 'id2', PrinterType.SAVED)
          ]);
          // Assert that the Show more button is hidden because printer list
          // length is <= 3.
          assertFalse(!!savedPrintersElement.$$('#show-more-container'));

          // Newly added printers will always be visible and inserted to the
          // top of the list.
          addNewSavedPrinter(createCupsPrinterInfo('test3', '3', 'id3'));
          expectedVisiblePrinters = [
            createPrinterListEntry('test3', '3', 'id3', PrinterType.SAVED),
            createPrinterListEntry('google', '4', 'id4', PrinterType.SAVED),
            createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED),
            createPrinterListEntry('test2', '2', 'id2', PrinterType.SAVED)
          ];
          verifyVisiblePrinters(
              printerEntryListTestElement, expectedVisiblePrinters);
          // Assert that the Show more button is still hidden because all newly
          // added printers are visible.
          assertFalse(!!savedPrintersElement.$$('#show-more-container'));
        });
  });

  test('PressShowMoreButton', function() {
    createCupsPrinterPage([
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
      createCupsPrinterInfo('test3', '3', 'id3'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrintersElement = page.$$('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          const printerEntryListTestElement =
              savedPrintersElement.$$('#printerEntryList');

          // There are 4 total printers but only 3 printers are visible and 1 is
          // hidden underneath the Show more section.
          verifyVisiblePrinters(printerEntryListTestElement, [
            createPrinterListEntry('google', '4', 'id4', PrinterType.SAVED),
            createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED),
            createPrinterListEntry('test2', '2', 'id2', PrinterType.SAVED),
          ]);
          // Assert that the Show more button is shown since printer list length
          // is > 3.
          assertTrue(!!savedPrintersElement.$$('#show-more-container'));

          // Click on the Show more button.
          savedPrintersElement.$$('#show-more-icon').click();
          Polymer.dom.flush();
          assertFalse(!!savedPrintersElement.$$('#show-more-container'));
          // Clicking on the Show more button reveals all hidden printers.
          verifyVisiblePrinters(printerEntryListTestElement, [
            createPrinterListEntry('google', '4', 'id4', PrinterType.SAVED),
            createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED),
            createPrinterListEntry('test2', '2', 'id2', PrinterType.SAVED),
            createPrinterListEntry('test3', '3', 'id3', PrinterType.SAVED)
          ]);
        });
  });

  test('ShowMoreButtonIsInitiallyShownAndWithANewPrinterAdded', function() {
    createCupsPrinterPage([
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
      createCupsPrinterInfo('test3', '3', 'id3'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrintersElement = page.$$('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          const printerEntryListTestElement =
              savedPrintersElement.$$('#printerEntryList');

          // There are 4 total printers but only 3 printers are visible and 1 is
          // hidden underneath the Show more section.
          verifyVisiblePrinters(printerEntryListTestElement, [
            createPrinterListEntry('google', '4', 'id4', PrinterType.SAVED),
            createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED),
            createPrinterListEntry('test2', '2', 'id2', PrinterType.SAVED),
          ]);
          // Assert that the Show more button is shown since printer list length
          // is > 3.
          assertTrue(!!savedPrintersElement.$$('#show-more-container'));

          // Newly added printers will always be visible.
          addNewSavedPrinter(createCupsPrinterInfo('test5', '5', 'id5'));
          verifyVisiblePrinters(printerEntryListTestElement, [
            createPrinterListEntry('test5', '5', 'id5', PrinterType.SAVED),
            createPrinterListEntry('google', '4', 'id4', PrinterType.SAVED),
            createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED),
            createPrinterListEntry('test2', '2', 'id2', PrinterType.SAVED)
          ]);
          // Assert that the Show more button is still shown.
          assertTrue(!!savedPrintersElement.$$('#show-more-container'));
        });
  });

  test('ShowMoreButtonIsShownAndRemovePrinters', function() {
    createCupsPrinterPage([
      createCupsPrinterInfo('google', '3', 'id3'),
      createCupsPrinterInfo('google2', '4', 'id4'),
      createCupsPrinterInfo('google3', '5', 'id5'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrintersElement = page.$$('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          const printerEntryListTestElement =
              savedPrintersElement.$$('#printerEntryList');

          // There are 5 total printers but only 3 printers are visible and 2
          // are hidden underneath the Show more section.
          verifyVisiblePrinters(printerEntryListTestElement, [
            createPrinterListEntry('google', '3', 'id3', PrinterType.SAVED),
            createPrinterListEntry('google2', '4', 'id4', PrinterType.SAVED),
            createPrinterListEntry('google3', '5', 'id5', PrinterType.SAVED)
          ]);
          // Assert that the Show more button is shown since printer list length
          // is > 3.
          assertTrue(!!savedPrintersElement.$$('#show-more-container'));

          // Simulate removing 'google' printer.
          removeSavedPrinter('id3');
          // Printer list has 4 elements now, but since the list is still
          // collapsed we should still expect only 3 elements to be visible.
          // Since printers were initially alphabetically sorted, we should
          // expect 'test1' to be the next visible printer.
          verifyVisiblePrinters(printerEntryListTestElement, [
            createPrinterListEntry('google2', '4', 'id4', PrinterType.SAVED),
            createPrinterListEntry('google3', '5', 'id5', PrinterType.SAVED),
            createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED)
          ]);
          assertTrue(!!savedPrintersElement.$$('#show-more-container'));

          // Simulate removing 'google2' printer.
          removeSavedPrinter('id4');
          // Printer list has 3 elements now, the Show more button should be
          // hidden.
          verifyVisiblePrinters(printerEntryListTestElement, [
            createPrinterListEntry('google3', '5', 'id5', PrinterType.SAVED),
            createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED),
            createPrinterListEntry('test2', '2', 'id2', PrinterType.SAVED)
          ]);
          assertFalse(!!savedPrintersElement.$$('#show-more-container'));
        });
  });

  test('ShowMoreButtonIsShownAndSearchQueryFiltersCorrectly', function() {
    createCupsPrinterPage([
      createCupsPrinterInfo('google', '3', 'id3'),
      createCupsPrinterInfo('google2', '4', 'id4'),
      createCupsPrinterInfo('google3', '5', 'id5'),
      createCupsPrinterInfo('google4', '6', 'id6'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrintersElement = page.$$('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          const printerEntryListTestElement =
              savedPrintersElement.$$('#printerEntryList');

          // There are 6 total printers but only 3 printers are visible and 3
          // are hidden underneath the Show more section.
          verifyVisiblePrinters(printerEntryListTestElement, [
            createPrinterListEntry('google', '3', 'id3', PrinterType.SAVED),
            createPrinterListEntry('google2', '4', 'id4', PrinterType.SAVED),
            createPrinterListEntry('google3', '5', 'id5', PrinterType.SAVED)
          ]);
          // Assert that the Show more button is shown since printer list length
          // is > 3.
          assertTrue(!!savedPrintersElement.$$('#show-more-container'));

          // Set search term to 'google' and expect 4 visible printers.
          searchTerm = 'google';
          savedPrintersElement.searchTerm = searchTerm;
          Polymer.dom.flush();
          verifySearchQueryResults(
              savedPrintersElement,
              [
                createPrinterListEntry('google', '3', 'id3', PrinterType.SAVED),
                createPrinterListEntry(
                    'google2', '4', 'id4', PrinterType.SAVED),
                createPrinterListEntry(
                    'google3', '5', 'id5', PrinterType.SAVED),
                createPrinterListEntry('google4', '6', 'id6', PrinterType.SAVED)
              ],
              searchTerm);
          // Having a search term should hide the Show more button.
          assertFalse(!!savedPrintersElement.$$('#show-more-container'));

          // Search for a term with no matching printers. Expect Show more
          // button to still be hidden.
          searchTerm = 'noSearchFound';
          savedPrintersElement.searchTerm = searchTerm;
          Polymer.dom.flush();
          verifySearchQueryResults(savedPrintersElement, [], searchTerm);

          assertFalse(!!savedPrintersElement.$$('#show-more-container'));

          // Change search term and expect new set of visible printers.
          searchTerm = 'test';
          savedPrintersElement.searchTerm = searchTerm;
          Polymer.dom.flush();
          verifySearchQueryResults(
              savedPrintersElement,
              [
                createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED),
                createPrinterListEntry('test2', '2', 'id2', PrinterType.SAVED)
              ],
              searchTerm);
          assertFalse(!!savedPrintersElement.$$('#show-more-container'));

          // Remove the search term and expect the collapsed list to appear
          // again.
          searchTerm = '';
          savedPrintersElement.searchTerm = searchTerm;
          Polymer.dom.flush();
          const expectedVisiblePrinters = [
            createPrinterListEntry('google', '3', 'id3', PrinterType.SAVED),
            createPrinterListEntry('google2', '4', 'id4', PrinterType.SAVED),
            createPrinterListEntry('google3', '5', 'id5', PrinterType.SAVED)
          ];
          verifySearchQueryResults(
              savedPrintersElement, expectedVisiblePrinters, searchTerm);
          verifyVisiblePrinters(
              printerEntryListTestElement, expectedVisiblePrinters);
          assertTrue(!!savedPrintersElement.$$('#show-more-container'));
        });
  });

  test('ShowMoreButtonAddAndRemovePrinters', function() {
    createCupsPrinterPage([
      createCupsPrinterInfo('google', '3', 'id3'),
      createCupsPrinterInfo('google2', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrintersElement = page.$$('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          const printerEntryListTestElement =
              savedPrintersElement.$$('#printerEntryList');

          // There are 4 total printers but only 3 printers are visible and 1 is
          // hidden underneath the Show more section.
          verifyVisiblePrinters(printerEntryListTestElement, [
            createPrinterListEntry('google', '3', 'id3', PrinterType.SAVED),
            createPrinterListEntry('google2', '4', 'id4', PrinterType.SAVED),
            createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED)
          ]);
          // Assert that the Show more button is shown since printer list length
          // is > 3.
          assertTrue(!!savedPrintersElement.$$('#show-more-container'));

          // Add a new printer and expect it to be at the top of the list.
          addNewSavedPrinter(createCupsPrinterInfo('newPrinter', '5', 'id5'));
          verifyVisiblePrinters(printerEntryListTestElement, [
            createPrinterListEntry('newPrinter', '5', 'id5', PrinterType.SAVED),
            createPrinterListEntry('google', '3', 'id3', PrinterType.SAVED),
            createPrinterListEntry('google2', '4', 'id4', PrinterType.SAVED),
            createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED)
          ]);
          assertTrue(!!savedPrintersElement.$$('#show-more-container'));

          // Now simulate removing printer 'test1'.
          removeSavedPrinter('id1');
          // If the number of visible printers is > 3, removing printers will
          // decrease the number of visible printers until there are only 3
          // visible printers. In this case, we remove 'test1' and now only
          // have 3 visible printers and 1 hidden printer: 'test2'.
          verifyVisiblePrinters(printerEntryListTestElement, [
            createPrinterListEntry('newPrinter', '5', 'id5', PrinterType.SAVED),
            createPrinterListEntry('google', '3', 'id3', PrinterType.SAVED),
            createPrinterListEntry('google2', '4', 'id4', PrinterType.SAVED)
          ]);
          assertTrue(!!savedPrintersElement.$$('#show-more-container'));

          // Remove another printer and assert that we still have 3 visible
          // printers but now 'test2' is our third visible printer.
          removeSavedPrinter('id4');
          verifyVisiblePrinters(printerEntryListTestElement, [
            createPrinterListEntry('newPrinter', '5', 'id5', PrinterType.SAVED),
            createPrinterListEntry('google', '3', 'id3', PrinterType.SAVED),
            createPrinterListEntry('test2', '2', 'id2', PrinterType.SAVED)
          ]);
          // Printer list length is <= 3, Show more button should be hidden.
          assertFalse(!!savedPrintersElement.$$('#show-more-container'));
        });
  });
});

suite('CupsNearbyPrintersTests', function() {
  let page = null;
  let nearbyPrintersElement = null;

  /** @type {?settings.TestCupsPrintersBrowserProxy} */
  let cupsPrintersBrowserProxy = null;

  /** @type{!HtmlElement} */
  let printerEntryListTestElement = null;

  /** @type {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  let mojoApi_;

  suiteSetup(function() {
    mojoApi_ = new FakeNetworkConfig();
    network_config.MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;
  });

  setup(function() {
    const mojom = chromeos.networkConfig.mojom;
    cupsPrintersBrowserProxy =
        new printerBrowserProxy.TestCupsPrintersBrowserProxy;

    settings.CupsPrintersBrowserProxyImpl.instance_ = cupsPrintersBrowserProxy;

    // Simulate internet connection.
    mojoApi_.resetForTest();
    const wifi1 =
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kWiFi, 'wifi1');
    wifi1.connectionState = mojom.ConnectionStateType.kOnline;
    mojoApi_.addNetworksForTest([wifi1]);

    PolymerTest.clearBody();
    settings.navigateTo(settings.routes.CUPS_PRINTERS);

    page = document.createElement('settings-cups-printers');
    // Enable feature flag to show the new saved printers list.
    // TODO(jimmyxgong): Remove this line when the feature flag is removed.
    page.enableUpdatedUi_ = true;
    document.body.appendChild(page);
    assertTrue(!!page);

    Polymer.dom.flush();
  });

  teardown(function() {
    mojoApi_.resetForTest();
    cupsPrintersBrowserProxy.reset();
    page.remove();
    nearbyPrintersElement = null;
    page = null;
  });

  test('nearbyPrintersSuccessfullyPopulates', function() {
    const automaticPrinterList = [
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ];
    const discoveredPrinterList = [
      createCupsPrinterInfo('test3', '3', 'id3'),
      createCupsPrinterInfo('test4', '4', 'id4'),
    ];

    return test_util.flushTasks().then(() => {
      nearbyPrintersElement = page.$$('settings-cups-nearby-printers');
      assertTrue(!!nearbyPrintersElement);

      // Assert that no printers have been detected.
      let nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);
      assertEquals(0, nearbyPrinterEntries.length);

      // Simuluate finding nearby printers.
      cr.webUIListenerCallback(
          'on-nearby-printers-changed', automaticPrinterList,
          discoveredPrinterList);

      Polymer.dom.flush();

      nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);

      const expectedPrinterList =
          automaticPrinterList.concat(discoveredPrinterList);
      verifyPrintersList(nearbyPrinterEntries, expectedPrinterList);
    });
  });

  test('nearbyPrintersSortOrderAutoFirstThenDiscovered', function() {
    let discoveredPrinterA =
        createCupsPrinterInfo('printerNameA', 'printerAddress1', 'printerId1');
    let discoveredPrinterB =
        createCupsPrinterInfo('printerNameB', 'printerAddress2', 'printerId2');
    let discoveredPrinterC =
        createCupsPrinterInfo('printerNameC', 'printerAddress3', 'printerId3');
    let autoPrinterD =
        createCupsPrinterInfo('printerNameD', 'printerAddress4', 'printerId4');
    let autoPrinterE =
        createCupsPrinterInfo('printerNameE', 'printerAddress5', 'printerId5');
    let autoPrinterF =
        createCupsPrinterInfo('printerNameF', 'printerAddress6', 'printerId6');

    // Add printers in a non-alphabetical order to test sorting.
    const automaticPrinterList = [autoPrinterF, autoPrinterD, autoPrinterE];
    const discoveredPrinterList =
        [discoveredPrinterC, discoveredPrinterA, discoveredPrinterB];

    // Expected sort order is to sort automatic printers first then
    // sort discovered printers
    const expectedPrinterList = [
      autoPrinterD, autoPrinterE, autoPrinterF, discoveredPrinterA,
      discoveredPrinterB, discoveredPrinterC
    ];

    return test_util.flushTasks().then(() => {
      nearbyPrintersElement = page.$$('settings-cups-nearby-printers');
      assertTrue(!!nearbyPrintersElement);

      // Simuluate finding nearby printers.
      cr.webUIListenerCallback(
          'on-nearby-printers-changed', automaticPrinterList,
          discoveredPrinterList);

      Polymer.dom.flush();

      nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);

      verifyPrintersList(nearbyPrinterEntries, expectedPrinterList);
    });
  });

  test('addingAutomaticPrinterIsSuccessful', function() {
    const automaticPrinterList = [createCupsPrinterInfo('test1', '1', 'id1')];
    const discoveredPrinterList = [];

    return test_util.flushTasks()
        .then(() => {
          nearbyPrintersElement = page.$$('settings-cups-nearby-printers');
          assertTrue(!!nearbyPrintersElement);

          // Simuluate finding nearby printers.
          cr.webUIListenerCallback(
              'on-nearby-printers-changed', automaticPrinterList,
              discoveredPrinterList);

          Polymer.dom.flush();

          // Requery and assert that the newly detected printer automatic
          // printer has the correct button.
          nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);
          assertEquals(1, nearbyPrinterEntries.length);
          assertTrue(!!nearbyPrinterEntries[0].$$('#savePrinterButton'));

          // Add an automatic printer and assert that that the toast
          // notification is shown.
          clickAddAutomaticButton(nearbyPrinterEntries[0]);

          Polymer.dom.flush();

          return cupsPrintersBrowserProxy.whenCalled('addDiscoveredPrinter');
        })
        .then(() => {
          const expectedToastMessage =
              'Added ' + automaticPrinterList[0].printerName;
          verifyErrorToastMessage(expectedToastMessage, page.$$('#errorToast'));
        });
  });

  test('addingDiscoveredPrinterIsSuccessful', function() {
    const automaticPrinterList = [];
    const discoveredPrinterList = [createCupsPrinterInfo('test3', '3', 'id3')];

    let manufacturerDialog = null;

    return test_util.flushTasks()
        .then(() => {
          nearbyPrintersElement = page.$$('settings-cups-nearby-printers');
          assertTrue(!!nearbyPrintersElement);

          // Simuluate finding nearby printers.
          cr.webUIListenerCallback(
              'on-nearby-printers-changed', automaticPrinterList,
              discoveredPrinterList);

          Polymer.dom.flush();

          // Requery and assert that a newly detected discovered printer has
          // the correct icon button.
          nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);
          assertEquals(1, nearbyPrinterEntries.length);
          assertTrue(!!nearbyPrinterEntries[0].$$('#setupPrinterButton'));

          // Assert that clicking on the setup button shows the advanced
          // configuration dialog.
          clickSetupButton(nearbyPrinterEntries[0]);

          Polymer.dom.flush();

          addDialog = page.$$('#addPrinterDialog');
          manufacturerDialog =
              addDialog.$$('add-printer-manufacturer-model-dialog');
          assertTrue(!!manufacturerDialog);

          return cupsPrintersBrowserProxy.whenCalled(
              'getCupsPrinterManufacturersList');
        })
        .then(() => {
          const addButton = manufacturerDialog.$$('#addPrinterButton');
          assertTrue(addButton.disabled);

          // Populate the manufacturer and model fields to enable the Add
          // button.
          manufacturerDialog.$$('#manufacturerDropdown').value = 'make';
          modelDropdown = manufacturerDialog.$$('#modelDropdown');
          modelDropdown.value = 'model';

          assertTrue(!addButton.disabled);

          addButton.click();
          return cupsPrintersBrowserProxy.whenCalled('addCupsPrinter');
        })
        .then(() => {
          // Assert that the toast notification is shown and has the expected
          // message when adding a discovered printer.
          const expectedToastMessage =
              'Added ' + discoveredPrinterList[0].printerName;
          verifyErrorToastMessage(expectedToastMessage, page.$$('#errorToast'));
        });
  });

  test('NetworkConnectedButNoInternet', function() {
    // Simulate connecting to a network with no internet connection.
    mojoApi_.setNetworkConnectionStateForTest(
        'wifi1_guid',
        chromeos.networkConfig.mojom.ConnectionStateType.kConnected);
    return test_util.flushTasks().then(() => {
      // We require internet to be able to add a new printer. Connecting to
      // a network without connectivity should be equivalent to not being
      // connected to a network.
      assertTrue(!!page.$$('#cloudOffIcon'));
      assertTrue(!!page.$$('#connectionMessage'));
      assertTrue(!!page.$$('#addManualPrinterIcon').disabled);
    });
  });

  test('checkNetworkConnection', function() {
    // Simulate disconnecting from a network.
    mojoApi_.setNetworkConnectionStateForTest(
        'wifi1_guid',
        chromeos.networkConfig.mojom.ConnectionStateType.kNotConnected);
    return test_util.flushTasks()
        .then(() => {
          // Expect offline text to show up when no internet is
          // connected.
          assertTrue(!!page.$$('#cloudOffIcon'));
          assertTrue(!!page.$$('#connectionMessage'));
          assertTrue(!!page.$$('#addManualPrinterIcon').disabled);

          // Simulate connecting to a network with connectivity.
          mojoApi_.setNetworkConnectionStateForTest(
              'wifi1_guid',
              chromeos.networkConfig.mojom.ConnectionStateType.kOnline);
          return test_util.flushTasks();
        })
        .then(() => {
          const automaticPrinterList = [
            createCupsPrinterInfo('test1', '1', 'id1'),
            createCupsPrinterInfo('test2', '2', 'id2'),
          ];
          const discoveredPrinterList = [
            createCupsPrinterInfo('test3', '3', 'id3'),
            createCupsPrinterInfo('test4', '4', 'id4'),
          ];

          // Simuluate finding nearby printers.
          cr.webUIListenerCallback(
              'on-nearby-printers-changed', automaticPrinterList,
              discoveredPrinterList);

          Polymer.dom.flush();

          nearbyPrintersElement = page.$$('settings-cups-nearby-printers');
          assertTrue(!!nearbyPrintersElement);

          printerEntryListTestElement =
              nearbyPrintersElement.$$('#printerEntryList');
          assertTrue(!!printerEntryListTestElement);

          nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);

          const expectedPrinterList =
              automaticPrinterList.concat(discoveredPrinterList);
          verifyPrintersList(nearbyPrinterEntries, expectedPrinterList);
        });
  });

  test('NearbyPrintersSearchTermFiltersCorrectPrinters', function() {
    let discoveredPrinterList = [
      createCupsPrinterInfo('test1', 'printerAddress1', 'printerId1'),
      createCupsPrinterInfo('test2', 'printerAddress2', 'printerId2'),
      createCupsPrinterInfo('google', 'printerAddress3', 'printerId3'),
    ];

    return test_util.flushTasks().then(() => {
      nearbyPrintersElement = page.$$('settings-cups-nearby-printers');
      assertTrue(!!nearbyPrintersElement);

      printerEntryListTestElement =
          nearbyPrintersElement.$$('#printerEntryList');
      assertTrue(!!printerEntryListTestElement);

      // Simuluate finding nearby printers.
      cr.webUIListenerCallback(
          'on-nearby-printers-changed', [], discoveredPrinterList);

      Polymer.dom.flush();

      verifyVisiblePrinters(printerEntryListTestElement, [
        createPrinterListEntry(
            'google', 'printerAddress3', 'printerId3', PrinterType.DISCOVERD),
        createPrinterListEntry(
            'test1', 'printerAddress1', 'printerId1', PrinterType.DISCOVERD),
        createPrinterListEntry(
            'test2', 'printerAddress2', 'printerId2', PrinterType.DISCOVERD)
      ]);

      searchTerm = 'google';
      nearbyPrintersElement.searchTerm = searchTerm;
      Polymer.dom.flush();

      // Filtering "google" should result in one visible entry and two hidden
      // entries.
      verifySearchQueryResults(
          nearbyPrintersElement, [createPrinterListEntry(
                                     'google', 'printerAddress3', 'printerId3',
                                     PrinterType.DISCOVERD)],
          searchTerm);

      // Filtering "test" should result in two visible entries and one hidden
      // entry.
      searchTerm = 'test';
      nearbyPrintersElement.searchTerm = searchTerm;
      Polymer.dom.flush();

      verifySearchQueryResults(
          nearbyPrintersElement,
          [
            createPrinterListEntry(
                'test1', 'printerAddress1', 'printerId1',
                PrinterType.DISCOVERD),
            createPrinterListEntry(
                'test2', 'printerAddress2', 'printerId2', PrinterType.DISCOVERD)
          ],
          searchTerm);

      // Add more printers and assert that they are correctly filtered.
      discoveredPrinterList.push(
          createCupsPrinterInfo('test3', 'printerAddress4', 'printerId4'));
      discoveredPrinterList.push(
          createCupsPrinterInfo('google2', 'printerAddress5', 'printerId5'));

      // Simuluate finding nearby printers.
      cr.webUIListenerCallback(
          'on-nearby-printers-changed', [], discoveredPrinterList);

      Polymer.dom.flush();

      verifySearchQueryResults(
          nearbyPrintersElement,
          [
            createPrinterListEntry(
                'test1', 'printerAddress1', 'printerId1',
                PrinterType.DISCOVERD),
            createPrinterListEntry(
                'test2', 'printerAddress2', 'printerId2',
                PrinterType.DISCOVERD),
            createPrinterListEntry(
                'test3', 'printerAddress4', 'printerId4', PrinterType.DISCOVERD)
          ],
          searchTerm);
    });
  });

  test('NearbyPrintersNoSearchFound', function() {
    const discoveredPrinterList = [
      createCupsPrinterInfo('test1', 'printerAddress1', 'printerId1'),
      createCupsPrinterInfo('google', 'printerAddress2', 'printerId2')
    ];

    return test_util.flushTasks().then(() => {
      nearbyPrintersElement = page.$$('settings-cups-nearby-printers');
      assertTrue(!!nearbyPrintersElement);

      printerEntryListTestElement =
          nearbyPrintersElement.$$('#printerEntryList');
      assertTrue(!!printerEntryListTestElement);

      // Simuluate finding nearby printers.
      cr.webUIListenerCallback(
          'on-nearby-printers-changed', [], discoveredPrinterList);

      Polymer.dom.flush();

      searchTerm = 'google';
      nearbyPrintersElement.searchTerm = searchTerm;
      Polymer.dom.flush();

      // Set the search term and filter out the printers. Filtering "google"
      // should result in one visible entry and one hidden entries.
      verifySearchQueryResults(
          nearbyPrintersElement, [createPrinterListEntry(
                                     'google', 'printerAddress2', 'printerId2',
                                     PrinterType.DISCOVERED)],
          searchTerm);

      // Change search term to something that has no matches.
      searchTerm = 'noSearchFound';
      nearbyPrintersElement.searchTerm = searchTerm;
      Polymer.dom.flush();

      verifySearchQueryResults(nearbyPrintersElement, [], searchTerm);

      // Change search term back to "google" and verify that the No search found
      // message is no longer there.
      searchTerm = 'google';
      nearbyPrintersElement.searchTerm = searchTerm;
      Polymer.dom.flush();

      verifySearchQueryResults(
          nearbyPrintersElement, [createPrinterListEntry(
                                     'google', 'printerAddress2', 'printerId2',
                                     PrinterType.DISCOVERD)],
          searchTerm);
    });
  });
});

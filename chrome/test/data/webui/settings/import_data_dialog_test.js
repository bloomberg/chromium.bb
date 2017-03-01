// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @implements {settings.ImportDataBrowserProxy}
 * @extends {settings.TestBrowserProxy}
 */
var TestImportDataBrowserProxy = function() {
  settings.TestBrowserProxy.call(this, [
    'initializeImportDialog',
    'importFromBookmarksFile',
    'importData',
  ]);

  /** @private {!Array<!settings.BrowserProfile} */
  this.browserProfiles_ = [];
};

TestImportDataBrowserProxy.prototype = {
  __proto__: settings.TestBrowserProxy.prototype,

  /** @param {!Array<!settings.BrowserProfile} browserProfiles */
  setBrowserProfiles: function(browserProfiles) {
    this.browserProfiles_ = browserProfiles;
  },

  /** @override */
  initializeImportDialog: function() {
    this.methodCalled('initializeImportDialog');
    return Promise.resolve(this.browserProfiles_.slice());
  },

  /** @override */
  importFromBookmarksFile: function() {
    this.methodCalled('importFromBookmarksFile');
  },

  /** @override */
  importData: function(browserProfileIndex) {
    this.methodCalled('importData', browserProfileIndex);
  },
};

suite('ImportDataDialog', function() {
  /** @type {!Array<!settings.BrowserProfile} */
  var browserProfiles = [
    {
      autofillFormData: true,
      favorites: true,
      history: true,
      index: 0,
      name: "Mozilla Firefox",
      passwords: true,
      search: true
    },
    {
      autofillFormData: false,
      favorites: true,
      history: false,
      index: 1,
      name: "Bookmarks HTML File",
      passwords: false,
      search: false
    },
  ];

  function createBooleanPref(name) {
    return {
      key: name,
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    };
  }

  var prefs = {};
  [
    'import_history',
    'import_bookmarks',
    'import_saved_passwords',
    'import_search_engine',
    'import_autofill_form_data',
  ].forEach(function(name) {
    prefs[name] = createBooleanPref(name);
  });

  var dialog = null;

  setup(function() {
    browserProxy = new TestImportDataBrowserProxy();
    browserProxy.setBrowserProfiles(browserProfiles);
    settings.ImportDataBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    dialog = document.createElement('settings-import-data-dialog');
    dialog.set('prefs', prefs);
    document.body.appendChild(dialog);
    return browserProxy.whenCalled('initializeImportDialog').then(function() {
      assertTrue(dialog.$.dialog.open);
      Polymer.dom.flush();
    });
  });

  function simulateBrowserProfileChange(index) {
    dialog.$.browserSelect.selectedIndex = index;
    dialog.$.browserSelect.dispatchEvent(new CustomEvent('change'));
  }

  test('Initialization', function() {
    assertFalse(dialog.$.import.hidden);
    assertFalse(dialog.$.import.disabled);
    assertFalse(dialog.$.cancel.hidden);
    assertFalse(dialog.$.cancel.disabled);
    assertTrue(dialog.$.done.hidden);
    assertTrue(dialog.$.successIcon.parentElement.hidden);
  });

  test('ImportButton', function() {
    assertFalse(dialog.$.import.disabled);

    // Flip all prefs to false.
    Object.keys(prefs).forEach(function(prefName) {
      dialog.set('prefs.' + prefName + '.value', false);
    });
    assertTrue(dialog.$.import.disabled);

    // Change browser selection to "Import from Bookmarks HTML file".
    simulateBrowserProfileChange(1);
    assertTrue(dialog.$.import.disabled);

    // Ensure everything except |import_bookmarks| is ignored.
    dialog.set('prefs.import_history.value', true);
    assertTrue(dialog.$.import.disabled);

    dialog.set('prefs.import_bookmarks.value', true);
    assertFalse(dialog.$.import.disabled);
  });

  function assertInProgressButtons() {
    assertFalse(dialog.$.import.hidden);
    assertTrue(dialog.$.import.disabled);
    assertFalse(dialog.$.cancel.hidden);
    assertTrue(dialog.$.cancel.disabled);
    assertTrue(dialog.$.done.hidden);
    assertTrue(dialog.$$('paper-spinner').active);
    assertFalse(dialog.$$('paper-spinner').hidden);
  }

  function assertSucceededButtons() {
    assertTrue(dialog.$.import.hidden);
    assertTrue(dialog.$.cancel.hidden);
    assertFalse(dialog.$.done.hidden);
    assertFalse(dialog.$$('paper-spinner').active);
    assertTrue(dialog.$$('paper-spinner').hidden);
  }

  /** @param {!settings.ImportDataStatus} status */
  function simulateImportStatusChange(status) {
    cr.webUIListenerCallback('import-data-status-changed', status);
  }

  test('ImportFromBookmarksFile', function() {
    simulateBrowserProfileChange(1);
    MockInteractions.tap(dialog.$.import);
    return browserProxy.whenCalled('importFromBookmarksFile').then(function() {
      simulateImportStatusChange(settings.ImportDataStatus.IN_PROGRESS);
      assertInProgressButtons();

      simulateImportStatusChange(settings.ImportDataStatus.SUCCEEDED);
      assertSucceededButtons();

      assertFalse(dialog.$.successIcon.parentElement.hidden);
      assertFalse(dialog.$$('settings-toggle-button').parentElement.hidden);
    });
  });

  test('ImportFromBrowserProfile', function() {
    dialog.set('prefs.import_bookmarks.value', false);

    var expectedIndex = 0;
    simulateBrowserProfileChange(expectedIndex);
    MockInteractions.tap(dialog.$.import);
    return browserProxy.whenCalled('importData').then(function(actualIndex) {
      assertEquals(expectedIndex, actualIndex);

      simulateImportStatusChange(settings.ImportDataStatus.IN_PROGRESS);
      assertInProgressButtons();

      simulateImportStatusChange(settings.ImportDataStatus.SUCCEEDED);
      assertSucceededButtons();

      assertFalse(dialog.$.successIcon.parentElement.hidden);
      assertTrue(dialog.$$('settings-toggle-button').parentElement.hidden);
    });
  });

  test('ImportError', function() {
    simulateImportStatusChange(settings.ImportDataStatus.FAILED);
    assertFalse(dialog.$.dialog.open);
  });
});

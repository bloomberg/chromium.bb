// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_search_engines_page', function() {
  /**
   * @param {boolean} canBeDefault
   * @param {boolean} canBeEdited
   * @param {boolean} canBeRemoved
   * @return {!SearchEngine}
   */
  var createSampleSearchEngine = function(
      canBeDefault, canBeEdited, canBeRemoved) {
    return {
      canBeDefault: canBeDefault,
      canBeEdited: canBeEdited,
      canBeRemoved: canBeRemoved,
      default: false,
      displayName: "Google displayName",
      iconURL: "http://www.google.com/favicon.ico",
      isOmniboxExtension: false,
      keyword: "google.com",
      modelIndex: 0,
      name: "Google",
      url: "https://search.foo.com/search?p=%s",
      urlLocked: false,
    };
  };

  /** @return {!SearchEngine} */
  var createSampleOmniboxExtension = function() {
    return {
      canBeDefault: false,
      canBeEdited: false,
      canBeRemoved: false,
      default: false,
      displayName: "Omnibox extension displayName",
      extension: {
        icon: "chrome://extension-icon/some-extension-icon",
        id: "dummyextensionid",
        name: "Omnibox extension"
      },
      isOmniboxExtension: true,
      keyword: "oe",
      modelIndex: 6,
      name: "Omnibox extension",
      url: "chrome-extension://dummyextensionid/?q=%s",
      urlLocked: false
    };
  };

  function registerDialogTests() {
    suite('AddSearchEngineDialogTests', function() {
      /** @type {?SettingsAddSearchEngineDialog} */
      var dialog = null;
      var browserProxy = null;

      setup(function() {
        browserProxy = new settings_search.TestSearchEnginesBrowserProxy();
        settings.SearchEnginesBrowserProxyImpl.instance_ = browserProxy;
        PolymerTest.clearBody();
        dialog = document.createElement('settings-search-engine-dialog');
        document.body.appendChild(dialog);
      });

      teardown(function() { dialog.remove(); });

      // Tests that the dialog calls 'searchEngineEditStarted' and
      // 'searchEngineEditCancelled' when closed from the 'x' button.
      test('DialogOpenAndClose', function() {
        return browserProxy.whenCalled('searchEngineEditStarted').then(
            function() {
              MockInteractions.tap(dialog.$.dialog.getCloseButton());
              return browserProxy.whenCalled('searchEngineEditCancelled');
            });
      });

      // Tests that the dialog calls 'searchEngineEditStarted' and
      // 'searchEngineEditCancelled' when closed from the 'cancel' button.
      test('DialogOpenAndCancel', function() {
        return browserProxy.whenCalled('searchEngineEditStarted').then(
            function() {
              MockInteractions.tap(dialog.$.cancel);
              return browserProxy.whenCalled('searchEngineEditCancelled');
            });
      });

      // Tests the dialog to add a new search engine. Specifically
      //  - paper-input elements are empty initially.
      //  - action button initially disabled.
      //  - validation is triggered on 'input' event.
      //  - action button is enabled when all fields are valid.
      //  - action button triggers appropriate browser signal when tapped.
      test('DialogAddSearchEngine', function() {
        /**
         * Triggers an 'input' event on the paper-input element and checks that
         * validation is triggered.
         * @param {string} inputId
         * @return {!Promise}
         */
        var inputAndValidate = function(inputId) {
          var inputElement = dialog.$[inputId];
          browserProxy.resetResolver('validateSearchEngineInput');
          inputElement.fire('input');
          return inputElement.value != '' ?
              // Expeting validation only on non-empty values.
              browserProxy.whenCalled('validateSearchEngineInput') :
              Promise.resolve();
        };

        assertEquals('', dialog.$.searchEngine.value);
        assertEquals('', dialog.$.keyword.value);
        assertEquals('', dialog.$.queryUrl.value);
        var actionButton = dialog.$.actionButton;
        assertTrue(actionButton.disabled);

        return inputAndValidate('searchEngine').then(function() {
          return inputAndValidate('keyword');
        }).then(function() {
          return inputAndValidate('queryUrl');
        }).then(function() {
          // Manually set the text to a non-empty string for all fields.
          dialog.$.searchEngine.value = 'foo';
          dialog.$.keyword.value = 'bar';
          dialog.$.queryUrl.value = 'baz';

          return inputAndValidate('searchEngine');
        }).then(function() {
          // Assert that the action button has been enabled now that all input
          // is valid and non-empty.
          assertFalse(actionButton.disabled);
          MockInteractions.tap(actionButton);
          return browserProxy.whenCalled('searchEngineEditCompleted');
        });
      });

    });
  }

  function registerSearchEngineEntryTests() {
    suite('SearchEngineEntryTests', function() {
      /** @type {?SettingsSearchEngineEntryElement} */
      var entry = null;

      /** @type {!settings_search.TestSearchEnginesBrowserProxy} */
      var browserProxy = null;

      /** @type {!SearchEngine} */
      var searchEngine = createSampleSearchEngine(true, true, true);

      setup(function() {
        browserProxy = new settings_search.TestSearchEnginesBrowserProxy();
        settings.SearchEnginesBrowserProxyImpl.instance_ = browserProxy;
        PolymerTest.clearBody();
        entry = document.createElement('settings-search-engine-entry');
        entry.set('engine', searchEngine);
        document.body.appendChild(entry);
      });

      teardown(function() { entry.remove(); });

      // Test that the <search-engine-entry> is populated according to its
      // underlying SearchEngine model.
      test('Initialization', function() {
        assertEquals(
            searchEngine.displayName,
            entry.root.querySelector('#name-column').textContent.trim());
        assertEquals(
            searchEngine.keyword,
            entry.root.querySelector('#keyword-column').textContent);
        assertEquals(
            searchEngine.url,
            entry.root.querySelector('#url-column').textContent);
      });

      test('Remove_Enabled', function() {
        // Open action menu.
        MockInteractions.tap(entry.$$('paper-icon-button'));
        var menu = entry.$$('dialog[is=cr-action-menu]');
        assertTrue(menu.open);

        var deleteButton = entry.$.delete;
        assertTrue(!!deleteButton);
        assertFalse(deleteButton.hidden);
        MockInteractions.tap(deleteButton);
        return browserProxy.whenCalled('removeSearchEngine').then(
            function(modelIndex) {
              assertFalse(menu.open);
              assertEquals(entry.engine.modelIndex, modelIndex);
            });
      });

      test('MakeDefault_Enabled', function() {
        // Open action menu.
        MockInteractions.tap(entry.$$('paper-icon-button'));
        var menu = entry.$$('dialog[is=cr-action-menu]');
        assertTrue(menu.open);

        var makeDefaultButton = entry.$.makeDefault;
        assertTrue(!!makeDefaultButton);
        MockInteractions.tap(makeDefaultButton);
        return browserProxy.whenCalled('setDefaultSearchEngine').then(
            function(modelIndex) {
              assertFalse(menu.open);
              assertEquals(entry.engine.modelIndex, modelIndex);
            });
      });

      // Test that clicking the "edit" button brings up a dialog.
      test('Edit_Enabled', function() {
        // Open action menu.
        MockInteractions.tap(entry.$$('paper-icon-button'));
        var menu = entry.$$('dialog[is=cr-action-menu]');
        assertTrue(menu.open);

        var engine = entry.engine;
        var editButton = entry.$.edit;
        assertTrue(!!editButton);
        assertFalse(editButton.hidden);
        MockInteractions.tap(editButton);
        return browserProxy.whenCalled('searchEngineEditStarted').then(
            function(modelIndex) {
              assertEquals(engine.modelIndex, modelIndex);
              var dialog = entry.$$('settings-search-engine-dialog');
              assertTrue(!!dialog);

              // Check that the paper-input fields are pre-populated.
              assertEquals(engine.name, dialog.$.searchEngine.value);
              assertEquals(engine.keyword, dialog.$.keyword.value);
              assertEquals(engine.url, dialog.$.queryUrl.value);

              assertFalse(dialog.$.actionButton.disabled);
            });
      });

      /**
       * Checks that the given button is disabled (by being hidden), for the
       * given search engine.
       * @param {!SearchEngine} searchEngine
       * @param {string} buttonId
       */
      function testButtonDisabled(searchEngine, buttonId) {
        entry.engine = searchEngine;
        var button = entry.$[buttonId];
        assertTrue(!!button);
        assertTrue(button.hidden);
      }

      test('Remove_Disabled', function() {
        testButtonDisabled(
            createSampleSearchEngine(true, true, false), 'delete');
      });

      test('MakeDefault_Disabled', function() {
        testButtonDisabled(
            createSampleSearchEngine(false, true, true), 'makeDefault');
      });

      test('Edit_Disabled', function() {
        testButtonDisabled(createSampleSearchEngine(true, false, true), 'edit');
      });

      test('All_Disabled', function() {
        entry.engine = createSampleSearchEngine(true, false, false);
        Polymer.dom.flush();
        assertTrue(entry.hasAttribute('show-dots_'));

        entry.engine = createSampleSearchEngine(false, false, false);
        Polymer.dom.flush();
        assertFalse(entry.hasAttribute('show-dots_'));
      });
    });
  }

  function registerPageTests() {
    suite('SearchEnginePageTests', function() {
      /** @type {?SettingsSearchEnginesPageElement} */
      var page = null;

      var browserProxy = null;

      /** @type {!SearchEnginesInfo} */
      var searchEnginesInfo = {
        defaults: [createSampleSearchEngine()],
        others: [],
        extensions: [createSampleOmniboxExtension()],
      };

      setup(function() {
        browserProxy = new settings_search.TestSearchEnginesBrowserProxy();
        browserProxy.setSearchEnginesInfo(searchEnginesInfo);
        settings.SearchEnginesBrowserProxyImpl.instance_ = browserProxy;
        PolymerTest.clearBody();
        page = document.createElement('settings-search-engines-page');
        document.body.appendChild(page);
        return browserProxy.whenCalled('getSearchEnginesList');
      });

      teardown(function() { page.remove(); });

      // Tests that the page is querying and displaying search engine info on
      // startup.
      test('Initialization', function() {
        var searchEnginesLists = page.shadowRoot.
            querySelectorAll('settings-search-engines-list');
        assertEquals(2, searchEnginesLists.length);

        // Note: iron-list may create hidden children, so test the length
        // if IronList.items instead of the child nodes.
        Polymer.dom.flush();
        var defaultsList = searchEnginesLists[0];
        var defaultsEntries = Polymer.dom(defaultsList.shadowRoot).
            querySelector('iron-list').items;
        assertEquals(
            searchEnginesInfo.defaults.length, defaultsEntries.length);

        var othersList = searchEnginesLists[1];
        var othersEntries = Polymer.dom(othersList.shadowRoot).
            querySelector('iron-list').items;
        assertEquals(
            searchEnginesInfo.others.length, othersEntries.length);

        var extensionEntries = Polymer.dom(page.shadowRoot).
            querySelector('iron-list').items;
        assertEquals(
            searchEnginesInfo.extensions.length, extensionEntries.length);
      });

      // Test that the "no other search engines" message is shown/hidden as
      // expected.
      test('NoOtherSearchEnginesMessage', function() {
        var message = page.root.querySelector('#noOtherEngines');
        assertTrue(!!message);
        assertFalse(message.hasAttribute('hidden'));

        cr.webUIListenerCallback('search-engines-changed', {
          defaults: [],
          others: [createSampleSearchEngine()],
          extensions: [],
        });
        assertTrue(message.hasAttribute('hidden'));
      });

      // Tests that the add search engine dialog opens when the corresponding
      // button is tapped.
      test('AddSearchEngineDialog', function() {
        assertFalse(!!page.$$('settings-search-engine-dialog'));
        var addSearchEngineButton = page.$.addSearchEngine;
        assertTrue(!!addSearchEngineButton);

        MockInteractions.tap(addSearchEngineButton);
        Polymer.dom.flush();
        assertTrue(!!page.$$('settings-search-engine-dialog'));
      });
    });
  }

  function registerOmniboxExtensionEntryTests() {
    suite('OmniboxExtensionEntryTests', function() {
      /** @type {?SettingsOmniboxExtensionEntryElement} */
      var entry = null;

      var browserProxy = null;

      setup(function() {
        browserProxy = new TestExtensionControlBrowserProxy();
        settings.ExtensionControlBrowserProxyImpl.instance_ = browserProxy;
        PolymerTest.clearBody();
        entry = document.createElement('settings-omnibox-extension-entry');
        entry.set('engine', createSampleOmniboxExtension());
        document.body.appendChild(entry);

        // Open action menu.
        MockInteractions.tap(entry.$$('paper-icon-button'));
      });

      teardown(function() { entry.remove(); });

      test('Manage', function() {
        var manageButton = entry.$.manage;
        assertTrue(!!manageButton);
        MockInteractions.tap(manageButton);
        return browserProxy.whenCalled('manageExtension').then(
            function(extensionId) {
              assertEquals(entry.engine.extension.id, extensionId);
            });
      });

      test('Disable', function() {
        var disableButton = entry.$.disable;
        assertTrue(!!disableButton);
        MockInteractions.tap(disableButton);
        return browserProxy.whenCalled('disableExtension').then(
            function(extensionId) {
              assertEquals(entry.engine.extension.id, extensionId);
            });
      });
    });
  }

  return {
    registerTests: function() {
      registerDialogTests();
      registerSearchEngineEntryTests();
      registerOmniboxExtensionEntryTests();
      registerPageTests();
    },
  };
});

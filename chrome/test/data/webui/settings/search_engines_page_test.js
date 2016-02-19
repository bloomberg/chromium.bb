// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_search_engines_page', function() {
  /**
   * TODO(dpapad): Similar class exists in webui_resource_async_browsertest.js.
   * Move somewhere else and re-use.
   * @constructor
   */
  var PromiseResolver = function() {
    this.promise = new Promise(function(resolve, reject) {
      this.resolve = resolve;
      this.reject = reject;
    }.bind(this));
  };

  /**
   * A test version of SearchEnginesBrowserProxy. Provides helper methods
   * for allowing tests to know when a method was called, as well as
   * specifying mock responses.
   *
   * @constructor
   * @implements {settings.SearchEnginesBrowserProxy}
   */
  var TestSearchEnginesBrowserProxy = function() {
    /** @private {!Map<string, !PromiseResolver>} */
    this.resolverMap_ = new Map();
    var wrapperMethods = [
      'getSearchEnginesList',
      'removeSearchEngine',
      'searchEngineEditCancelled',
      'searchEngineEditCompleted',
      'searchEngineEditStarted',
      'setDefaultSearchEngine',
      'validateSearchEngineInput',
    ];
    wrapperMethods.forEach(this.resetResolver, this);

    /** @private {!SearchEnginesInfo} */
    this.searchEnginesInfo_ = {defaults: [], others: [], extensions: []};
  };

  TestSearchEnginesBrowserProxy.prototype = {
    /**
     * @param {string} methodName
     * @return {!Promise} A promise that is resolved when the given method
     *     is called.
     */
    whenCalled: function(methodName) {
      return this.resolverMap_.get(methodName).promise;
    },

    /**
     * Resets the PromiseResolver associated with the given method.
     * @param {string} methodName
     */
    resetResolver: function(methodName) {
      this.resolverMap_.set(methodName, new PromiseResolver());
    },

    /** @override */
    setDefaultSearchEngine: function(modelIndex) {
      this.resolverMap_.get('setDefaultSearchEngine').resolve(modelIndex);
    },

    /** @override */
    removeSearchEngine: function(modelIndex) {
      this.resolverMap_.get('removeSearchEngine').resolve(modelIndex);
    },

    /** @override */
    searchEngineEditStarted: function(modelIndex) {
      this.resolverMap_.get('searchEngineEditStarted').resolve(modelIndex);
    },

    /** @override */
    searchEngineEditCancelled: function() {
      this.resolverMap_.get('searchEngineEditCancelled').resolve();
    },

    /** @override */
    searchEngineEditCompleted: function(searchEngine, keyword, queryUrl) {
      this.resolverMap_.get('searchEngineEditCompleted').resolve();
    },

    /**
     * Sets the response to be returned by |getSearchEnginesList|.
     * @param {!SearchEnginesInfo}
     */
    setSearchEnginesInfo: function(searchEnginesInfo) {
      this.searchEnginesInfo_ = searchEnginesInfo;
    },

    /** @override */
    getSearchEnginesList: function() {
      this.resolverMap_.get('getSearchEnginesList').resolve();
      return Promise.resolve(this.searchEnginesInfo_);
    },

    /** @override */
    validateSearchEngineInput: function(fieldName, fieldValue) {
      this.resolverMap_.get('validateSearchEngineInput').resolve();
      return Promise.resolve(true);
    },
  };

  /** @return {!SearchEngine} */
  var createSampleSearchEngine = function() {
    return {
      canBeDefault: false,
      canBeEdited: true,
      canBeRemoved: false,
      default: false,
      displayName: "Google",
      iconURL: "http://www.google.com/favicon.ico",
      isOmniboxExtension: false,
      keyword: "google.com",
      modelIndex: 0,
      name: "Google",
      url: "https://search.foo.com/search?p=%s",
      urlLocked: false,
    };
  };

  function registerDialogTests() {
    suite('AddSearchEngineDialogTests', function() {
      /** @type {?SettingsAddSearchEngineDialog} */
      var dialog = null;
      var browserProxy = null;

      setup(function() {
        browserProxy = new TestSearchEnginesBrowserProxy();
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
              MockInteractions.tap(dialog.$.close);
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
      //  - validation is triggered on 'focus'. 'change' is not teted because
      //    MockInteractions does not currently provide a way to trigger such
      //    events.
      //  - action button is enabled when all fields are valid.
      //  - action button triggers appropriate browser signal when tapped.
      test('DialogAddSearchEngine', function() {
        /**
         * Focuses the given paper-input element and checks that validation is
         * triggered.
         * @param {string} inputId
         * @return {!Promise}
         */
        var focusAndValidate = function(inputId) {
          browserProxy.resetResolver('validateSearchEngineInput');
          MockInteractions.focus(dialog.$[inputId]);
          return browserProxy.whenCalled('validateSearchEngineInput');
        };

        assertEquals('', dialog.$.searchEngine.value);
        assertEquals('', dialog.$.keyword.value);
        assertEquals('', dialog.$.queryUrl.value);
        var actionButton = dialog.$.actionButton;
        assertTrue(actionButton.disabled);

        return focusAndValidate('searchEngine').then(function() {
          return focusAndValidate('keyword');
        }).then(function() {
          return focusAndValidate('queryUrl');
        }).then(function() {
          // Manually set the text to a non-empty string for all fields.
          dialog.$.searchEngine.value = 'foo';
          dialog.$.keyword.value = 'bar';
          dialog.$.queryUrl.value = 'baz';

          // MockInteractions does not provide a way to trigger a 'change' event
          // yet. Triggering the 'focus' event instead, to update the state of
          // the action button.
          return focusAndValidate('searchEngine');
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

  function registerEntryTests() {
    suite('SearchEngineEntryTests', function() {
      /** @type {?SettingsSearchEngineEntryElement} */
      var entry = null;

      var browserProxy = null;

      setup(function() {
        browserProxy = new TestSearchEnginesBrowserProxy();
        settings.SearchEnginesBrowserProxyImpl.instance_ = browserProxy;
        PolymerTest.clearBody();
        entry = document.createElement('settings-search-engine-entry');
        entry.set('engine', createSampleSearchEngine());
        document.body.appendChild(entry);
      });

      teardown(function() { entry.remove(); });

      test('RemoveSearchEngine', function() {
        var deleteButton = entry.$.delete;
        assertTrue(!!deleteButton);
        MockInteractions.tap(deleteButton);
        return browserProxy.whenCalled('removeSearchEngine').then(
            function(modelIndex) {
              assertEquals(entry.engine.modelIndex, modelIndex);
            });
      });

      test('MakeDefault', function() {
        var makeDefaultButton = entry.$.makeDefault;
        assertTrue(!!makeDefaultButton);
        MockInteractions.tap(makeDefaultButton);
        return browserProxy.whenCalled('setDefaultSearchEngine').then(
            function(modelIndex) {
              assertEquals(entry.engine.modelIndex, modelIndex);
            });
      });

      // Test that the "make default" and "remove" buttons are hidden for
      // the default search engine.
      test('DefaultSearchEngineHiddenButtons', function() {
        assertFalse(entry.$.makeDefault.hidden);
        assertFalse(entry.$.edit.hidden);
        assertFalse(entry.$.delete.hidden);

        // Mark the engine as the "default" one.
        var engine = createSampleSearchEngine();
        engine.default = true;
        entry.set('engine', engine);
        Polymer.dom.flush();

        assertTrue(entry.$.makeDefault.hidden);
        assertFalse(entry.$.edit.hidden);
        assertTrue(entry.$.delete.hidden);
      });

      // Test that clicking the "edit" button brings up a dialog.
      test('Edit', function() {
        var engine = entry.engine;
        var editButton = entry.$.edit;
        assertTrue(!!editButton);
        MockInteractions.tap(editButton);
        return browserProxy.whenCalled('searchEngineEditStarted').then(
            function(modelIndex) {
              assertEquals(engine.modelIndex, modelIndex);
              var dialog = entry.$$('settings-search-engine-dialog');
              assertTrue(!!dialog);

              // Check that the paper-input fields are pre-populated.
              assertEquals(engine.displayName, dialog.$.searchEngine.value);
              assertEquals(engine.keyword, dialog.$.keyword.value);
              assertEquals(engine.url, dialog.$.queryUrl.value);

              assertFalse(dialog.$.actionButton.disabled);
            });
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
        extensions: [],
      };

      setup(function() {
        browserProxy = new TestSearchEnginesBrowserProxy();
        browserProxy.setSearchEnginesInfo(searchEnginesInfo);
        settings.SearchEnginesBrowserProxyImpl.instance_ = browserProxy;
        PolymerTest.clearBody();
        page = document.createElement('settings-search-engines-page');
        document.body.appendChild(page);
      });

      teardown(function() { page.remove(); });

      // Tests that the page is querying and displaying search engine info on
      // startup.
      test('Initialization', function() {
        return browserProxy.whenCalled('getSearchEnginesList').then(function() {
          var searchEnginesLists = Polymer.dom(page.shadowRoot).
              querySelectorAll('settings-search-engines-list');
          assertEquals(2, searchEnginesLists.length);

          Polymer.dom.flush();
          var defaultsList = searchEnginesLists[0];
          var defaultsEntries = Polymer.dom(defaultsList.shadowRoot).
              querySelectorAll('settings-search-engine-entry');
          assertEquals(
              searchEnginesInfo.defaults.length, defaultsEntries.length);

          var othersList = searchEnginesLists[1];
          var othersEntries = Polymer.dom(othersList.shadowRoot).
              querySelectorAll('settings-search-engine-entry');
          assertEquals(
              searchEnginesInfo.others.length, othersEntries.length);
        });
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

  return {
    registerDialogTests: registerDialogTests,
    registerEntryTests: registerEntryTests,
    registerPageTests: registerPageTests,
  };
});

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_search', function() {
  /**
   * A test version of SearchEnginesBrowserProxy. Provides helper methods
   * for allowing tests to know when a method was called, as well as
   * specifying mock responses.
   *
   * @constructor
   * @implements {settings.SearchEnginesBrowserProxy}
   * @extends {settings.TestBrowserProxy}
   */
  var TestSearchEnginesBrowserProxy = function() {
    settings.TestBrowserProxy.call(this, [
      'getSearchEnginesList',
      'removeSearchEngine',
      'searchEngineEditCancelled',
      'searchEngineEditCompleted',
      'searchEngineEditStarted',
      'setDefaultSearchEngine',
      'validateSearchEngineInput',
      'manageExtension',
      'disableExtension',
    ]);

    /** @private {!SearchEnginesInfo} */
    this.searchEnginesInfo_ = {defaults: [], others: [], extensions: []};
  };

  TestSearchEnginesBrowserProxy.prototype = {
    __proto__: settings.TestBrowserProxy.prototype,

    /** @override */
    setDefaultSearchEngine: function(modelIndex) {
      this.methodCalled('setDefaultSearchEngine', modelIndex);
    },

    /** @override */
    removeSearchEngine: function(modelIndex) {
      this.methodCalled('removeSearchEngine', modelIndex);
    },

    /** @override */
    searchEngineEditStarted: function(modelIndex) {
      this.methodCalled('searchEngineEditStarted', modelIndex);
    },

    /** @override */
    searchEngineEditCancelled: function() {
      this.methodCalled('searchEngineEditCancelled');
    },

    /** @override */
    searchEngineEditCompleted: function(searchEngine, keyword, queryUrl) {
      this.methodCalled('searchEngineEditCompleted');
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
      this.methodCalled('getSearchEnginesList');
      return Promise.resolve(this.searchEnginesInfo_);
    },

    /** @override */
    validateSearchEngineInput: function(fieldName, fieldValue) {
      this.methodCalled('validateSearchEngineInput');
      return Promise.resolve(true);
    },

    /** @override */
    manageExtension: function(extensionId) {
      this.methodCalled('manageExtension', extensionId);
    },

    /** @override */
    disableExtension: function(extensionId) {
      this.methodCalled('disableExtension', extensionId);
    },
  };

  /**
   * @param {boolean} canBeDefault
   * @param {boolean} canBeEdited
   * @param {boolean} canBeRemoved
   * @return {!SearchEngine}
   */
  function createSampleSearchEngine(canBeDefault, canBeEdited, canBeRemoved) {
    return {
      canBeDefault: canBeDefault,
      canBeEdited: canBeEdited,
      canBeRemoved: canBeRemoved,
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
  }

  return {
    createSampleSearchEngine: createSampleSearchEngine,
    TestSearchEnginesBrowserProxy: TestSearchEnginesBrowserProxy,
  };
});

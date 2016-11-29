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
      'getHotwordInfo',
      'setHotwordSearchEnabled',
    ]);

    /** @type {boolean} */
    this.hotwordSearchEnabled = false;

    /** @private {!SearchEnginesInfo} */
    this.searchEnginesInfo_ = {defaults: [], others: [], extensions: []};

    /** @private {!SearchPageHotwordInfo} */
    this.hotwordInfo_ = {
      allowed: true,
      enabled: false,
      alwaysOn: true,
      errorMessage: '',
      userName: 'user@test.org',
      historyEnabled: false,
    };
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
    getHotwordInfo: function() {
      this.methodCalled('getHotwordInfo');
      return Promise.resolve(this.hotwordInfo_);
    },

    /** @override */
    setHotwordSearchEnabled: function(enabled) {
      this.hotwordSearchEnabled = enabled;
      this.hotwordInfo_.enabled = true;
      this.hotwordInfo_.historyEnabled = this.hotwordInfo_.alwaysOn;
      this.methodCalled('setHotwordSearchEnabled');
    },

    /**
     * Sets the response to be returned by |getSearchEnginesList|.
     * @param {!SearchEnginesInfo} searchEnginesInfo
     */
    setSearchEnginesInfo: function(searchEnginesInfo) {
      this.searchEnginesInfo_ = searchEnginesInfo;
    },

    /**
     * Sets the response to be returned by |getSearchEnginesList|.
     * @param {!SearchPageHotwordInfo} hotwordInfo
     */
    setHotwordInfo: function(hotwordInfo) {
      this.hotwordInfo_ = hotwordInfo;
      cr.webUIListenerCallback('hotword-info-update', this.hotwordInfo_);
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

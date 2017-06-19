// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "Manage search engines" section
 * to interact with the browser.
 */

/**
 * @typedef {{canBeDefault: boolean,
 *            canBeEdited: boolean,
 *            canBeRemoved: boolean,
 *            default: boolean,
 *            displayName: string,
 *            extension: ({id: string,
 *                         name: string,
 *                         canBeDisabled: boolean,
 *                         icon: string}|undefined),
 *            iconURL: (string|undefined),
 *            isOmniboxExtension: boolean,
 *            keyword: string,
 *            modelIndex: number,
 *            name: string,
 *            url: string,
 *            urlLocked: boolean}}
 * @see chrome/browser/ui/webui/settings/search_engine_manager_handler.cc
 */
var SearchEngine;

/**
 * @typedef {{
 *   defaults: !Array<!SearchEngine>,
 *   others: !Array<!SearchEngine>,
 *   extensions: !Array<!SearchEngine>
 * }}
 */
var SearchEnginesInfo;

/**
 * @typedef {{
 *   allowed: boolean,
 *   enabled: boolean,
 *   alwaysOn: boolean,
 *   errorMessage: string,
 *   userName: string,
 *   historyEnabled: boolean
 * }}
 */
var SearchPageHotwordInfo;

cr.define('settings', function() {
  /** @interface */
  function SearchEnginesBrowserProxy() {}

  SearchEnginesBrowserProxy.prototype = {
    /** @param {number} modelIndex */
    setDefaultSearchEngine: function(modelIndex) {},

    /** @param {number} modelIndex */
    removeSearchEngine: function(modelIndex) {},

    /** @param {number} modelIndex */
    searchEngineEditStarted: function(modelIndex) {},

    searchEngineEditCancelled: function() {},

    /**
     * @param {string} searchEngine
     * @param {string} keyword
     * @param {string} queryUrl
     */
    searchEngineEditCompleted: function(searchEngine, keyword, queryUrl) {},

    /** @return {!Promise<!SearchEnginesInfo>} */
    getSearchEnginesList: function() {},

    /**
     * @param {string} fieldName
     * @param {string} fieldValue
     * @return {!Promise<boolean>}
     */
    validateSearchEngineInput: function(fieldName, fieldValue) {},

    /** @return {!Promise<!SearchPageHotwordInfo>} */
    getHotwordInfo: function() {},

    /** @param {boolean} enabled */
    setHotwordSearchEnabled: function(enabled) {},

    /** @return {!Promise<boolean>} */
    getGoogleNowAvailability: function() {},
  };

  /**
   * @constructor
   * @implements {settings.SearchEnginesBrowserProxy}
   */
  function SearchEnginesBrowserProxyImpl() {}
  // The singleton instance_ is replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(SearchEnginesBrowserProxyImpl);

  SearchEnginesBrowserProxyImpl.prototype = {
    /** @override */
    setDefaultSearchEngine: function(modelIndex) {
      chrome.send('setDefaultSearchEngine', [modelIndex]);
    },

    /** @override */
    removeSearchEngine: function(modelIndex) {
      chrome.send('removeSearchEngine', [modelIndex]);
    },

    /** @override */
    searchEngineEditStarted: function(modelIndex) {
      chrome.send('searchEngineEditStarted', [modelIndex]);
    },

    /** @override */
    searchEngineEditCancelled: function() {
      chrome.send('searchEngineEditCancelled');
    },

    /** @override */
    searchEngineEditCompleted: function(searchEngine, keyword, queryUrl) {
      chrome.send('searchEngineEditCompleted', [
        searchEngine,
        keyword,
        queryUrl,
      ]);
    },

    /** @override */
    getSearchEnginesList: function() {
      return cr.sendWithPromise('getSearchEnginesList');
    },

    /** @override */
    validateSearchEngineInput: function(fieldName, fieldValue) {
      return cr.sendWithPromise(
          'validateSearchEngineInput', fieldName, fieldValue);
    },

    /** @override */
    getHotwordInfo: function() {
      return cr.sendWithPromise('getHotwordInfo');
    },

    /** @override */
    setHotwordSearchEnabled: function(enabled) {
      chrome.send('setHotwordSearchEnabled', [enabled]);
    },

    /** @override */
    getGoogleNowAvailability: function() {
      return cr.sendWithPromise('getGoogleNowAvailability');
    },
  };

  return {
    SearchEnginesBrowserProxy: SearchEnginesBrowserProxy,
    SearchEnginesBrowserProxyImpl: SearchEnginesBrowserProxyImpl,
  };
});

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-search-page' is the settings page containing search settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <settings-search-page prefs="{{prefs}}"></settings-search-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element settings-search-page
 */
Polymer({
  is: 'settings-search-page',

  properties: {
    /**
     * The current active route.
     */
    currentRoute: {
      type: Object,
      notify: true,
    },

    /**
     * List of default search engines available.
     * @type {?Array<!SearchEngine>}
     */
    searchEngines: {
      type: Array,
      value: function() { return []; },
    },
  },

  /** @override */
  created: function() {
    chrome.searchEnginesPrivate.onSearchEnginesChanged.addListener(
        this.updateSearchEngines_.bind(this));
    chrome.searchEnginesPrivate.getSearchEngines(
        this.updateSearchEngines_.bind(this));
  },

  /**
   * Persists the new default search engine back to Chrome. Called when the
   * user selects a new default in the search engines dropdown.
   * @private
   */
  defaultEngineGuidChanged_: function() {
    chrome.searchEnginesPrivate.setSelectedSearchEngine(
        this.$.searchEnginesMenu.value);
  },


  /**
   * Updates the list of default search engines based on the given |engines|.
   * @param {!Array<!SearchEngine>} engines All the search engines.
   * @private
   */
  updateSearchEngines_: function(engines) {
    var defaultEngines = [];

    engines.forEach(function(engine) {
      if (engine.type ==
          chrome.searchEnginesPrivate.SearchEngineType.DEFAULT) {
        defaultEngines.push(engine);
        if (engine.isSelected) {
          this.$.searchEnginesMenu.value = engine.guid;
        }
      }
    }, this);

    this.searchEngines = defaultEngines;
  },

  /** @private */
  onSearchEnginesTap_: function() {
    this.$.pages.setSubpageChain(['search-engines']);
  },
});

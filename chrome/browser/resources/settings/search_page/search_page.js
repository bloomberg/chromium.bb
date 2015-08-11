// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-search-page' is the settings page containing search settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <cr-settings-search-page prefs="{{prefs}}"></cr-settings-search-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-search-page
 */
Polymer({
  is: 'cr-settings-search-page',

  properties: {
    /**
     * Route for the page.
     */
    route: String,

    /**
     * Whether the page is a subpage.
     */
    subpage: {
      type: Boolean,
      value: false,
    },

    /**
     * ID of the page.
     */
    PAGE_ID: {
      type: String,
      value: 'search',
    },

    /**
     * Title for the page header and navigation menu.
     */
    pageTitle: {
      type: String,
      value: function() { return loadTimeData.getString('searchPageTitle'); },
    },

    /**
     * Name of the 'iron-icon' to be shown in the settings-page-header.
     */
    icon: {
      type: Boolean,
      value: 'search',
    },

    /**
     * List of default search engines available.
     * @type {?Array<!SearchEngine>}
     */
    searchEngines: {
      type: Array,
      value: function() { return []; },
    },

    inSubpage: {
      type: Boolean,
      notify: true,
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
  manageSearchEngines_: function() {
    this.$.pages.navigateTo('search-engines');
  },

  /** @private */
  handleBack_: function() {
    this.$.pages.back();
  },
});

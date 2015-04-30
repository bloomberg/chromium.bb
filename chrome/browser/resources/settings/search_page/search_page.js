// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-search-page' is the settings page containing search settings.
 *
 * Example:
 *
 *    <core-animated-pages>
 *      <cr-settings-search-page prefs="{{prefs}}"></cr-settings-search-page>
 *      ... other pages ...
 *    </core-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-search-page
 */
Polymer('cr-settings-search-page', {
  publish: {
    /**
     * Preferences state.
     *
     * @attribute prefs
     * @type {CrSettingsPrefsElement}
     * @default null
     */
    prefs: null,

    /**
     * Whether the page is a subpage.
     *
     * @attribute subpage
     * @type {boolean}
     * @default false
     */
    subpage: false,

    /**
     * ID of the page.
     *
     * @attribute PAGE_ID
     * @const {string}
     * @default 'search'
     */
    PAGE_ID: 'search',

    /**
     * Title for the page header and navigation menu.
     *
     * @attribute pageTitle
     * @type {string}
     */
    pageTitle: loadTimeData.getString('searchPageTitle'),

    /**
     * Name of the 'core-icon' to be shown in the settings-page-header.
     *
     * @attribute icon
     * @type {string}
     * @default 'search'
     */
    icon: 'search',

    /**
     * List of default search engines available.
     *
     * @attribute searchEngines
     * @type {Array<!SearchEngine}
     * @default null
     */
    searchEngines: null,

    /**
     * GUID of the currently selected default search engine.
     *
     * @attribute defaultEngineGuid
     * @type {string}
     * @default ''
     */
    defaultEngineGuid: '',
  },

  /** @override */
  created: function() {
    this.searchEngines = [];
  },

  /** @override */
  domReady: function() {
    chrome.searchEnginesPrivate.onSearchEnginesChanged.addListener(
        this.updateSearchEngines_.bind(this));
    chrome.searchEnginesPrivate.getSearchEngines(
        this.updateSearchEngines_.bind(this));
  },

  /**
   * Persists the new default search engine back to Chrome. Called when the
   * user selects a new default in the search engines dropdown.
   */
  defaultEngineGuidChanged: function() {
    chrome.searchEnginesPrivate.setSelectedSearchEngine(this.defaultEngineGuid);
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
          this.defaultEngineGuid = engine.guid;
        }
      }
    }, this);

    this.searchEngines = defaultEngines;
  }
});

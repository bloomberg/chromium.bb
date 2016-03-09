// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-search-page' is the settings page containing search settings.
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
     * @private {!Array<!SearchEngine>}
     */
    searchEngines_: {
      type: Array,
      value: function() { return []; }
    },

    /** @private {!settings.SearchEnginesBrowserProxy} */
    browserProxy_: Object,
  },

  /** @override */
  created: function() {
    this.browserProxy_ = settings.SearchEnginesBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready: function() {
    var updateSearchEngines = function(searchEngines) {
      this.set('searchEngines_', searchEngines.defaults);
    }.bind(this);
    this.browserProxy_.getSearchEnginesList().then(updateSearchEngines);
    cr.addWebUIListener('search-engines-changed', updateSearchEngines);
  },

  /** @private */
  onManageSearchEnginesTap_: function() {
    this.$.pages.setSubpageChain(['search-engines']);
  },

  /** @private */
  onDefaultEngineChanged_: function() {
    this.browserProxy_.setDefaultSearchEngine(this.$.searchEnginesMenu.value);
  },
});

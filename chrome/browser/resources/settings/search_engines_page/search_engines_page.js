// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-engines-page' is the settings page
 * containing search engines settings.
 *
 * Example:
 *
 *    <core-animated-pages>
 *      <settings-search-engines-page prefs="{{prefs}}">
 *      </settings-search-engines-page>
 *      ... other pages ...
 *    </core-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element settings-search-engines-page
 */
Polymer({
  is: 'settings-search-engines-page',

  properties: {
    /** @type {!Array<!SearchEngine>} */
    defaultEngines: {
      type: Array,
      value: function() { return []; }
    },

    /** @type {!Array<!SearchEngine>} */
    otherEngines: {
      type: Array,
      value: function() { return []; }
    },
  },

  /** @override */
  ready: function() {
    chrome.searchEnginesPrivate.onSearchEnginesChanged.addListener(
        this.enginesChanged_.bind(this));
    this.enginesChanged_();
  },

  /** @private */
  enginesChanged_: function() {
    chrome.searchEnginesPrivate.getSearchEngines(function(engines) {
      this.defaultEngines = engines.filter(function(engine) {
        return engine.type ==
            chrome.searchEnginesPrivate.SearchEngineType.DEFAULT;
      }, this);

      this.otherEngines = engines.filter(function(engine) {
        return engine.type ==
            chrome.searchEnginesPrivate.SearchEngineType.OTHER;
      }, this);
    }.bind(this));
  }
});

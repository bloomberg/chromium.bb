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

    /** @type {boolean} */
    showAddSearchEngineDialog_: {
      type: Boolean,
      value: false,
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
  },

  /** @private */
  onAddSearchEngineTap_: function() {
    this.showAddSearchEngineDialog_ = true;
    this.async(function() {
      var dialog = this.$$('settings-add-search-engine-dialog');
      // Register listener to detect when the dialog is closed. Flip the boolean
      // once closed to force a restamp next time it is shown such that the
      // previous dialog's contents are cleared.
      dialog.addEventListener('iron-overlay-closed', function() {
        this.showAddSearchEngineDialog_ = false;
      }.bind(this));
      dialog.open();
    }.bind(this));
  },
});

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-engines-page' is the settings page
 * containing search engines settings.
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

    /** @private {boolean} */
    showAddSearchEngineDialog_: Boolean,

    /** @private {boolean} */
    otherSearchEnginesExpanded_: {
      type: Boolean,
      value: true,
    },
  },

  /**
   * Holds WebUI listeners that need to be removed when this element is
   * destroyed.
   * TODO(dpapad): Move listener tracking logic to a Polymer behavior class,
   * such that it can be re-used.
   * @private {!Array<!WebUIListener>}
   */
  webUIListeners_: [],

  /** @override */
  ready: function() {
    settings.SearchEnginesBrowserProxyImpl.getInstance().
        getSearchEnginesList().then(this.enginesChanged_.bind(this));
    this.webUIListeners_.push(cr.addWebUIListener(
        'search-engines-changed', this.enginesChanged_.bind(this)));
  },

  /** @override */
  detached: function() {
    this.webUIListeners_.forEach(function(listener) {
      cr.removeWebUIListener(listener);
    });
  },

  /**
   * @param {!SearchEnginesInfo} searchEnginesInfo
   * @private
   */
  enginesChanged_: function(searchEnginesInfo) {
    this.defaultEngines = searchEnginesInfo['defaults'];
    this.otherEngines = searchEnginesInfo['others'];
    // TODO(dpapad): Use searchEnginesInfo['extensions'] once UI mocks are
    // provided.
  },

  /** @private */
  onAddSearchEngineTap_: function() {
    this.showAddSearchEngineDialog_ = true;
    this.async(function() {
      var dialog = this.$$('settings-search-engine-dialog');
      // Register listener to detect when the dialog is closed. Flip the boolean
      // once closed to force a restamp next time it is shown such that the
      // previous dialog's contents are cleared.
      dialog.addEventListener('iron-overlay-closed', function() {
        this.showAddSearchEngineDialog_ = false;
      }.bind(this));
    }.bind(this));
  },
});

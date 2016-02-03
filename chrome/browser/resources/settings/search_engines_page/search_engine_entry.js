// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-engine-entry' is a component for showing a
 * search engine with its name, domain and query URL.
 *
 * @group Chrome Settings Elements
 * @element settings-search-engine-entry
 */
Polymer({
  is: 'settings-search-engine-entry',

  properties: {
    /** @type {!SearchEngine} */
    engine: Object
  },

  /** @private */
  onDeleteTap_: function() {
    chrome.searchEnginesPrivate.removeSearchEngine(this.engine.guid);
  },

  /** @private */
  onEditTap_: function() {
    // TODO(dpapad): Implement edit mode.
  },

  /** @private */
  onMakeDefaultTap_: function() {
    chrome.searchEnginesPrivate.setSelectedSearchEngine(this.engine.guid);
    var popupMenu = this.$$('iron-dropdown');
    popupMenu.close();
  },
});

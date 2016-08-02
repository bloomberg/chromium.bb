// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'history-side-bar',

  properties: {
    selectedPage: {
      type: String,
      notify: true
    },

    route: Object,

    showFooter: Boolean,

    // If true, the sidebar is contained within an app-drawer.
    drawer: {
      type: Boolean,
      reflectToAttribute: true
    },
  },

  /** @private */
  onSelectorActivate_: function() {
    this.fire('history-close-drawer');
  },

  /**
   * Relocates the user to the clear browsing data section of the settings page.
   * @param {Event} e
   * @private
   */
  onClearBrowsingDataTap_: function(e) {
    md_history.BrowserService.getInstance().openClearBrowsingData();
    e.preventDefault();
  },

  /**
   * @param {Object} route
   * @private
   */
  getQueryString_: function(route) {
    return window.location.search;
  }
});

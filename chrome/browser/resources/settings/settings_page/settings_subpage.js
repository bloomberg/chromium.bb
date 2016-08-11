// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-subpage' shows a subpage beneath a subheader. The header contains
 * the subpage title and a back icon.
 */

Polymer({
  is: 'settings-subpage',

  behaviors: [
    // TODO(michaelpg): phase out NeonAnimatableBehavior.
    Polymer.NeonAnimatableBehavior,
    Polymer.IronResizableBehavior,
  ],

  properties: {
    pageTitle: String,

    /** Setting a |searchLabel| will enable search. */
    searchLabel: String,

    searchTerm: {
      type: String,
      notify: true,
      value: '',
    },

    /**
     * Indicates which element triggers this subpage. Used by the searching
     * algorithm to show search bubbles. It is |null| for subpages that are
     * skipped during searching.
     * @type {?HTMLElement}
     */
    associatedControl: {
      type: Object,
      value: null,
    },
  },

  /** @private */
  onTapBack_: function() {
    var previousRoute =
        window.history.state &&
        assert(settings.getRouteForPath(
            /** @type {string} */ (window.history.state)));

    if (previousRoute && previousRoute.contains(settings.getCurrentRoute()))
      window.history.back();
    else
      settings.navigateTo(assert(settings.getCurrentRoute().parent));
  },

  /** @private */
  onSearchChanged_: function(e) {
    this.searchTerm = e.detail;
  },
});

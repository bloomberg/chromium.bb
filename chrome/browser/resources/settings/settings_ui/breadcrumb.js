// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-breadcrumb' displays the breadcrumb.
 *
 * Example:
 *
 *     <settings-breadcrumb current-route="{{currentRoute}}">
 *     </settings-breadcrumb>
 *
 * @group Chrome Settings Elements
 * @element settings-breadcrumb
 */
Polymer({
  is: 'settings-breadcrumb',

  properties: {
    /**
     * The current active route.
     */
    currentRoute: {
      type: Object,
      notify: true,
    },

    /**
     * Page titles for the currently active route.
     */
    currentRouteTitles: {
      type: Object,
    },
  },

  /** @private */
  onTapPage_: function() {
    this.currentRoute = {
      page: this.currentRoute.page,
      section: '',
      subpage: [],
    };
  },

  /** @private */
  onTapSubpage_: function(event) {
    var clickedIndex = event.target.dataset.subpageIndex;
    this.currentRoute = {
      page: this.currentRoute.page,
      section: this.currentRoute.section,
      subpage: this.currentRoute.subpage.slice(0, clickedIndex + 1),
    };
  },
});

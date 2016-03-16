// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-main' displays the selected settings page.
 *
 * Example:
 *
 *     <settings-main pages="[[pages]]" selected-page-id="{{selectedId}}">
 *     </settings-main>
 *
 * See settings-drawer for example of use in 'paper-drawer-panel'.
 */
Polymer({
  is: 'settings-main',

  properties: {
    /**
     * Preferences state.
     *
     * @type {?CrSettingsPrefsElement}
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * The current active route.
     */
    currentRoute: {
      type: Object,
      notify: true,
      observer: 'currentRouteChanged_',
    },

    // If false the 'basic' page should be shown.
    showAdvancedPage_: {
      type: Boolean,
      value: false
    }
  },

  /** @private */
  currentRouteChanged_: function(newRoute, oldRoute) {
    this.showAdvancedPage_ = newRoute.page == 'advanced';
  },
});

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

    /** @private */
    showAdvancedPage_: Boolean,

    /** @private */
    showBasicPage_: Boolean,

    /** @private */
    showAboutPage_: Boolean,
  },

  /**
   * @param {!SettingsRoute} newRoute
   * @private
   */
  currentRouteChanged_: function(newRoute) {
    this.showAboutPage_ = newRoute.page == 'about';
    this.showAdvancedPage_ = newRoute.page == 'advanced';
    this.showBasicPage_ = newRoute.page == 'basic';
  },
});

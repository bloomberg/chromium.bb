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
 *
 * @group Chrome Settings Elements
 * @element settings-main
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

  listeners: {
    'expand-animation-complete': 'onExpandAnimationComplete_',
  },

  /** @private */
  currentRouteChanged_: function(newRoute, oldRoute) {
    this.showAdvancedPage_ = newRoute.page == 'advanced';

    var pageContainer = this.$.pageContainer;
    if (!oldRoute) {
      pageContainer.classList.toggle('expanded', newRoute.section);
      return;
    }

    // For contraction only, apply new styling immediately.
    if (!newRoute.section && oldRoute.section) {
      pageContainer.classList.remove('expanded');

      // TODO(tommycli): Save and restore scroll position. crbug.com/537359.
      pageContainer.scrollTop = 0;
    }
  },

  /** @private */
  onExpandAnimationComplete_: function() {
    if (this.currentRoute.section) {
      var pageContainer = this.$.pageContainer;
      pageContainer.classList.add('expanded');
      pageContainer.scrollTop = 0;
    }
  },
});

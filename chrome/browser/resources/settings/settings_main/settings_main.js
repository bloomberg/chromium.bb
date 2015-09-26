// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-main' displays the selected settings page.
 *
 * Example:
 *
 *     <cr-settings-main pages="[[pages]]" selected-page-id="{{selectedId}}">
 *     </cr-settings-main>
 *
 * See cr-settings-drawer for example of use in 'paper-drawer-panel'.
 *
 * @group Chrome Settings Elements
 * @element cr-settings-main
 */
Polymer({
  is: 'cr-settings-main',

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
    },

    /**
     * Container that determines the sizing of expanded sections.
     */
    expandContainer: {
      type: Object,
    },
  },

  /** @override */
  ready: function() {
    this.expandContainer = this.$.pageContainer;
  },

  /** @private */
  getSelectedPage_: function(currentRoute) {
    return currentRoute.page || 'basic';
  },
});

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-drawer' holds the user card and navigation menu for settings
 * pages.
 *
 * Example:
 *
 *     <paper-drawer-panel>
 *       <cr-settings-drawer drawer selected-id="{{selectedId}}"
 *           pages="[[pages]]">
 *       </cr-settings-drawer>
 *       <cr-settings-main main selected-id="{{selectedId}}" pages="[[pages]]">
 *       </cr-settings-main>
 *     </paper-drawer-panel>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-drawer
 */
Polymer({
  is: 'cr-settings-drawer',

  properties: {
    /**
     * Pages to include in the navigation.
     * @type {!Array<!HTMLElement>}
     */
    pages: {
      type: Array,
      value: function() { return []; },
    },

    /**
     * ID of the currently selected page.
     * @type {string}
     */
    selectedId: {
      type: String,
      notify: true,
    },

    /**
     * @private {!Object}
     * TODO(michaelpg): Create custom element and data source for user card.
     */
    user_: {
      type: Object,
      value: function() {
        return {
          name: 'Chrome User',
          email: 'user@example.com',
          iconUrl: 'chrome://theme/IDR_PROFILE_AVATAR_23@1x',
        };
      },
      readOnly: true,
    },
  },
});

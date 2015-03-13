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
 *     <core-drawer-panel>
 *       <cr-settings-drawer drawer selectedId="{{selectedId}}"
 *           pages="{{pages}}">
 *       </cr-settings-drawer>
 *       <cr-settings-main main selectedId="{{selectedId}}" pages="{{pages}}">
 *       </cr-settings-main>
 *     </core-drawer-panel>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-drawer
 */
Polymer('cr-settings-drawer', {
  publish: {
    /**
     * Pages to include in the navigation.
     *
     * @attribute pages
     * @type Array<!Object>
     * @default null
     */
    pages: null,

    /**
     * ID of the currently selected page.
     *
     * @attribute selectedId
     * @type string
     * default ''
     */
    selectedId: '',
  },

  /** @override */
  created: function() {
    this.pages = [];
  },

  /**
   * @type {Object}
   * TODO(michaelpg): Create custom element and data source for user card.
   */
  user: {
    name: 'Chrome User',
    email: 'user@example.com',
    iconUrl: 'chrome://theme/IDR_PROFILE_AVATAR_23@1x',
  },
});

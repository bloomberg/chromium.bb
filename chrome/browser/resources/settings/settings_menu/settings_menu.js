// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-menu' shows a menu with the given pages.
 *
 * Example:
 *
 *     <cr-settings-menu pages="{{pages}}" selectedId="{{selectedId}}">
 *     </cr-settings-menu>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-menu
 */
Polymer('cr-settings-menu', {
  publish: {
    /**
     * Pages to show menu items for.
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
});

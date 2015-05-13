// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-menu' shows a menu with the given pages.
 *
 * Example:
 *
 *     <cr-settings-menu pages="[[pages]]" selected-id="{{selectedId}}">
 *     </cr-settings-menu>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-menu
 */
Polymer({
  is: 'cr-settings-menu',

  properties: {
    /**
     * Pages to show menu items for.
     * @type {!Array<!HTMLElement>}
     */
    pages: {
      type: Array,
      value: function() { return []; },
    },

    /**
     * ID of the currently selected page.
     */
    selectedId: {
      type: String,
      value: '',
      notify: true,
    },
  },
});

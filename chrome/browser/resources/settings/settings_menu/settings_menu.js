// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-menu' shows a menu with a hardcoded set of pages and subpages.
 *
 * Example:
 *
 *     <cr-settings-menu selected-page-id="{{selectedPageId}}">
 *     </cr-settings-menu>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-menu
 */
Polymer({
  is: 'cr-settings-menu',

  properties: {
    /**
     * ID of the currently selected page.
     */
    selectedPageId: {
      type: String,
      notify: true,
      observer: 'selectedPageIdChanged_',
    },
  },

  ready: function() {
    this.addEventListener('paper-submenu-open', function(event) {
      this.selectedPageId = event.path[0].dataset.page;
    });
  },

  /** @private */
  selectedPageIdChanged_: function() {
    var submenus = this.shadowRoot.querySelectorAll('paper-submenu');
    for (var i = 0; i < submenus.length; ++i) {
      var node = submenus[i];
      node.opened = node.dataset.page == this.selectedPageId;
    }
  },
});

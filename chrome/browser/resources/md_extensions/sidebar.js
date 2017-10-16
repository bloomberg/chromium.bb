// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
cr.define('extensions', function() {
  const Sidebar = Polymer({
    is: 'extensions-sidebar',

    properties: {
      /** @private {number} */
      selected_: {
        type: Number,
        value: -1,
      },
    },

    hostAttributes: {
      role: 'navigation',
    },

    /** @override */
    attached: function() {
      this.selected_ =
          extensions.navigation.getCurrentPage().page == Page.SHORTCUTS ? 1 : 0;
    },

    /** @private */
    onExtensionsTap_: function() {
      extensions.navigation.navigateTo({page: Page.LIST});
    },

    /** @private */
    onKeyboardShortcutsTap_: function() {
      extensions.navigation.navigateTo({page: Page.SHORTCUTS});
    },
  });

  return {Sidebar: Sidebar};
});

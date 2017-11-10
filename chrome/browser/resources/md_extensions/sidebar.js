// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
cr.define('extensions', function() {
  const Sidebar = Polymer({
    is: 'extensions-sidebar',

    hostAttributes: {
      role: 'navigation',
    },

    /** @override */
    attached: function() {
      this.$.sectionMenu.select(
          extensions.navigation.getCurrentPage().page == Page.SHORTCUTS ? 1 :
                                                                          0);
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

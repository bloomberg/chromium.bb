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

    /** @private */
    onExtensionsTap_: function() {
      extensions.navigation.navigateTo(
          {page: Page.LIST, type: extensions.ShowingType.EXTENSIONS});
    },

    /** @private */
    onAppsTap_: function() {
      extensions.navigation.navigateTo(
          {page: Page.LIST, type: extensions.ShowingType.APPS});
    },

    /** @private */
    onKeyboardShortcutsTap_: function() {
      extensions.navigation.navigateTo({page: Page.SHORTCUTS});
    },

    /**
     * @param {!PageState} state
     */
    updateSelected: function(state) {
      let selected;

      switch (state.page) {
        case Page.LIST:
          if (state.type == extensions.ShowingType.APPS)
            selected = 1;
          else
            selected = 0;
          break;
        case Page.SHORTCUTS:
          selected = 2;
          break;
        default:
          selected = -1;
          break;
      }

      this.selected_ = selected;
    },
  });

  return {Sidebar: Sidebar};
});

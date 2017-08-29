// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
cr.define('extensions', function() {
  const Sidebar = Polymer({
    is: 'extensions-sidebar',

    behaviors: [I18nBehavior],

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
  });

  return {Sidebar: Sidebar};
});

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('extensions');

// Declare this here to make closure compiler happy, and us sad.
/** @enum {number} */
extensions.ShowingType = {
  EXTENSIONS: 0,
  APPS: 1,
};

cr.define('extensions', function() {
  /** @interface */
  var SidebarListDelegate = function() {};

  SidebarListDelegate.prototype = {
    /**
     * Shows the given type of item.
     * @param {extensions.ShowingType} type
     */
    showType: assertNotReached,

    /** Shows the keyboard shortcuts page. */
    showKeyboardShortcuts: assertNotReached,
  };

  var Sidebar = Polymer({
    is: 'extensions-sidebar',

    behaviors: [I18nBehavior],

    /** @param {extensions.SidebarListDelegate} listDelegate */
    setListDelegate: function(listDelegate) {
      /** @private {extensions.SidebarListDelegate} */
      this.listDelegate_ = listDelegate;
    },

    /** @private */
    onExtensionsTap_: function() {
      this.listDelegate_.showType(extensions.ShowingType.EXTENSIONS);
    },

    /** @private */
    onAppsTap_: function() {
      this.listDelegate_.showType(extensions.ShowingType.APPS);
    },

    /** @private */
    onKeyboardShortcutsTap_: function() {
      this.listDelegate_.showKeyboardShortcuts();
    },
  });

  return {
    Sidebar: Sidebar,
    SidebarListDelegate: SidebarListDelegate,
  };
});

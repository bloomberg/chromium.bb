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
  var SidebarDelegate = function() {};

  SidebarDelegate.prototype = {
    /**
     * Toggles whether or not the profile is in developer mode.
     * @param {boolean} inDevMode
     */
    setProfileInDevMode: assertNotReached,

    /** Opens the dialog to load unpacked extensions. */
    loadUnpacked: assertNotReached,

    /** Opens the dialog to pack an extension. */
    packExtension: assertNotReached,

    /** Updates all extensions. */
    updateAllExtensions: assertNotReached,
  };

  /** @interface */
  var SidebarListDelegate = function() {};

  SidebarListDelegate.prototype = {
    /**
     * Shows the given type of item.
     * @param {extensions.ShowingType} type
     */
    showType: assertNotReached,
  };

  var Sidebar = Polymer({
    is: 'extensions-sidebar',

    properties: {
      inDevMode: {
        type: Boolean,
        value: false,
      },
    },

    behaviors: [I18nBehavior],

    /** @param {extensions.SidebarDelegate} delegate */
    setDelegate: function(delegate) {
      /** @private {extensions.SidebarDelegate} */
      this.delegate_ = delegate;
    },

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
    onDevModeChange_: function() {
      this.delegate_.setProfileInDevMode(
          this.$['developer-mode-checkbox'].checked);
    },

    /** @private */
    onLoadUnpackedTap_: function() {
      this.delegate_.loadUnpacked();
    },

    /** @private */
    onPackTap_: function() {
      this.delegate_.packExtension();
    },

    /** @private */
    onUpdateNowTap_: function() {
      this.delegate_.updateAllExtensions();
    },
  });

  return {
    Sidebar: Sidebar,
    SidebarDelegate: SidebarDelegate,
    SidebarListDelegate: SidebarListDelegate,
  };
});


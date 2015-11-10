// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

  var Sidebar = Polymer({
    is: 'extensions-sidebar',

    properties: {
      inDevMode: {
        type: Boolean,
        value: false,
      },
    },

    behaviors: [
      I18nBehavior,
    ],

    /** @param {extensions.SidebarDelegate} delegate */
    setDelegate: function(delegate) {
      this.delegate_ = delegate;
    },

    onDevModeChange_: function() {
      this.delegate_.setProfileInDevMode(
          this.$['developer-mode-checkbox'].checked);
    },

    onLoadUnpackedTap_: function() {
      this.delegate_.loadUnpacked();
    },

    onPackTap_: function() {
      this.delegate_.packExtension();
    },

    onUpdateNowTap_: function() {
      this.delegate_.updateAllExtensions();
    },
  });

  return {
    Sidebar: Sidebar,
    SidebarDelegate: SidebarDelegate,
  };
});


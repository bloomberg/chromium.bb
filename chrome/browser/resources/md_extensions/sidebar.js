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

  /** @interface */
  var SidebarScrollDelegate = function() {};

  SidebarScrollDelegate.prototype = {
    /** Scrolls to the extensions section. */
    scrollToExtensions: assertNotReached,

    /** Scrolls to the apps section. */
    scrollToApps: assertNotReached,

    /** Scrolls to the websites section. */
    scrollToWebsites: assertNotReached,
  };

  var Sidebar = Polymer({
    is: 'extensions-sidebar',

    properties: {
      inDevMode: {
        type: Boolean,
        value: false,
      },

      hideExtensionsButton: {
        type: Boolean,
        value: false,
      },

      hideAppsButton: {
        type: Boolean,
        value: false,
      },

      hideWebsitesButton: {
        type: Boolean,
        value: false,
      },
    },

    behaviors: [
      I18nBehavior,
    ],

    /** @param {extensions.SidebarDelegate} delegate */
    setDelegate: function(delegate) {
      /** @private {extensions.SidebarDelegate} */
      this.delegate_ = delegate;
    },

    /** @param {extensions.SidebarScrollDelegate} scrollDelegate */
    setScrollDelegate: function(scrollDelegate) {
      /** @private {extensions.SidebarScrollDelegate} */
      this.scrollDelegate_ = scrollDelegate;
    },

    /** @private */
    onExtensionsTap_: function() {
      this.scrollDelegate_.scrollToExtensions();
    },

    /** @private */
    onAppsTap_: function() {
      this.scrollDelegate_.scrollToApps();
    },

    /** @private */
    onWebsitesTap_: function() {
      this.scrollDelegate_.scrollToWebsites();
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
    SidebarScrollDelegate: SidebarScrollDelegate,
  };
});


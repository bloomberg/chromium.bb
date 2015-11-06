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
  };

  var Sidebar = Polymer({
    is: 'extensions-sidebar',

    properties: {
      inDevMode: {
        type: Boolean,
        value: false,
      },
    },

    /** @param {extensions.SidebarDelegate} delegate */
    setDelegate: function(delegate) {
      this.delegate_ = delegate;
    },

    onDevModeChange_: function() {
      this.delegate_.setProfileInDevMode(
          this.$['developer-mode-checkbox'].checked);
    },
  });

  return {
    Sidebar: Sidebar,
    SidebarDelegate: SidebarDelegate,
  };
});


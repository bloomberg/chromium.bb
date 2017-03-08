// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('extensions');

cr.define('extensions', function() {
  /** @interface */
  var ToolbarDelegate = function() {};

  ToolbarDelegate.prototype = {
    /**
     * Toggles whether or not the profile is in developer mode.
     * @param {boolean} inDevMode
     */
    setProfileInDevMode: assertNotReached,

    /** Opens the dialog to load unpacked extensions. */
    loadUnpacked: assertNotReached,

    /** Updates all extensions. */
    updateAllExtensions: assertNotReached,
  };

  var Toolbar = Polymer({
    is: 'extensions-toolbar',

    behaviors: [I18nBehavior],

    properties: {
      inDevMode: {
        type: Boolean,
        value: false,
      },
    },

    /** @param {extensions.ToolbarDelegate} delegate */
    setDelegate: function(delegate) {
      /** @private {extensions.ToolbarDelegate} */
      this.delegate_ = delegate;
    },

    /** @private */
    onDevModeChange_: function() {
      this.delegate_.setProfileInDevMode(this.$['dev-mode'].checked);
    },

    /** @private */
    onLoadUnpackedTap_: function() {
      this.delegate_.loadUnpacked();
    },

    /** @private */
    onPackTap_: function() {
      this.fire('pack-tap');
    },

    /** @private */
    onUpdateNowTap_: function() {
      this.delegate_.updateAllExtensions();
    },
  });

  return {
    Toolbar: Toolbar,
    ToolbarDelegate: ToolbarDelegate,
  };
});

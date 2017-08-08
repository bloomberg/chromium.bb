// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('extensions');

cr.define('extensions', function() {
  /** @interface */
  const ToolbarDelegate = function() {};

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

  const Toolbar = Polymer({
    is: 'extensions-toolbar',

    behaviors: [I18nBehavior],

    properties: {
      /** @type {extensions.ToolbarDelegate} */
      delegate: Object,

      inDevMode: {
        type: Boolean,
        value: false,
      },
    },

    /** @private */
    onDevModeChange_: function() {
      this.delegate.setProfileInDevMode(this.$['dev-mode'].checked);
    },

    /** @private */
    onLoadUnpackedTap_: function() {
      this.delegate.loadUnpacked();
    },

    /** @private */
    onPackTap_: function() {
      this.fire('pack-tap');
    },

    /** @private */
    onUpdateNowTap_: function() {
      this.delegate.updateAllExtensions();
    },
  });

  return {
    Toolbar: Toolbar,
    ToolbarDelegate: ToolbarDelegate,
  };
});

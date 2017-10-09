// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('extensions');

cr.define('extensions', function() {
  /** @interface */
  class ToolbarDelegate {
    /**
     * Toggles whether or not the profile is in developer mode.
     * @param {boolean} inDevMode
     */
    setProfileInDevMode(inDevMode) {}

    /** Opens the dialog to load unpacked extensions. */
    loadUnpacked() {}

    /** Updates all extensions. */
    updateAllExtensions() {}
  }

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

      // <if expr="chromeos">
      kioskEnabled: Boolean,
      // </if>
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

    // <if expr="chromeos">
    /** @private */
    onKioskTap_: function() {
      this.fire('kiosk-tap');
    },
    // </if>

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

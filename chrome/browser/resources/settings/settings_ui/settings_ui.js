// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-ui' implements the UI for the Settings page.
 *
 * Example:
 *
 *    <settings-ui prefs="{{prefs}}"></settings-ui>
 */
Polymer({
  is: 'settings-ui',

  properties: {
    /**
     * Preferences state.
     * @type {?CrSettingsPrefsElement}
     */
    prefs: Object,

    /** @type {?settings.DirectionDelegate} */
    directionDelegate: {
      observer: 'directionDelegateChanged_',
      type: Object,
    },
  },

  listeners: {
    'iron-select': 'onIronSelect_',
    'paper-responsive-change': 'onPaperResponsiveChange_',
  },

  /**
   * @param {!CustomEvent} e
   * @private
   */
  onIronSelect_: function(e) {
    if (Polymer.dom(e).path.indexOf(this.$.panel) >= 0)
      this.classList.remove('narrowing');
  },

  /**
   * @param {!CustomEvent} e
   * @private
   */
  onPaperResponsiveChange_: function(e) {
    if (Polymer.dom(e).rootTarget == this.$.panel)
      this.classList.toggle('narrowing', e.detail.narrow);
  },

  /** @private */
  directionDelegateChanged_: function() {
    this.$.panel.rightDrawer = this.directionDelegate.isRtl();
  },
});

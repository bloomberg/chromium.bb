// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `settings-toggle-button` is a toggle that controls a supplied preference.
 */
Polymer({
  is: 'settings-toggle-button',

  behaviors: [SettingsBooleanControlBehavior],

  properties: {
    elideLabel: {
      type: Boolean,
      reflectToAttribute: true,
    },
  },

  /** @override */
  focus: function() {
    this.$.control.focus();
  },

  /** @private */
  onLabelWrapperTap_: function() {
    if (this.controlDisabled_())
      return;

    this.checked = !this.checked;
    this.notifyChangedByUserInteraction();
  },

  /**
   * TODO(scottchen): temporary fix until polymer gesture bug resolved. See:
   * https://github.com/PolymerElements/paper-slider/issues/186
   * @private
   */
  resetTrackLock_: function() {
    Polymer.Gestures.gestures.tap.reset();
  },
});

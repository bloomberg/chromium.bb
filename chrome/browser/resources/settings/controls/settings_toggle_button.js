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

    /**
     * Which element will trigger state changes. Defaults to |this|.
     * @type {HTMLElement}
     */
    actionTarget: {
      observer: 'onActionTargetChange_',
      type: Object,
      value: function() {
        return this;
      },
    },
  },

  observers: [
    'onDisableOrPrefChange_(disabled, pref.*, actionTarget)',
  ],

  /** @override */
  focus: function() {
    this.$.control.focus();
  },

  /** @private */
  onActionTargetChange_: function(current, prior) {
    if (prior) {
      this.unlisten(prior, 'tap', 'onLabelWrapperTap_');
    }
    if (current) {
      this.listen(current, 'tap', 'onLabelWrapperTap_');
    }
  },

  /**
   * Handle taps directly on the toggle (see: onLabelWrapperTap_ for non-toggle
   * taps).
   * @param {!Event} e
   * @private
   */
  onToggleTap_: function(e) {
    // Stop the event from propagating to avoid firing two 'changed' events.
    e.stopPropagation();
  },

  /** @private */
  onDisableOrPrefChange_: function() {
    if (this.controlDisabled_()) {
      this.actionTarget.removeAttribute('actionable');
    } else {
      this.actionTarget.setAttribute('actionable', '');
    }
  },

  /**
   * Handle non-toggle button taps (see: onToggleTap_ for toggle taps).
   * @param {!Event} e
   * @private
   */
  onLabelWrapperTap_: function(e) {
    // Stop the event from propagating to avoid firing two 'changed' events.
    e.stopPropagation();
    if (this.controlDisabled_())
      return;

    this.checked = !this.checked;
    this.notifyChangedByUserInteraction();
    this.fire('change');
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

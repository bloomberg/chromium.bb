// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'controlled-radio-button',

  behaviors: [
    PrefControlBehavior,
    Polymer.IronA11yKeysBehavior,
  ],

  properties: {
    checked: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
      observer: 'checkedChanged_',
    },

    disabled: {
      type: Boolean,
      computed: 'computeDisabled_(pref.*)',
      reflectToAttribute: true,
      observer: 'disabledChanged_',
    },

    label: {
      type: String,
      value: '',  // Allows the hidden$= binding to run without being set.
    },

    name: {
      type: String,
      notify: true,
    },

    /** @private */
    pressed_: Boolean,
  },

  hostAttributes: {
    'aria-disabled': 'false',
    'aria-checked': 'false',
    role: 'radio',
    tabindex: 0,
  },

  listeners: {
    'blur': 'updatePressed_',
    'down': 'updatePressed_',
    'focus': 'updatePressed_',
    'up': 'updatePressed_',
  },

  keyBindings: {
    // This is mainly for screenreaders, which can perform actions on things
    // that aren't focused (only focused things get synthetic click/tap events).
    'enter:keyup': 'click',
    'space:keyup': 'click',
  },

  /** @private */
  checkedChanged_: function() {
    this.setAttribute('aria-checked', this.checked ? 'true' : 'false');
  },

  /**
   * @return {boolean} Whether the button is disabled.
   * @private
   */
  computeDisabled_: function() {
    return this.pref.enforcement == chrome.settingsPrivate.Enforcement.ENFORCED;
  },

  /**
   * @param {boolean} current
   * @param {boolean} previous
   * @private
   */
  disabledChanged_: function(current, previous) {
    if (previous === undefined && !this.disabled)
      return;

    this.setAttribute('tabindex', this.disabled ? -1 : 0);
    this.setAttribute('aria-disabled', this.disabled ? 'true' : 'false');
  },

  /**
   * @return {boolean}
   * @private
   */
  showIndicator_: function() {
    return this.disabled &&
        this.name == Settings.PrefUtil.prefToString(assert(this.pref));
  },

  /**
   * @param {!Event} e
   * @private
   */
  onIndicatorTap_: function(e) {
    // Disallow <controlled-radio-button on-click="..."> when disabled.
    e.preventDefault();
    e.stopPropagation();
  },

  /**
   * @param {!Event} e
   * @private
   */
  updatePressed_: function(e) {
    if (this.disabled)
      return;

    this.pressed_ = ['down', 'focus'].includes(e.type);
  },
});

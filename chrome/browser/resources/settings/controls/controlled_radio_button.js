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

    label: {
      type: String,
      value: '',  // Allows the hidden$= binding to run without being set.
    },

    name: {
      type: String,
      notify: true,
    },

    /** @private */
    controlled_: {
      type: Boolean,
      computed: 'computeControlled_(pref.*)',
      reflectToAttribute: true,
    },

    /** @private */
    pressed_: Boolean,
  },

  hostAttributes: {
    role: 'radio',
    tabindex: 0,
  },

  listeners: {
    'blur': 'updatePressed_',
    'down': 'updatePressed_',
    'focus': 'updatePressed_',
    'tap': 'onTap_',
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
  computeControlled_: function() {
    return this.pref.enforcement == chrome.settingsPrivate.Enforcement.ENFORCED;
  },

  /**
   * @param {boolean} controlled
   * @param {string} name
   * @return {boolean}
   * @private
   */
  showIndicator_: function(controlled, name) {
    return controlled &&
        name == Settings.PrefUtil.prefToString(assert(this.pref));
  },

  /**
   * @param {!Event} e
   * @private
   */
  onIndicatorTap_: function(e) {
    // Disallow <controlled-radio-button on-tap="..."> when controlled.
    e.preventDefault();
    e.stopPropagation();
  },

  /** @private */
  onTap_: function() {
    if (!this.controlled_)
      this.checked = true;
  },

  /**
   * @param {!Event} e
   * @private
   */
  updatePressed_: function(e) {
    this.pressed_ = ['down', 'focus'].includes(e.type);
  },
});

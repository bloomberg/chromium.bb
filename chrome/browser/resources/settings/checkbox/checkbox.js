// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `cr-settings-checkbox` is a checkbox that controls a supplied preference.
 *
 * Example:
 *      <cr-settings-checkbox pref="{{prefs.settings.enableFoo}}"
 *          label="Enable foo setting." subLabel="(bar also)">
 *      </cr-settings-checkbox>
 *
 * @element cr-settings-checkbox
 */
Polymer({
  is: 'cr-settings-checkbox',

  properties: {
    /**
     * The boolean preference object to control.
     * @type {?chrome.settingsPrivate.PrefObject}
     */
    pref: Object,

    label: {
      type: String,
      value: '',
    },

    subLabel: {
      type: String,
      value: '',
    },
  },

  /** @override */
  ready: function() {
    this.$.events.forward(this.$.checkbox, ['change']);
  },
});

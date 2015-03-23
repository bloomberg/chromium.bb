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
  publish: {
    /**
     * The boolean preference to control.
     *
     * @attribute pref
     * @type {Object}
     * @default null
     */
    pref: null,

    /**
     * @attribute label
     * @type {string}
     * @default ''
     */
    label: '',

    /**
     * @attribute label
     * @type {string}
     * @default ''
     */
    subLabel: '',
  },

  ready: function() {
    this.$.events.forward(this.$.checkbox, ['change']);
  },
});

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-ui' implements the UI for the Settings page.
 *
 * Example:
 *
 *    <cr-settings-ui prefs="{{prefs}}"></cr-settings-ui>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-ui
 */
Polymer({
  is: 'cr-settings-ui',

  properties: {
    /**
     * Preferences state.
     * @type {?CrSettingsPrefsElement}
     */
    prefs: Object,

    /**
     * Ordered list of settings pages available to the user. Do not edit
     * this variable directly.
     * @type {!Array<!HTMLElement>}
     */
    pages: {
      type: Array,
      value: function() { return []; },
      notify: true,
    },
  },
});

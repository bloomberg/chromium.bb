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
Polymer('cr-settings-ui', {
  publish: {
    /**
     * Preferences state.
     *
     * @attribute prefs
     * @type CrSettingsPrefsElement
     * @default null
     */
    prefs: null,

    /**
     * Ordered list of settings pages available to the user. Do not edit
     * this variable directly.
     *
     * @attribute pages
     * @type Array<!Object>
     * @default null
     */
    pages: null,
  },

  /** @override */
  created: function() {
    this.pages = [];
  },
});

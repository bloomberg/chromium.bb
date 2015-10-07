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
 *
 * @group Chrome Settings Elements
 * @element settings-ui
 */
Polymer({
  is: 'settings-ui',

  properties: {
    /**
     * Preferences state.
     * @type {?CrSettingsPrefsElement}
     */
    prefs: Object,
  },
});

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings' is the main MD-Settings element, combining the UI and models.
 *
 * Example:
 *
 *    <cr-settings></cr-settings>
 *
 * @group Chrome Settings Elements
 * @element cr-settings
 */
Polymer({
  is: 'cr-settings',

  properties {
    /**
     * The CrSettingsPrefsElement used throughout the app.
     * @private {!CrSettingsPrefsElement}
     */
    prefs_: {
      type: Object,
      value: function() { return this.$.prefs; },
      notify: true,
    }
  },
});

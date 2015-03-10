// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-downloads-page' is the settings page containing downloads
 * settings.
 *
 * Example:
 *
 *    <core-animated-pages>
 *      <cr-settings-downloads-page></cr-settings-downloads-page>
 *      ... other pages ...
 *    </core-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-downloads-page
 */
Polymer('cr-settings-downloads-page', {
  publish: {
    /**
     * Preferences state.
     *
     * @attribute prefs
     * @type CrSettingsPrefsElement
     * @default null
     */
    prefs: null,
  },

  selectDownloadLocation: function() {
    // TODO(orenb): Communicate with the C++ to actually display a folder
    // picker.
    this.$.downloadsPath.value = '/Downloads';
  },
});

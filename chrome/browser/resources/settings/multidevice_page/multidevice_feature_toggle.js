// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview
 * Toggle button with the SettingsBooleanControlBehavior of the
 * settings-toggle-button element. This is needed because the
 * settings-toggle-button is styled to take up an entire row but the row is not
 * well suited to the MultiDevice Setting UI use case (see the description of CL
 * 1149001 for more details).
 */
Polymer({
  is: 'settings-multidevice-feature-toggle',

  behaviors: [SettingsBooleanControlBehavior],

  /**
   * @param {!CustomEvent} event
   * @private
   */
  onChange_: function(event) {
    this.checked = event.detail;
    // TODO(jordynass): Check for auth token if necessary for pref change. This
    // requires both JavaScript and C++ changes, most likely using a
    // EasyUnlockBrowserProxy (in people_page/easy_unlock_browser_proxy.js) as
    // in people_page/lock_screen.
    this.notifyChangedByUserInteraction();
    this.resetToPrefValue();
  },
});

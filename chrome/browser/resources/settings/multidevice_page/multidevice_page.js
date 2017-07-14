// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings page for managing multidevice features.
 */

Polymer({
  is: 'settings-multidevice-page',

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Reflects the current state of the toggle buttons (in this page and the
     * subpage). This will be set when the user changes the toggle.
     * @private
     */
    smsConnectToggleState_: {
      type: Boolean,
      observer: 'smsConnectToggleStateChanged_',
    },
  },

  /** @private */
  smsConnectToggleStateChanged_: function() {
    // TODO(orenb): Switch from paper-toggle-button to settings-toggle-button,
    // which will manage the underlying pref state, once the new pref has been
    // implemented. Propagate here the pref value to the SMS connect component.
  },
});

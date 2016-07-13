// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Contains utilities for verifying the user has entered an account password.
 */

/** @polymerBehavior */
var QuickUnlockPasswordDetectBehavior = [QuickUnlockRoutingBehavior, {
  properties: {
    setModes: Object
  },

  /**
   * Verifies that there is an account password available for the
   * chrome.quickUnlockPrivate.setModes call. If there is no password, this will
   * redirect to the authenticate screen.
   */
  askForPasswordIfUnset: function() {
    if (!this.setModes) {
      this.currentRoute = {
        page: 'basic',
        section: 'people',
        subpage: [QuickUnlockScreen.AUTHENTICATE]
      };
    }
  }
}];

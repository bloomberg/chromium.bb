// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Demo mode setup screen implementation.
 */

login.createScreen('DemoSetupScreen', 'demo-setup', function() {
  return {
    EXTERNAL_API: ['onSetupFinished'],

    get defaultControl() {
      return $('demo-setup-content');
    },

    /** @override */
    onBeforeShow: function(data) {
      $('demo-setup-content').reset();
    },

    /**
     * Called when demo mode setup finished.
     * @param {boolean} isSuccess Whether demo setup finished successfully.
     * @param {string} message Error message to be displayed to the user,
     *  populated if setup finished with an error.
     */
    onSetupFinished: function(isSuccess, message) {
      $('demo-setup-content').onSetupFinished(isSuccess, message);
    },
  };
});

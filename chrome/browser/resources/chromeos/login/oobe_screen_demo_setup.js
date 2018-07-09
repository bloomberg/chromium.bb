// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Demo mode setup screen implementation.
 */

login.createScreen('DemoSetupScreen', 'demo-setup', function() {
  return {
    EXTERNAL_API: ['onSetupFinished'],

    /** @override */
    decorate: function() {
      var demoSetupScreen = $('demo-setup-content');
      demoSetupScreen.offlineDemoModeEnabled_ =
          loadTimeData.getValue('offlineDemoModeEnabled');
    },

    /** Returns a control which should receive an initial focus. */
    get defaultControl() {
      return $('demo-setup-content');
    },

    /** Called after resources are updated. */
    updateLocalizedContent: function() {
      $('demo-setup-content').updateLocalizedContent();
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

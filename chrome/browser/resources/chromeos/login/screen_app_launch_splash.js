// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview App install/launch splash screen implementation.
 */

login.createScreen('AppLaunchSplashScreen', 'app-launch-splash', function() {
  return {
    EXTERNAL_API: [
      'toggleNetworkConfig',
      'updateApp',
      'updateMessage',
    ],

    /**
     * Event handler that is invoked just before the frame is shown.
     * @param {string} data Screen init payload.
     */
    onBeforeShow: function(data) {
      this.updateApp(data['appInfo']);

      $('splash-shortcut-info').hidden = !data['shortcutEnabled'];

      Oobe.getInstance().headerHidden = true;
    },

    /**
     * Event handler that is invoked just before the frame is hidden.
     */
    onBeforeHide: function() {
    },

    /**
     * Toggles visibility of the network configuration option.
     * @param {boolean} visible Whether to show the option.
     */
    toggleNetworkConfig: function(visible) {
      // TODO(tengs): Implement network configuration in app launch.
    },

    /**
     * Updates the app name and icon.
     * @param {Object} app Details of app being launched.
     */
    updateApp: function(app) {
      $('splash-header').textContent = app.name;
      $('splash-header').style.backgroundImage = 'url(' + app.iconURL + ')';
    },

    /**
     * Updates the message for the current launch state.
     * @param {string} message Description for current launch state.
     */
    updateMessage: function(message) {
      $('splash-launch-text').textContent = message;
    }
  };
});

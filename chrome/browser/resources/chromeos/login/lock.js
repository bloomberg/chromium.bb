// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Login UI based on a stripped down OOBE controller.
 */

<include src="login_shared.js">

// Lazy load polymer.
(function() {
  'use strict';

  // Register loader for custom elements.
  cr.ui.login.ResourceLoader.registerAssets({
    id: 'custom-elements',
    html: [{ url: 'chrome://oobe/custom_elements.html' }]
  });

  // Called after polymer has been loaded. Fades the pin element in.
  var onPolymerLoaded = function() {
    var pinContainer = $('pin-container');
    pinContainer.style.opacity = 1;
  };

  // We only load the PIN element when it is actually shown so that lock screen
  // load times remain low when the user is not using a PIN.
  //
  // Loading the PIN element blocks the DOM, which will interrupt any running
  // animations. We load the PIN after an idle notification to allow the pod
  // fly-in animation to complete without interruption.
  if (loadTimeData.getBoolean('showPin')) {
    cr.ui.login.ResourceLoader.loadAssetsOnIdle('custom-elements',
                                                onPolymerLoaded);
  }
})();

cr.define('cr.ui.Oobe', function() {
  return {
    /**
     * Initializes the OOBE flow.  This will cause all C++ handlers to
     * be invoked to do final setup.
     */
    initialize: function() {
      // TODO(jdufault): Remove this after resolving crbug.com/452599.
      console.log('Start initializing LOCK OOBE');

      cr.ui.login.DisplayManager.initialize();
      login.AccountPickerScreen.register();

      cr.ui.Bubble.decorate($('bubble'));
      login.HeaderBar.decorate($('login-header-bar'));

      chrome.send('screenStateInitialize');
    },

    // Dummy Oobe functions not present with stripped login UI.
    initializeA11yMenu: function(e) {},
    handleAccessibilityLinkClick: function(e) {},
    handleSpokenFeedbackClick: function(e) {},
    handleHighContrastClick: function(e) {},
    handleScreenMagnifierClick: function(e) {},
    setUsageStats: function(checked) {},
    setOemEulaUrl: function(oemEulaUrl) {},
    setTpmPassword: function(password) {},
    refreshA11yInfo: function(data) {},

    /**
     * Reloads content of the page.
     * @param {!Object} data New dictionary with i18n values.
     */
    reloadContent: function(data) {
      loadTimeData.overrideValues(data);
      i18nTemplate.process(document, loadTimeData);
      Oobe.getInstance().updateLocalizedContent_();
    },
  };
});

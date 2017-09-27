// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  /**
   * Encapsulated handling of ChromeOS kiosk apps options page.
   * @constructor
   */
  function KioskAppsOverlay() {
  }

  cr.addSingletonGetter(KioskAppsOverlay);

  KioskAppsOverlay.prototype = {
    /**
     * Clear error timer id.
     * @type {?number}
     */
    clearErrorTimer_: null,

    /** @private {WebUIListenerTracker} */
    listenerTracker_: null,

    /**
     * Initialize the page.
     */
    initialize: function() {
      this.listenerTracker_ = new WebUIListenerTracker();

      cr.sendWithPromise('initializeKioskAppSettings')
          .then(KioskAppsOverlay.enableKiosk);

      extensions.KioskAppList.decorate($('kiosk-app-list'));

      var overlay = $('kiosk-apps-page');
      cr.ui.overlay.setupOverlay(overlay);
      cr.ui.overlay.globalInitialization();
      overlay.addEventListener('cancelOverlay', this.handleDismiss_.bind(this));

      $('kiosk-options-overlay-confirm').onclick =
          this.handleDismiss_.bind(this);
      $('kiosk-app-id-edit').addEventListener('keypress',
          this.handleAppIdInputKeyPressed_.bind(this));
      $('kiosk-app-add').onclick = this.handleAddButtonClick_.bind(this);
    },

    /*
     * Invoked when the page is shown.
     */
    didShowPage: function() {
      this.listenerTracker_.add(
          'kiosk-app-settings-changed', KioskAppsOverlay.setSettings);
      this.listenerTracker_.add(
          'kiosk-app-updated', KioskAppsOverlay.updateApp);
      this.listenerTracker_.add('kiosk-app-error', KioskAppsOverlay.showError);

      cr.sendWithPromise('getKioskAppSettings')
          .then(KioskAppsOverlay.setSettings);
      $('kiosk-app-id-edit').focus();
    },

    /**
     * Shows error for given app name/id and schedules it to cleared.
     * @param {!string} appName App name/id to show in error banner.
     */
    showError: function(appName) {
      var errorBanner = $('kiosk-apps-error-banner');
      var appNameElement = errorBanner.querySelector('.kiosk-app-name');
      appNameElement.textContent = appName;
      errorBanner.classList.add('visible');

      if (this.clearErrorTimer_)
        window.clearTimeout(this.clearErrorTimer_);

      // Sets a timer to clear out error banner after 5 seconds.
      this.clearErrorTimer_ = window.setTimeout(function() {
        errorBanner.classList.remove('visible');
        this.clearErrorTimer_ = null;
      }.bind(this), 5000);
    },

    /**
     * Handles keypressed event in the app id input element.
     * @private
     */
    handleAppIdInputKeyPressed_: function(e) {
      if (e.key == 'Enter' && e.target.value)
        this.handleAddButtonClick_();
    },

    /**
     * Handles click event on the add button.
     * @private
     */
    handleAddButtonClick_: function() {
      var appId = $('kiosk-app-id-edit').value;
      if (!appId)
        return;

      chrome.send('addKioskApp', [appId]);
      $('kiosk-app-id-edit').value = '';
    },

    /**
     * Handles the overlay being dismissed.
     * @private
     */
    handleDismiss_: function() {
      this.handleAddButtonClick_();
      extensions.ExtensionSettings.showOverlay(null);
      this.listenerTracker_.removeAll();
    }
  };

  /**
   * Sets apps to be displayed in kiosk-app-list.
   * @param {!Object<{apps: !Array<AppDict>, disableBailout: boolean,
   *     hasAutoLaunchApp: boolean}>} settings An object containing an array of
   *     app info objects and disable bailout shortcut flag.
   */
  KioskAppsOverlay.setSettings = function(settings) {
    $('kiosk-app-list').setApps(settings.apps);
    $('kiosk-disable-bailout-shortcut').checked = settings.disableBailout;
    $('kiosk-disable-bailout-shortcut').disabled = !settings.hasAutoLaunchApp;
  };

  /**
   * Update an app in kiosk-app-list.
   * @param {!Object} app App info to be updated.
   */
  KioskAppsOverlay.updateApp = function(app) {
    $('kiosk-app-list').updateApp(app);
  };

  /**
   * Shows error for given app name/id.
   * @param {!string} appName App name/id to show in error banner.
   */
  KioskAppsOverlay.showError = function(appName) {
    KioskAppsOverlay.getInstance().showError(appName);
  };

  /**
   * Enables consumer kiosk.
   * @param {!{kioskEnabled: boolean, autoLaunchEnabled: boolean}} params
   */
  KioskAppsOverlay.enableKiosk = function(params) {
    $('add-kiosk-app').hidden = !params.kioskEnabled;
    $('kiosk-disable-bailout-shortcut').parentNode.hidden =
        !params.autoLaunchEnabled;
    $('kiosk-app-list').setAutoLaunchEnabled(params.autoLaunchEnabled);
  };

  // Export
  return {
    KioskAppsOverlay: KioskAppsOverlay
  };
});

// <include src="kiosk_app_list.js">
// <include src="kiosk_app_disable_bailout_confirm.js">

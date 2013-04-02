// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;

  /**
   * Encapsulated handling of ChromeOS kiosk apps options page.
   * @extends {options.OptionsPage}
   * @constructor
   */
  function KioskAppsOverlay() {
    OptionsPage.call(this,
                     'kioskAppsOverlay',
                     loadTimeData.getString('kioskOverlayTitle'),
                     'kiosk-apps-page');
  }

  cr.addSingletonGetter(KioskAppsOverlay);

  KioskAppsOverlay.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * Clear error timer id.
     * @type {?number}
     */
    clearErrorTimer_: null,

    /** @override */
    initializePage: function() {
      // Call base class implementation to starts preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      options.KioskAppList.decorate($('kiosk-app-list'));

      $('kiosk-options-overlay-confirm').onclick =
          OptionsPage.closeOverlay.bind(OptionsPage);
      $('kiosk-app-id-edit').addEventListener('keypress',
          this.handleAppIdInputKeyPressed_);

      Preferences.getInstance().addEventListener('cros.kiosk.apps',
          this.loadAppsIfVisible_.bind(this));
      Preferences.getInstance().addEventListener('cros.kiosk.auto_launch',
          this.loadAppsIfVisible_.bind(this));
    },

    /** @override */
    didShowPage: function() {
      this.loadAppsIfVisible_();
      $('kiosk-app-id-edit').focus();
    },

    /**
     * Loads the apps if the overlay page is visible.
     * @private
     */
    loadAppsIfVisible_: function() {
      if (this.visible)
        chrome.send('getKioskApps');
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
      if (e.keyIdentifier == 'Enter' && e.target.value) {
        chrome.send('addKioskApp', [e.target.value]);
        e.target.value = '';
      }
    }
  };

  /**
   * Sets apps to be displayed in kiosk-app-list.
   * @param {!Array.<!Object>} apps An array of app info objects.
   */
  KioskAppsOverlay.setApps = function(apps) {
    $('kiosk-app-list').setApps(apps);
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

  // Export
  return {
    KioskAppsOverlay: KioskAppsOverlay
  };
});

<include src="kiosk_app_list.js"></include>
<include src="kiosk_app_disable_bailout_confirm.js"></include>

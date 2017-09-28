// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  const KioskDialog = Polymer({
    is: 'extensions-kiosk-dialog',
    behaviors: [WebUIListenerBehavior],
    properties: {
      /** @private {?string} */
      addAppInput_: {
        type: String,
        value: null,
      },

      /** @private {!Array<!KioskApp>} */
      apps_: Array,

      /** @private */
      bailoutDisabled_: Boolean,

      /** @private */
      canEditAutoLaunch_: Boolean,

      /** @private */
      canEditBailout_: Boolean,

      /** @private {?string} */
      errorAppId_: String,
    },

    /** @private {?extensions.KioskBrowserProxy} */
    kioskBrowserProxy_: null,

    /** @override */
    ready: function() {
      this.kioskBrowserProxy_ = extensions.KioskBrowserProxyImpl.getInstance();
    },

    /** @override */
    attached: function() {
      this.kioskBrowserProxy_.initializeKioskAppSettings()
          .then(params => {
            this.canEditAutoLaunch_ = params.autoLaunchEnabled;
            return this.kioskBrowserProxy_.getKioskAppSettings();
          })
          .then(this.setSettings_.bind(this));

      this.addWebUIListener(
          'kiosk-app-settings-changed', this.setSettings_.bind(this));
      this.addWebUIListener('kiosk-app-updated', this.updateApp_.bind(this));
      this.addWebUIListener('kiosk-app-error', this.showError_.bind(this));

      this.$.dialog.showModal();
    },

    /**
     * @param {!KioskAppSettings} settings
     * @private
     */
    setSettings_: function(settings) {
      this.apps_ = settings.apps;
      this.bailoutDisabled_ = settings.disableBailout;
      this.canEditBailout_ = settings.hasAutoLaunchApp;
    },

    /**
     * @param {!KioskApp} app
     * @private
     */
    updateApp_: function(app) {
      const index = this.apps_.findIndex(a => a.id == app.id);
      assert(index < this.apps_.length);
      this.set('apps_.' + index, app);
    },

    /**
     * @param {string} appId
     * @private
     */
    showError_: function(appId) {
      this.errorAppId_ = appId;
    },

    /**
     * @param {string} errorMessage
     * @return {string}
     * @private
     */
    getErrorMessage_: function(errorMessage) {
      return this.errorAppId_ + ' ' + errorMessage;
    },

    /** @private */
    onAddAppTap_: function() {
      assert(this.addAppInput_);
      this.kioskBrowserProxy_.addKioskApp(this.addAppInput_);
      this.addAppInput_ = null;
    },

    /** @private */
    clearInputInvalid_: function() {
      this.errorAppId_ = null;
    },

    /**
     * @param {{model: {item: !KioskApp}}} event
     * @private
     */
    onAutoLaunchButtonTap_: function(event) {
      const app = event.model.item;
      if (app.autoLaunch) {  // If the app is originally set to
                             // auto-launch.
        this.kioskBrowserProxy_.disableKioskAutoLaunch(app.id);
      } else {
        this.kioskBrowserProxy_.enableKioskAutoLaunch(app.id);
      }
    },

    /**
     * @param {!Event} event
     * @private
     */
    onBailoutTap_: function(event) {
      event.preventDefault();
      if (this.bailoutDisabled_) {
        this.kioskBrowserProxy_.setDisableBailoutShortcut(false);
        this.bailoutDisabled_ = false;
        this.$['confirm-dialog'].close();
      } else {
        this.$['confirm-dialog'].showModal();
      }
    },

    /** @private */
    onBailoutDialogCancelTap_: function() {
      this.$['confirm-dialog'].cancel();
    },

    /** @private */
    onBailoutDialogConfirmTap_: function() {
      this.kioskBrowserProxy_.setDisableBailoutShortcut(true);
      this.bailoutDisabled_ = true;
      this.$['confirm-dialog'].close();
    },

    /** @private */
    onDoneTap_: function() {
      this.$.dialog.close();
    },

    /**
     * @param {{model: {item: !KioskApp}}} event
     * @private
     */
    onDeleteAppTap_: function(event) {
      this.kioskBrowserProxy_.removeKioskApp(event.model.item.id);
    },

    /**
     * @param {boolean} autoLaunched
     * @param {string} disableStr
     * @param {string} enableStr
     * @return {string}
     * @private
     */
    getAutoLaunchButtonLabel_: function(autoLaunched, disableStr, enableStr) {
      return autoLaunched ? disableStr : enableStr;
    },

    /**
     * @param {!Event} e
     * @private
     */
    stopPropagation_: function(e) {
      e.stopPropagation();
    },
  });

  return {
    KioskDialog: KioskDialog,
  };
});

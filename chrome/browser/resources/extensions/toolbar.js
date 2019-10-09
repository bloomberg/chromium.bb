// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  /** @interface */
  class ToolbarDelegate {
    /**
     * Toggles whether or not the profile is in developer mode.
     * @param {boolean} inDevMode
     */
    setProfileInDevMode(inDevMode) {}

    /**
     * Opens the dialog to load unpacked extensions.
     * @return {!Promise}
     */
    loadUnpacked() {}

    /**
     * Updates all extensions.
     * @return {!Promise}
     */
    updateAllExtensions() {}
  }

  const Toolbar = Polymer({
    is: 'extensions-toolbar',

    properties: {
      /** @type {extensions.ToolbarDelegate} */
      delegate: Object,

      inDevMode: {
        type: Boolean,
        value: false,
        observer: 'onInDevModeChanged_',
        reflectToAttribute: true,
      },

      devModeControlledByPolicy: Boolean,

      isSupervised: Boolean,

      // <if expr="chromeos">
      kioskEnabled: Boolean,
      // </if>

      canLoadUnpacked: Boolean,

      /** @private */
      expanded_: Boolean,

      /** @private */
      showPackDialog_: Boolean,

      /**
       * Prevents initiating update while update is in progress.
       * @private
       */
      isUpdating_: {type: Boolean, value: false}
    },

    behaviors: [I18nBehavior],

    hostAttributes: {
      role: 'banner',
    },

    /**
     * @return {boolean}
     * @private
     */
    shouldDisableDevMode_: function() {
      return this.devModeControlledByPolicy || this.isSupervised;
    },

    /**
     * @return {string}
     * @private
     */
    getTooltipText_: function() {
      return this.i18n(
          this.isSupervised ? 'controlledSettingChildRestriction' :
                              'controlledSettingPolicy');
    },

    /**
     * @return {string}
     * @private
     */
    getIcon_: function() {
      return this.isSupervised ? 'cr20:kite' : 'cr20:domain';
    },

    /**
     * @param {!CustomEvent<boolean>} e
     * @private
     */
    onDevModeToggleChange_: function(e) {
      this.delegate.setProfileInDevMode(e.detail);
      chrome.metricsPrivate.recordUserAction(
          'Options_ToggleDeveloperMode_' + (e.detail ? 'Enabled' : 'Disabled'));
    },

    /**
     * @param {boolean} current
     * @param {boolean} previous
     * @private
     */
    onInDevModeChanged_: function(current, previous) {
      const drawer = this.$.devDrawer;
      if (this.inDevMode) {
        if (drawer.hidden) {
          drawer.hidden = false;
          // Requesting the offsetTop will cause a reflow (to account for
          // hidden).
          /** @suppress {suspiciousCode} */ drawer.offsetTop;
        }
      } else {
        if (previous == undefined) {
          drawer.hidden = true;
          return;
        }

        listenOnce(drawer, 'transitionend', e => {
          if (!this.inDevMode) {
            drawer.hidden = true;
          }
        });
      }
      this.expanded_ = !this.expanded_;
    },

    /** @private */
    onLoadUnpackedTap_: function() {
      this.delegate.loadUnpacked().catch(loadError => {
        this.fire('load-error', loadError);
      });
      chrome.metricsPrivate.recordUserAction('Options_LoadUnpackedExtension');
    },

    /** @private */
    onPackTap_: function() {
      chrome.metricsPrivate.recordUserAction('Options_PackExtension');
      this.showPackDialog_ = true;
    },

    /** @private */
    onPackDialogClose_: function() {
      this.showPackDialog_ = false;
      this.$.packExtensions.focus();
    },

    // <if expr="chromeos">
    /** @private */
    onKioskTap_: function() {
      this.fire('kiosk-tap');
    },
    // </if>

    /** @private */
    onUpdateNowTap_: function() {
      // If already updating, do not initiate another update.
      if (this.isUpdating_) {
        return;
      }

      this.isUpdating_ = true;

      const toastManager = cr.toastManager.getInstance();
      // Keep the toast open indefinitely.
      toastManager.duration = 0;
      toastManager.show(this.i18n('toolbarUpdatingToast'), false);
      this.delegate.updateAllExtensions().then(
          () => {
            toastManager.hide();
            toastManager.duration = 3000;
            toastManager.show(this.i18n('toolbarUpdateDone'), false);
            this.isUpdating_ = false;
          },
          () => {
            toastManager.hide();
            this.isUpdating_ = false;
          });
    },
  });

  return {
    Toolbar: Toolbar,
    ToolbarDelegate: ToolbarDelegate,
  };
});

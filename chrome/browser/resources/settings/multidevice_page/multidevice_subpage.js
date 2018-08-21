// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Subpage of settings-multidevice-page for managing multidevice features
 * individually and for forgetting a host.
 */
cr.exportPath('settings');

Polymer({
  is: 'settings-multidevice-subpage',

  behaviors: [
    MultiDeviceFeatureBehavior,
    CrNetworkListenerBehavior,
    PrefsBehavior,
  ],

  properties: {
    /** @type {?SettingsRoutes} */
    routes: {
      type: Object,
      value: settings.routes,
    },

    /** Overridden from NetworkListenerBehavior. */
    networkingPrivate: {
      type: Object,
      value: chrome.networkingPrivate,
    },

    /** Overridden from NetworkListenerBehavior. */
    networkListChangeSubscriberSelectors_: {
      type: Array,
      value: () => ['settings-multidevice-tether-item'],
    },

    /** Overridden from NetworkListenerBehavior. */
    networksChangeSubscriberSelectors_: {
      type: Array,
      value: () => ['settings-multidevice-tether-item'],
    },

    // TODO(jordynass): Set this variable once the information can be retrieved
    // by whatever implementation we use (possibly an IPC or from prefs).
    /**
     * If SMS Connect requires setup, it displays a paper button prompting the
     * setup flow. If it is already set up, it displays a regular toggle for the
     * feature.
     * @private {boolean}
     */
    androidMessagesRequiresSetup_: {
      type: Boolean,
      value: true,
    },
  },

  /** @private */
  handleAndroidMessagesButtonClick_: function() {
    this.androidMessagesRequiresSetup_ = false;
    this.setPrefValue('multidevice.sms_connect_enabled', true);
  },

  listeners: {
    'show-networks': 'onShowNetworks_',
  },

  onShowNetworks_: function() {
    settings.navigateTo(
        settings.routes.INTERNET_NETWORKS,
        new URLSearchParams('type=' + CrOnc.Type.TETHER));
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowIndividualFeatures_: function() {
    return this.pageContentData.mode ===
        settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED;
  },

  /** @private */
  handleForgetDeviceClick_: function() {
    // TODO(jordynass): Have this navigate to the route for dialog once it is
    // built.
  },

  /**
   * @return {string}
   * @private
   */
  getStatusText_: function() {
    return this.getPref('multidevice_setup.suite_enabled').value ?
        this.i18n('multideviceEnabled') :
        this.i18n('multideviceDisabled');
  },
});

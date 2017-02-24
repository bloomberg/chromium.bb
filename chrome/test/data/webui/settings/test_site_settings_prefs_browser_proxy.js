// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * An example empty pref.
 * @type {SiteSettingsPref}
 */
var prefsEmpty = {
  defaults: {
    auto_downloads: '',
    background_sync: '',
    camera: '',
    cookies: '',
    geolocation: '',
    javascript: '',
    mic: '',
    midiDevices: '',
    notifications: '',
    plugins: '',
    popups: '',
    unsandboxed_plugins: '',
  },
  exceptions: {
    auto_downloads: [],
    background_sync: [],
    camera: [],
    cookies: [],
    geolocation: [],
    javascript: [],
    mic: [],
    midiDevices: [],
    notifications: [],
    plugins: [],
    popups: [],
    unsandboxed_plugins: [],
  },
};

/**
 * A test version of SiteSettingsPrefsBrowserProxy. Provides helper methods
 * for allowing tests to know when a method was called, as well as
 * specifying mock responses.
 *
 * @constructor
 * @implements {settings.SiteSettingsPrefsBrowserProxy}
 * @extends {settings.TestBrowserProxy}
 */
var TestSiteSettingsPrefsBrowserProxy = function() {
  settings.TestBrowserProxy.call(this, [
    'fetchUsbDevices',
    'fetchZoomLevels',
    'getCookieDetails',
    'getDefaultValueForContentType',
    'getExceptionList',
    'isPatternValid',
    'observeProtocolHandlers',
    'observeProtocolHandlersEnabledState',
    'reloadCookies',
    'removeCookie',
    'removeProtocolHandler',
    'removeUsbDevice',
    'removeZoomLevel',
    'resetCategoryPermissionForOrigin',
    'setCategoryPermissionForOrigin',
    'setDefaultValueForContentType',
    'setProtocolDefault'
  ]);

  /** @private {!SiteSettingsPref} */
  this.prefs_ = prefsEmpty;

  /** @private {!Array<ZoomLevelEntry>} */
  this.zoomList_ = [];

  /** @private {!Array<!UsbDeviceEntry>} */
  this.usbDevices_ = [];

  /** @private {!Array<!ProtocolEntry>} */
  this.protocolHandlers_ = [];

  /** @private {?CookieList} */
  this.cookieDetails_ = null;

  /** @private {boolean} */
  this.isPatternValid_ = true;
};

TestSiteSettingsPrefsBrowserProxy.prototype = {
  __proto__: settings.TestBrowserProxy.prototype,

  /**
   * Sets the prefs to use when testing.
   * @param {!SiteSettingsPref} prefs The prefs to set.
   */
  setPrefs: function(prefs) {
    this.prefs_ = prefs;

    // Notify all listeners that their data may be out of date.
    for (var type in settings.ContentSettingsTypes) {
      cr.webUIListenerCallback(
          'contentSettingSitePermissionChanged',
          settings.ContentSettingsTypes[type],
          '');
    }
  },

  /**
   * Sets the prefs to use when testing.
   * @param {!Array<ZoomLevelEntry>} list The zoom list to set.
   */
  setZoomList: function(list) {
    this.zoomList_ = list;
  },

  /**
   * Sets the prefs to use when testing.
   * @param {!Array<UsbDeviceEntry>} list The usb device entry list to set.
   */
  setUsbDevices: function(list) {
    // Shallow copy of the passed-in array so mutation won't impact the source
    this.usbDevices_ = list.slice();
  },

  /**
   * Sets the prefs to use when testing.
   * @param {!Array<ProtocolEntry>} list The protocol handlers list to set.
   */
  setProtocolHandlers: function(list) {
    // Shallow copy of the passed-in array so mutation won't impact the source
    this.protocolHandlers_ = list.slice();
  },

  /** @override */
  setDefaultValueForContentType: function(contentType, defaultValue) {
    this.methodCalled(
        'setDefaultValueForContentType', [contentType, defaultValue]);
  },

  /** @override */
  getCookieDetails: function(site) {
    this.methodCalled('getCookieDetails', site);
    return Promise.resolve(this.cookieDetails_  || {id: '', children: []});
  },

  /**
   * @param {!CookieList} cookieList
   */
  setCookieDetails: function(cookieList) {
    this.cookieDetails_ = cookieList;
  },

  /** @override */
  getDefaultValueForContentType: function(contentType) {
    this.methodCalled('getDefaultValueForContentType', contentType);

    var pref = undefined;
    if (contentType == settings.ContentSettingsTypes.AUTOMATIC_DOWNLOADS) {
      pref = this.prefs_.defaults.auto_downloads;
    } else if (contentType == settings.ContentSettingsTypes.BACKGROUND_SYNC) {
      pref = this.prefs_.background_sync;
    } else if (contentType == settings.ContentSettingsTypes.CAMERA) {
      pref = this.prefs_.defaults.camera;
    } else if (contentType == settings.ContentSettingsTypes.COOKIES) {
      pref = this.prefs_.defaults.cookies;
    } else if (contentType == settings.ContentSettingsTypes.GEOLOCATION) {
      pref = this.prefs_.defaults.geolocation;
    } else if (contentType == settings.ContentSettingsTypes.IMAGES) {
      pref = this.prefs_.defaults.images;
    } else if (contentType == settings.ContentSettingsTypes.JAVASCRIPT) {
      pref = this.prefs_.defaults.javascript;
    } else if (contentType == settings.ContentSettingsTypes.MIC) {
      pref = this.prefs_.defaults.mic;
    } else if (contentType == settings.ContentSettingsTypes.MIDI_DEVICES) {
      pref = this.prefs_.defaults.midiDevices;
    } else if (contentType == settings.ContentSettingsTypes.NOTIFICATIONS) {
      pref = this.prefs_.defaults.notifications;
    } else if (contentType == settings.ContentSettingsTypes.PDF_DOCUMENTS) {
      pref = this.prefs_.defaults.pdf_documents;
    } else if (contentType == settings.ContentSettingsTypes.POPUPS) {
      pref = this.prefs_.defaults.popups;
    } else if (contentType == settings.ContentSettingsTypes.PLUGINS) {
      pref = this.prefs_.defaults.plugins;
    } else if (
        contentType == settings.ContentSettingsTypes.UNSANDBOXED_PLUGINS) {
      pref = this.prefs_.defaults.unsandboxed_plugins;
    } else {
      console.log('getDefault received unknown category: ' + contentType);
    }

    assert(pref != undefined, 'Pref is missing for ' + contentType);
    return Promise.resolve(pref);
  },

  /** @override */
  getExceptionList: function(contentType) {
    this.methodCalled('getExceptionList', contentType);

    var pref = undefined;
    if (contentType == settings.ContentSettingsTypes.AUTOMATIC_DOWNLOADS)
      pref = this.prefs_.exceptions.auto_downloads;
    else if (contentType == settings.ContentSettingsTypes.BACKGROUND_SYNC)
      pref = this.prefs_.exceptions.background_sync;
    else if (contentType == settings.ContentSettingsTypes.CAMERA)
      pref = this.prefs_.exceptions.camera;
    else if (contentType == settings.ContentSettingsTypes.COOKIES)
      pref = this.prefs_.exceptions.cookies;
    else if (contentType == settings.ContentSettingsTypes.GEOLOCATION)
      pref = this.prefs_.exceptions.geolocation;
    else if (contentType == settings.ContentSettingsTypes.IMAGES)
      pref = this.prefs_.exceptions.images;
    else if (contentType == settings.ContentSettingsTypes.JAVASCRIPT)
      pref = this.prefs_.exceptions.javascript;
    else if (contentType == settings.ContentSettingsTypes.MIC)
      pref = this.prefs_.exceptions.mic;
    else if (contentType == settings.ContentSettingsTypes.MIDI_DEVICES)
      pref = this.prefs_.exceptions.midiDevices;
    else if (contentType == settings.ContentSettingsTypes.NOTIFICATIONS)
      pref = this.prefs_.exceptions.notifications;
    else if (contentType == settings.ContentSettingsTypes.PDF_DOCUMENTS)
      pref = this.prefs_.exceptions.pdf_documents;
    else if (contentType == settings.ContentSettingsTypes.PLUGINS)
      pref = this.prefs_.exceptions.plugins;
    else if (contentType == settings.ContentSettingsTypes.PROTECTED_CONTENT)
      pref = this.prefs_.exceptions.protectedContent;
    else if (contentType == settings.ContentSettingsTypes.POPUPS)
      pref = this.prefs_.exceptions.popups;
    else if (contentType == settings.ContentSettingsTypes.UNSANDBOXED_PLUGINS)
      pref = this.prefs_.exceptions.unsandboxed_plugins;
    else
      console.log('getExceptionList received unknown category: ' + contentType);

    assert(pref != undefined, 'Pref is missing for ' + contentType);
    return Promise.resolve(pref);
  },

  /** @override */
  isPatternValid: function(pattern) {
    this.methodCalled('isPatternValid', pattern);
    return Promise.resolve(this.isPatternValid_);
  },

  /**
   * Specify whether isPatternValid should succeed or fail.
   */
  setIsPatternValid: function(isValid) {
    this.isPatternValid_ = isValid;
  },

  /** @override */
  resetCategoryPermissionForOrigin: function(
      primaryPattern, secondaryPattern, contentType, incognito) {
    this.methodCalled('resetCategoryPermissionForOrigin',
        [primaryPattern, secondaryPattern, contentType, incognito]);
    return Promise.resolve();
  },

  /** @override */
  setCategoryPermissionForOrigin: function(
      primaryPattern, secondaryPattern, contentType, value, incognito) {
    this.methodCalled('setCategoryPermissionForOrigin',
        [primaryPattern, secondaryPattern, contentType, value, incognito]);
    return Promise.resolve();
  },

  /** @override */
  fetchZoomLevels: function() {
    cr.webUIListenerCallback('onZoomLevelsChanged', this.zoomList_);
    this.methodCalled('fetchZoomLevels');
  },

  /** @override */
  reloadCookies: function() {
    return Promise.resolve({id: null, children: []});
  },

  /** @override */
  removeCookie: function(path) {
    this.methodCalled('removeCookie', path);
  },

  /** @override */
  removeZoomLevel: function(host) {
    this.methodCalled('removeZoomLevel', [host]);
  },

  /** @override */
  fetchUsbDevices: function() {
    this.methodCalled('fetchUsbDevices');
    return Promise.resolve(this.usbDevices_);
  },

  /** @override */
  removeUsbDevice: function() {
    this.methodCalled('removeUsbDevice', arguments);
  },

  /** @override */
  observeProtocolHandlers: function() {
    cr.webUIListenerCallback('setHandlersEnabled', true);
    cr.webUIListenerCallback('setProtocolHandlers', this.protocolHandlers_);
    this.methodCalled('observeProtocolHandlers');
  },

  /** @override */
  observeProtocolHandlersEnabledState: function() {
    cr.webUIListenerCallback('setHandlersEnabled', true);
    this.methodCalled('observeProtocolHandlersEnabledState');
  },

  /** @override */
  setProtocolDefault: function() {
    this.methodCalled('setProtocolDefault', arguments);
  },

  /** @override */
  removeProtocolHandler: function() {
    this.methodCalled('removeProtocolHandler', arguments);
  }
};

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * In the real (non-test) code, this data comes from the C++ handler.
 * Only used for tests.
 * @typedef {{defaults: Map<string, !DefaultContentSetting>,
 *            exceptions: !Map<string, !Array<!RawSiteException>>}}
 */
var SiteSettingsPref;

/**
 * An example empty pref.
 * TODO(patricialor): Use the values from settings.ContentSettingsTypes (see
 * site_settings/constants.js) as the keys for these instead.
 * @type {SiteSettingsPref}
 */
var prefsEmpty = {
  defaults: {
    ads: {},
    auto_downloads: {},
    background_sync: {},
    camera: {},
    cookies: {},
    geolocation: {},
    javascript: {},
    mic: {},
    midiDevices: {},
    notifications: {},
    plugins: {},
    images: {},
    popups: {},
    unsandboxed_plugins: {},
  },
  exceptions: {
    ads: [],
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
    images: [],
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
 * @extends {TestBrowserProxy}
 */
var TestSiteSettingsPrefsBrowserProxy = function() {
  TestBrowserProxy.call(this, [
    'fetchUsbDevices',
    'fetchZoomLevels',
    'getCookieDetails',
    'getDefaultValueForContentType',
    'getExceptionList',
    'getOriginPermissions',
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
    'setProtocolDefault',
    'updateIncognitoStatus',
  ]);

  /** @private {boolean} */
  this.hasIncognito_ = false;

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
  __proto__: TestBrowserProxy.prototype,

  /**
   * Pretends an incognito session started or ended.
   * @param {boolean} hasIncognito True for session started.
   */
  setIncognito: function(hasIncognito) {
    this.hasIncognito_ = hasIncognito;
    cr.webUIListenerCallback('onIncognitoStatusChanged', hasIncognito);
  },

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
    if (contentType == settings.ContentSettingsTypes.ADS) {
      pref = this.prefs_.defaults.ads;
    } else if (
        contentType == settings.ContentSettingsTypes.AUTOMATIC_DOWNLOADS) {
      pref = this.prefs_.defaults.auto_downloads;
    } else if (contentType == settings.ContentSettingsTypes.BACKGROUND_SYNC) {
      pref = this.prefs_.defaults.background_sync;
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
    if (contentType == settings.ContentSettingsTypes.ADS)
      pref = this.prefs_.exceptions.ads;
    else if (contentType == settings.ContentSettingsTypes.AUTOMATIC_DOWNLOADS)
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

    if (this.hasIncognito_) {
      var incognitoElements = [];
      for (var i = 0; i < pref.length; ++i)
        incognitoElements.push(Object.assign({incognito: true}, pref[i]));
      pref = pref.concat(incognitoElements);
    }

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
  getOriginPermissions: function(origin, contentTypes) {
    this.methodCalled('getOriginPermissions', [origin, contentTypes]);

    var exceptionList = [];
    contentTypes.forEach(function(contentType) {
      // Convert |contentType| to its corresponding pref name, if different.
      if (contentType == settings.ContentSettingsTypes.GEOLOCATION) {
        contentType = 'geolocation';
      } else if (contentType == settings.ContentSettingsTypes.CAMERA) {
        contentType = 'camera';
      } else if (contentType == settings.ContentSettingsTypes.MIC) {
        contentType = 'mic';
      } else if (contentType == settings.ContentSettingsTypes.BACKGROUND_SYNC) {
        contentType = 'background_sync';
      } else if (
          contentType == settings.ContentSettingsTypes.AUTOMATIC_DOWNLOADS) {
        contentType = 'auto_downloads';
      } else if (
          contentType == settings.ContentSettingsTypes.UNSANDBOXED_PLUGINS) {
        contentType = 'unsandboxed_plugins';
      }

      var setting;
      var source;
      this.prefs_.exceptions[contentType].some(function(originPrefs) {
        if (originPrefs.origin == origin) {
          setting = originPrefs.setting;
          source = originPrefs.source;
          return true;
        }
      });
      assert(
          settings !== undefined,
          'There was no exception set for origin: ' + origin +
              ' and contentType: ' + contentType);

      exceptionList.push({
        embeddingOrigin: '',
        incognito: false,
        origin: origin,
        displayName: '',
        setting: setting,
        source: source,
      })
    }, this);
    return Promise.resolve(exceptionList);
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
  },

  /** @override */
  updateIncognitoStatus: function() {
    this.methodCalled('updateIncognitoStatus', arguments);
  }
};

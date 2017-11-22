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
 * TODO(https://crbug.com/742706): Use the values from
 * settings.ContentSettingsTypes (see site_settings/constants.js) as the keys
 * for these instead.
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
    midi_devices: {},
    notifications: {},
    plugins: {},
    images: {},
    popups: {},
    protectedContent: {},
    sound: {},
    unsandboxed_plugins: {},
    clipboard: {},
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
    midi_devices: [],
    notifications: [],
    plugins: [],
    images: [],
    popups: [],
    protectedContent: [],
    sound: [],
    unsandboxed_plugins: [],
    clipboard: [],
  },
};

/**
 * A test version of SiteSettingsPrefsBrowserProxy. Provides helper methods
 * for allowing tests to know when a method was called, as well as
 * specifying mock responses.
 *
 * @implements {settings.SiteSettingsPrefsBrowserProxy}
 */
class TestSiteSettingsPrefsBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'fetchUsbDevices',
      'fetchZoomLevels',
      'getDefaultValueForContentType',
      'getExceptionList',
      'getOriginPermissions',
      'isOriginValid',
      'isPatternValid',
      'observeProtocolHandlers',
      'observeProtocolHandlersEnabledState',
      'removeProtocolHandler',
      'removeUsbDevice',
      'removeZoomLevel',
      'resetCategoryPermissionForPattern',
      'setCategoryPermissionForPattern',
      'setDefaultValueForContentType',
      'setOriginPermissions',
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

    /** @private {boolean} */
    this.isOriginValid_ = true;

    /** @private {boolean} */
    this.isPatternValid_ = true;
  }

  /**
   * Pretends an incognito session started or ended.
   * @param {boolean} hasIncognito True for session started.
   */
  setIncognito(hasIncognito) {
    this.hasIncognito_ = hasIncognito;
    cr.webUIListenerCallback('onIncognitoStatusChanged', hasIncognito);
  }

  /**
   * Sets the prefs to use when testing.
   * @param {!SiteSettingsPref} prefs The prefs to set.
   */
  setPrefs(prefs) {
    this.prefs_ = prefs;

    // Notify all listeners that their data may be out of date.
    for (var type in this.prefs_.exceptions) {
      let exceptionList = this.prefs_.exceptions[type];
      for (var i = 0; i < exceptionList.length; ++i) {
        cr.webUIListenerCallback(
            'contentSettingSitePermissionChanged', type,
            exceptionList[i].origin, '');
      }
    }
  }

  /**
   * Sets the default prefs only. Use this only when there is a need to
   * distinguish between the callback for permissions changing and the callback
   * for default permissions changing.
   * TODO(https://crbug.com/742706): This function is a hack and should be
   * removed.
   * @param {!Map<string, !DefaultContentSetting>} defaultPrefs The new
   *     default prefs to set.
   */
  setDefaultPrefs(defaultPrefs) {
    this.prefs_.defaults = defaultPrefs;

    // Notify all listeners that their data may be out of date.
    for (var type in settings.ContentSettingsTypes) {
      cr.webUIListenerCallback(
          'contentSettingCategoryChanged', settings.ContentSettingsTypes[type]);
    }
  }

  /**
   * Sets one exception for a given category, replacing any existing exceptions
   * for the same origin. Note this ignores embedding origins.
   * @param {!settings.ContentSettingsTypes} category The category the new
   *     exception belongs to.
   * @param {!RawSiteException} newException The new preference to add/replace.
   */
  setSingleException(category, newException) {
    // Remove entries from the current prefs which have the same origin.
    var newPrefs = /** @type {!Array<RawSiteException>} */
        (this.prefs_.exceptions[category].filter((categoryException) => {
          if (categoryException.origin != newException.origin)
            return true;
        }));
    newPrefs.push(newException);
    this.prefs_.exceptions[category] = newPrefs;

    cr.webUIListenerCallback(
        'contentSettingSitePermissionChanged', category, newException.origin);
  }

  /**
   * Sets the prefs to use when testing.
   * @param {!Array<ZoomLevelEntry>} list The zoom list to set.
   */
  setZoomList(list) {
    this.zoomList_ = list;
  }

  /**
   * Sets the prefs to use when testing.
   * @param {!Array<UsbDeviceEntry>} list The usb device entry list to set.
   */
  setUsbDevices(list) {
    // Shallow copy of the passed-in array so mutation won't impact the source
    this.usbDevices_ = list.slice();
  }

  /**
   * Sets the prefs to use when testing.
   * @param {!Array<ProtocolEntry>} list The protocol handlers list to set.
   */
  setProtocolHandlers(list) {
    // Shallow copy of the passed-in array so mutation won't impact the source
    this.protocolHandlers_ = list.slice();
  }

  /** @override */
  setDefaultValueForContentType(contentType, defaultValue) {
    this.methodCalled(
        'setDefaultValueForContentType', [contentType, defaultValue]);
  }

  /** @override */
  setOriginPermissions(origin, contentTypes, blanketSetting) {
    for (var type in this.prefs_.exceptions) {
      let exceptionList = this.prefs_.exceptions[type];
      for (var i = 0; i < exceptionList.length; ++i) {
        var effectiveSetting = blanketSetting;
        if (blanketSetting == settings.ContentSetting.DEFAULT) {
          effectiveSetting = this.prefs_.defaults[type].setting;
          exceptionList[i].source = settings.SiteSettingSource.DEFAULT;
        }
        exceptionList[i].setting = effectiveSetting;
      }
    }

    this.setPrefs(this.prefs_);
    this.methodCalled(
        'setOriginPermissions', [origin, contentTypes, blanketSetting]);
  }

  /** @override */
  getDefaultValueForContentType(contentType) {
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
      pref = this.prefs_.defaults.midi_devices;
    } else if (contentType == settings.ContentSettingsTypes.NOTIFICATIONS) {
      pref = this.prefs_.defaults.notifications;
    } else if (contentType == settings.ContentSettingsTypes.PDF_DOCUMENTS) {
      pref = this.prefs_.defaults.pdf_documents;
    } else if (contentType == settings.ContentSettingsTypes.POPUPS) {
      pref = this.prefs_.defaults.popups;
    } else if (contentType == settings.ContentSettingsTypes.PLUGINS) {
      pref = this.prefs_.defaults.plugins;
    } else if (contentType == settings.ContentSettingsTypes.SOUND) {
      pref = this.prefs_.defaults.sound;
    } else if (
        contentType == settings.ContentSettingsTypes.UNSANDBOXED_PLUGINS) {
      pref = this.prefs_.defaults.unsandboxed_plugins;
    } else if (contentType == settings.ContentSettingsTypes.PROTECTED_CONTENT) {
      pref = this.prefs_.defaults.protectedContent;
    } else if (contentType == settings.ContentSettingsTypes.CLIPBOARD) {
      pref = this.prefs_.defaults.clipboard;
    } else {
      console.log('getDefault received unknown category: ' + contentType);
    }

    assert(pref != undefined, 'Pref is missing for ' + contentType);
    return Promise.resolve(pref);
  }

  /** @override */
  getExceptionList(contentType) {
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
      pref = this.prefs_.exceptions.midi_devices;
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
    else if (contentType == settings.ContentSettingsTypes.SOUND)
      pref = this.prefs_.exceptions.sound;
    else if (contentType == settings.ContentSettingsTypes.UNSANDBOXED_PLUGINS)
      pref = this.prefs_.exceptions.unsandboxed_plugins;
    else if (contentType == settings.ContentSettingsTypes.CLIPBOARD)
      pref = this.prefs_.exceptions.clipboard;
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
  }

  /** @override */
  isOriginValid(origin) {
    this.methodCalled('isOriginValid', origin);
    return Promise.resolve(this.isOriginValid_);
  }

  /**
   * Specify whether isOriginValid should succeed or fail.
   */
  setIsOriginValid(isValid) {
    this.isOriginValid_ = isValid;
  }

  /** @override */
  isPatternValid(pattern) {
    this.methodCalled('isPatternValid', pattern);
    return Promise.resolve(this.isPatternValid_);
  }

  /**
   * Specify whether isPatternValid should succeed or fail.
   */
  setIsPatternValid(isValid) {
    this.isPatternValid_ = isValid;
  }

  /** @override */
  resetCategoryPermissionForPattern(
      primaryPattern, secondaryPattern, contentType, incognito) {
    this.methodCalled(
        'resetCategoryPermissionForPattern',
        [primaryPattern, secondaryPattern, contentType, incognito]);
    return Promise.resolve();
  }

  /** @override */
  getOriginPermissions(origin, contentTypes) {
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
      } else if (contentType == settings.ContentSettingsTypes.MIDI_DEVICES) {
        contentType = 'midi_devices';
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
      this.prefs_.exceptions[contentType].some((originPrefs) => {
        if (originPrefs.origin == origin) {
          setting = originPrefs.setting;
          source = originPrefs.source;
          return true;
        }
      });
      assert(
          setting != undefined,
          'There was no exception set for origin: ' + origin +
              ' and contentType: ' + contentType);

      exceptionList.push({
        embeddingOrigin: '',
        incognito: false,
        origin: origin,
        displayName: '',
        setting: setting,
        source: source,
      });
    }, this);
    return Promise.resolve(exceptionList);
  }

  /** @override */
  setCategoryPermissionForPattern(
      primaryPattern, secondaryPattern, contentType, value, incognito) {
    this.methodCalled(
        'setCategoryPermissionForPattern',
        [primaryPattern, secondaryPattern, contentType, value, incognito]);
    return Promise.resolve();
  }

  /** @override */
  fetchZoomLevels() {
    cr.webUIListenerCallback('onZoomLevelsChanged', this.zoomList_);
    this.methodCalled('fetchZoomLevels');
  }

  /** @override */
  removeZoomLevel(host) {
    this.methodCalled('removeZoomLevel', [host]);
  }

  /** @override */
  fetchUsbDevices() {
    this.methodCalled('fetchUsbDevices');
    return Promise.resolve(this.usbDevices_);
  }

  /** @override */
  removeUsbDevice() {
    this.methodCalled('removeUsbDevice', arguments);
  }

  /** @override */
  observeProtocolHandlers() {
    cr.webUIListenerCallback('setHandlersEnabled', true);
    cr.webUIListenerCallback('setProtocolHandlers', this.protocolHandlers_);
    this.methodCalled('observeProtocolHandlers');
  }

  /** @override */
  observeProtocolHandlersEnabledState() {
    cr.webUIListenerCallback('setHandlersEnabled', true);
    this.methodCalled('observeProtocolHandlersEnabledState');
  }

  /** @override */
  setProtocolDefault() {
    this.methodCalled('setProtocolDefault', arguments);
  }

  /** @override */
  removeProtocolHandler() {
    this.methodCalled('removeProtocolHandler', arguments);
  }

  /** @override */
  updateIncognitoStatus() {
    this.methodCalled('updateIncognitoStatus', arguments);
  }
}

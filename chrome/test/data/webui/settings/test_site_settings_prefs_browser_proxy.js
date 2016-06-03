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
    fullscreen: '',
    geolocation: '',
    javascript: '',
    keygen: '',
    mic: '',
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
    fullscreen: [],
    geolocation: [],
    javascript: [],
    keygen: [],
    mic: [],
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
    'getDefaultValueForContentType',
    'getExceptionList',
    'resetCategoryPermissionForOrigin',
    'setCategoryPermissionForOrigin',
    'setDefaultValueForContentType',
  ]);

  /** @private {!SiteSettingsPref} */
  this.prefs_ = prefsEmpty;
};

// TODO(finnur): Modify the tests so that most of the code this class implements
//     can be ripped out.
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

  /** @override */
  setDefaultValueForContentType: function(contentType, defaultValue) {
    this.methodCalled(
        'setDefaultValueForContentType', [contentType, defaultValue]);
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
    } else if (contentType == settings.ContentSettingsTypes.FULLSCREEN) {
      pref = this.prefs_.defaults.fullscreen;
    } else if (contentType == settings.ContentSettingsTypes.GEOLOCATION) {
      pref = this.prefs_.defaults.geolocation;
    } else if (contentType == settings.ContentSettingsTypes.IMAGES) {
      pref = this.prefs_.defaults.images;
    } else if (contentType == settings.ContentSettingsTypes.JAVASCRIPT) {
      pref = this.prefs_.defaults.javascript;
    } else if (contentType == settings.ContentSettingsTypes.KEYGEN) {
      pref = this.prefs_.defaults.keygen;
    } else if (contentType == settings.ContentSettingsTypes.MIC) {
      pref = this.prefs_.defaults.mic;
    } else if (contentType == settings.ContentSettingsTypes.NOTIFICATIONS) {
      pref = this.prefs_.defaults.notifications;
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
    return Promise.resolve(pref != 'block');
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
    else if (contentType == settings.ContentSettingsTypes.FULLSCREEN)
      pref = this.prefs_.exceptions.fullscreen;
    else if (contentType == settings.ContentSettingsTypes.GEOLOCATION)
      pref = this.prefs_.exceptions.geolocation;
    else if (contentType == settings.ContentSettingsTypes.IMAGES)
      pref = this.prefs_.exceptions.images;
    else if (contentType == settings.ContentSettingsTypes.JAVASCRIPT)
      pref = this.prefs_.exceptions.javascript;
    else if (contentType == settings.ContentSettingsTypes.KEYGEN)
      pref = this.prefs_.exceptions.keygen;
    else if (contentType == settings.ContentSettingsTypes.MIC)
      pref = this.prefs_.exceptions.mic;
    else if (contentType == settings.ContentSettingsTypes.NOTIFICATIONS)
      pref = this.prefs_.exceptions.notifications;
    else if (contentType == settings.ContentSettingsTypes.PLUGINS)
      pref = this.prefs_.exceptions.plugins;
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
  resetCategoryPermissionForOrigin: function(
      primaryPattern, secondaryPattern, contentType) {
    this.methodCalled('resetCategoryPermissionForOrigin',
        [primaryPattern, secondaryPattern, contentType]);
    return Promise.resolve();
  },

  /** @override */
  setCategoryPermissionForOrigin: function(
      primaryPattern, secondaryPattern, contentType, value) {
    this.methodCalled('setCategoryPermissionForOrigin',
        [primaryPattern, secondaryPattern, contentType, value]);
    return Promise.resolve();
  },
};

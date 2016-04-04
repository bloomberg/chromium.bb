// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * An example empty pref.
 * @type {SiteSettingsPref}
 */
var prefsEmpty = {
  defaults: {
    media_stream_camera: '',
    cookies: '',
    fullscreen: '',
    geolocation: '',
    javascript: '',
    media_stream_mic: '',
    notifications: '',
    popups: '',
  },
  exceptions: {
    media_stream_camera: [],
    cookies: [],
    fullscreen: [],
    geolocation: [],
    javascript: [],
    media_stream_mic: [],
    notifications: [],
    popups: [],
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

    if (contentType == settings.ContentSettingsTypes.CAMERA) {
      return Promise.resolve(
          this.prefs_.defaults.media_stream_camera != 'block');
    } else if (contentType == settings.ContentSettingsTypes.COOKIES) {
      return Promise.resolve(this.prefs_.defaults.cookies != 'block');
    } else if (contentType == settings.ContentSettingsTypes.FULLSCREEN) {
      return Promise.resolve(this.prefs_.defaults.fullscreen != 'block');
    } else if (contentType == settings.ContentSettingsTypes.GEOLOCATION) {
      return Promise.resolve(this.prefs_.defaults.geolocation != 'block');
    } else if (contentType == settings.ContentSettingsTypes.IMAGES) {
      return Promise.resolve(this.prefs_.defaults.images != 'block');
    } else if (contentType == settings.ContentSettingsTypes.JAVASCRIPT) {
      return Promise.resolve(this.prefs_.defaults.javascript != 'block');
    } else if (contentType == settings.ContentSettingsTypes.MIC) {
      return Promise.resolve(this.prefs_.defaults.media_stream_mic != 'block');
    } else if (contentType == settings.ContentSettingsTypes.NOTIFICATIONS) {
      return Promise.resolve(this.prefs_.defaults.notifications != 'block');
    } else if (contentType == settings.ContentSettingsTypes.POPUPS) {
      return Promise.resolve(this.prefs_.defaults.popups != 'block');
    } else {
      console.log('getDefault received unknown category: ' + contentType);
    }

    return Promise.resolve([]);
  },

  /** @override */
  getExceptionList: function(contentType) {
    this.methodCalled('getExceptionList', contentType);

    if (contentType == settings.ContentSettingsTypes.CAMERA)
      return Promise.resolve(this.prefs_.exceptions.media_stream_camera);
    else if (contentType == settings.ContentSettingsTypes.COOKIES)
      return Promise.resolve(this.prefs_.exceptions.cookies);
    else if (contentType == settings.ContentSettingsTypes.FULLSCREEN)
      return Promise.resolve(this.prefs_.exceptions.fullscreen);
    else if (contentType == settings.ContentSettingsTypes.GEOLOCATION)
      return Promise.resolve(this.prefs_.exceptions.geolocation);
    else if (contentType == settings.ContentSettingsTypes.IMAGES)
      return Promise.resolve(this.prefs_.exceptions.images);
    else if (contentType == settings.ContentSettingsTypes.JAVASCRIPT)
      return Promise.resolve(this.prefs_.exceptions.javascript);
    else if (contentType == settings.ContentSettingsTypes.MIC)
      return Promise.resolve(this.prefs_.exceptions.media_stream_mic);
    else if (contentType == settings.ContentSettingsTypes.NOTIFICATIONS)
      return Promise.resolve(this.prefs_.exceptions.notifications);
    else if (contentType == settings.ContentSettingsTypes.POPUPS)
      return Promise.resolve(this.prefs_.exceptions.popups);
    else
      console.log('getExceptionList received unknown category: ' + contentType);

    return Promise.resolve([]);
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

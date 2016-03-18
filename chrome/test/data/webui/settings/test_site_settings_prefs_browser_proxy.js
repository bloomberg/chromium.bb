// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * An example empty pref.
 * @type {SiteSettingsPref}
 */
var prefsEmpty = {
  defaults: {
    location: '',
    notifications: '',
  },
  exceptions: {
    location: [],
    notifications: [],
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
      'setDefaultValueForContentType',
    ]);

    /** @private {!SiteSettingsPref} */
    this.prefs_ = prefsEmpty;
  };

  TestSiteSettingsPrefsBrowserProxy.prototype = {
    __proto__: settings.TestBrowserProxy.prototype,

    /**
     * Sets the prefs to use when testing.
     * @param {!SiteSettingsPref} prefs The prefs to set.
     */
    setPrefs: function(prefs) {
      this.prefs_ = prefs;
    },

    /** @override */
    setDefaultValueForContentType: function(contentType, defaultValue) {
      if (contentType == settings.ContentSettingsTypes.GEOLOCATION)
        this.prefs_.defaults.location = defaultValue == 'allow';
      else if (contentType == settings.ContentSettingsTypes.NOTIFICATIONS)
        this.prefs_.defaults.notifications = defaultValue == 'allow';
      this.methodCalled(
          'setDefaultValueForContentType', contentType + '|' + defaultValue);
    },

    /** @override */
    getDefaultValueForContentType: function(contentType) {
      this.methodCalled('getDefaultValueForContentType', contentType);
      if (contentType == settings.ContentSettingsTypes.GEOLOCATION)
        return Promise.resolve(this.prefs_.defaults.location != 'block');
      else if (contentType == settings.ContentSettingsTypes.NOTIFICATIONS)
        return Promise.resolve(this.prefs_.defaults.notifications != 'block');

      return Promise.resolve([]);
    },

    /** @override */
    getExceptionList: function(contentType) {
      this.methodCalled('getExceptionList', contentType);
      if (contentType == settings.ContentSettingsTypes.GEOLOCATION)
        return Promise.resolve(this.prefs_.exceptions.location);
      else if (contentType == settings.ContentSettingsTypes.NOTIFICATIONS)
        return Promise.resolve(this.prefs_.exceptions.notifications);

      return Promise.resolve([]);
    },

};

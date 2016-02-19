// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "Site Settings" section to
 * interact with the content settings prefs.
 */

cr.define('settings', function() {
  /** @constructor */
  function SiteSettingsPrefsBrowserProxy() {};

  // The singleton instance_ is replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(SiteSettingsPrefsBrowserProxy);

  SiteSettingsPrefsBrowserProxy.prototype = {
    /**
     * Sets the default value for a site settings category.
     * @param {number} contentType The category to change.
     * @param {number} defaultValue The value to set as default.
     */
    setDefaultValueForContentType: function(contentType, defaultValue) {
      chrome.send('setDefaultValueForContentType', [contentType, defaultValue]);
    },

    /**
     * Gets the default value for a site settings category.
     * @param {number} contentType The category to change.
     * @return {Promise}
     */
    getDefaultValueForContentType: function(contentType) {
      return cr.sendWithPromise('getDefaultValueForContentType', contentType);
    },
  };

  return {
    SiteSettingsPrefsBrowserProxy: SiteSettingsPrefsBrowserProxy,
  };
});

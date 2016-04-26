// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "Site Settings" section to
 * interact with the content settings prefs.
 */

/**
 * @typedef {{embeddingOrigin: string,
 *            origin: string,
 *            originForDisplay: string,
 *            setting: string,
 *            source: string}}
 */
var SiteException;

/**
 * @typedef {{location: string,
 *            notifications: string}}
 */
var CategoryDefaultsPref;

/**
 * @typedef {{location: Array<SiteException>,
 *            notifications: Array<SiteException>}}
 */
var ExceptionListPref;

/**
 * @typedef {{defaults: CategoryDefaultsPref,
 *            exceptions: ExceptionListPref}}
 */
var SiteSettingsPref;

cr.define('settings', function() {
  /** @interface */
  function SiteSettingsPrefsBrowserProxy() {}

  SiteSettingsPrefsBrowserProxy.prototype = {
    /**
     * Sets the default value for a site settings category.
     * @param {number} contentType The category to change.
     * @param {number} defaultValue The value to set as default.
     */
    setDefaultValueForContentType: function(contentType, defaultValue) {},

    /**
     * Gets the default value for a site settings category.
     * @param {number} contentType The category to change.
     * @return {Promise<boolean>}
     */
    getDefaultValueForContentType: function(contentType) {},

    /**
     * Gets the exceptions (site list) for a particular category.
     * @param {number} contentType The category to change.
     * @return {Promise<Array<SiteException>>}
     */
    getExceptionList: function(contentType) {},

    /**
     * Resets the category permission for a given origin (expressed as primary
     *    and secondary patterns).
     * @param {string} primaryPattern The origin to change (primary pattern).
     * @param {string} secondaryPattern The embedding origin to change
     *    (secondary pattern).
     * @param {number} contentType The category to change.
     */
    resetCategoryPermissionForOrigin: function(
        primaryPattern, secondaryPattern, contentType) {},

    /**
     * Sets the category permission for a given origin (expressed as primary
     *    and secondary patterns).
     * @param {string} primaryPattern The origin to change (primary pattern).
     * @param {string} secondaryPattern The embedding origin to change
     *    (secondary pattern).
     * @param {number} contentType The category to change.
     * @param {string} value The value to change the permission to.
     */
    setCategoryPermissionForOrigin: function(
        primaryPattern, secondaryPattern, contentType, value) {},

    /**
     * Checks whether a pattern is valid.
     * @param {string} pattern The pattern to check
     * @return {!Promise<boolean>} True if the pattern is valid.
     */
    isPatternValid: function(pattern) {},
  };

  /**
   * @constructor
   * @implements {SiteSettingsPrefsBrowserProxy}
   */
  function SiteSettingsPrefsBrowserProxyImpl() {}

  // The singleton instance_ is replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(SiteSettingsPrefsBrowserProxyImpl);

  SiteSettingsPrefsBrowserProxyImpl.prototype = {
    /** @override */
    setDefaultValueForContentType: function(contentType, defaultValue) {
      chrome.send('setDefaultValueForContentType', [contentType, defaultValue]);
    },

    /** @override */
    getDefaultValueForContentType: function(contentType) {
      return cr.sendWithPromise('getDefaultValueForContentType', contentType);
    },

    /** @override */
    getExceptionList: function(contentType) {
      return cr.sendWithPromise('getExceptionList', contentType);
    },

    /** @override */
    resetCategoryPermissionForOrigin: function(
        primaryPattern, secondaryPattern, contentType) {
      chrome.send('resetCategoryPermissionForOrigin',
          [primaryPattern, secondaryPattern, contentType]);
    },

    /** @override */
    setCategoryPermissionForOrigin: function(
        primaryPattern, secondaryPattern, contentType, value) {
      chrome.send('setCategoryPermissionForOrigin',
          [primaryPattern, secondaryPattern, contentType, value]);
    },

    /** @override */
    isPatternValid: function(pattern) {
      return cr.sendWithPromise('isPatternValid', pattern);
    },

  };

  return {
    SiteSettingsPrefsBrowserProxyImpl: SiteSettingsPrefsBrowserProxyImpl,
  };
});

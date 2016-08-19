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

/**
 * @typedef {{name: string,
 *            id: string}}
 */
var MediaPickerEntry;

/**
 * @typedef {{protocol: string,
 *            spec: string}}
 */
 var ProtocolHandlerEntry;

/**
 * @typedef {{name: string,
 *            product-id: Number,
 *            serial-number: string,
 *            vendor-id: Number}}
 */
var UsbDeviceDetails;

/**
 * @typedef {{embeddingOrigin: string,
 *            object: UsbDeviceDetails,
 *            objectName: string,
 *            origin: string,
 *            setting: string,
 *            source: string}}
 */
var UsbDeviceEntry;

cr.define('settings', function() {
  /** @interface */
  function SiteSettingsPrefsBrowserProxy() {}

  SiteSettingsPrefsBrowserProxy.prototype = {
    /**
     * Sets the default value for a site settings category.
     * @param {string} contentType The name of the category to change.
     * @param {string} defaultValue The name of the value to set as default.
     */
    setDefaultValueForContentType: function(contentType, defaultValue) {},

    /**
     * Gets the default value for a site settings category.
     * @param {string} contentType The name of the category to query.
     * @return {Promise<boolean>}
     */
    getDefaultValueForContentType: function(contentType) {},

    /**
     * Gets the exceptions (site list) for a particular category.
     * @param {string} contentType The name of the category to query.
     * @return {Promise<Array<SiteException>>}
     */
    getExceptionList: function(contentType) {},

    /**
     * Resets the category permission for a given origin (expressed as primary
     *    and secondary patterns).
     * @param {string} primaryPattern The origin to change (primary pattern).
     * @param {string} secondaryPattern The embedding origin to change
     *    (secondary pattern).
     * @param {string} contentType The name of the category to reset.
     */
    resetCategoryPermissionForOrigin: function(
        primaryPattern, secondaryPattern, contentType) {},

    /**
     * Sets the category permission for a given origin (expressed as primary
     *    and secondary patterns).
     * @param {string} primaryPattern The origin to change (primary pattern).
     * @param {string} secondaryPattern The embedding origin to change
     *    (secondary pattern).
     * @param {string} contentType The name of the category to change.
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

    /**
     * Gets the list of default capture devices for a given type of media. List
     * is returned through a JS call to updateDevicesMenu.
     * @param {string} type The type to look up.
     */
    getDefaultCaptureDevices: function(type) {},

    /**
     * Sets a default devices for a given type of media.
     * @param {string} type The type of media to configure.
     * @param {string} defaultValue The id of the media device to set.
     */
    setDefaultCaptureDevice: function(type, defaultValue) {},

    /**
     * Reloads all cookies.
     * @return {!Promise<Array<CookieDataSummaryItem>>} Returns the full cookie
     *     list.
     */
    reloadCookies: function() {},

    /**
     * Fetches all children of a given cookie.
     * @param {string} path The path to the parent cookie.
     * @return {!Promise<Array<CookieDataSummaryItem>>} Returns a cookie list
     *     for the given path.
     */
    loadCookieChildren: function(path) {},

    /**
     * Removes a given cookie.
     * @param {string} path The path to the parent cookie.
     */
    removeCookie: function(path) {},

    /**
     * Removes all cookies.
     * @return {!Promise<Array<CookieDataSummaryItem>>} Returns the up to date
     *     cookie list once deletion is complete (empty list).
     */
    removeAllCookies: function() {},

    /**
     * Initializes the protocol handler list. List is returned through JS calls
     * to setHandlersEnabled, setProtocolHandlers & setIgnoredProtocolHandlers.
     */
    initializeProtocolHandlerList: function() {},

    /**
     * Enables or disables the ability for sites to ask to become the default
     * protocol handlers.
     * @param {boolean} enabled Whether sites can ask to become default.
     */
    setProtocolHandlerDefault: function(enabled) {},

    /**
     * Sets a certain url as default for a given protocol handler.
     * @param {string} protocol The protocol to set a default for.
     * @param {string} url The url to use as the default.
     */
    setProtocolDefault: function(protocol, url) {},

    /**
     * Deletes a certain protocol handler by url.
     * @param {string} protocol The protocol to delete the url from.
     * @param {string} url The url to delete.
     */
    removeProtocolHandler: function(protocol, url) {},

    /**
     * Fetches a list of all USB devices and the sites permitted to use them.
     * @return {!Promise<Array<UsbDeviceEntry>>} The list of USB devices.
     */
    fetchUsbDevices: function() {},

    /**
     * Removes a particular USB device object permission by origin and embedding
     * origin.
     * @param {string} origin The origin to look up the permission for.
     * @param {string} embeddingOrigin the embedding origin to look up.
     * @param {UsbDeviceDetails} usbDevice The USB device to revoke permission
     *     for.
     */
    removeUsbDevice: function(origin, embeddingOrigin, usbDevice) {},
  };

  /**
   * @constructor
   * @implements {settings.SiteSettingsPrefsBrowserProxy}
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

    /** @override */
    getDefaultCaptureDevices: function(type) {
      chrome.send('getDefaultCaptureDevices', [type]);
    },

    /** @override */
    setDefaultCaptureDevice: function(type, defaultValue) {
      chrome.send('setDefaultCaptureDevice', [type, defaultValue]);
    },

    /** @override */
    reloadCookies: function() {
      return cr.sendWithPromise('reloadCookies');
    },

    /** @override */
    loadCookieChildren: function(path) {
      return cr.sendWithPromise('loadCookie', path);
    },

    /** @override */
    removeCookie: function(path) {
      chrome.send('removeCookie', [path]);
    },

    /** @override */
    removeAllCookies: function() {
      return cr.sendWithPromise('removeAllCookies');
    },


    initializeProtocolHandlerList: function() {
      chrome.send('initializeProtocolHandlerList');
    },

    /** @override */
    setProtocolHandlerDefault: function(enabled) {
      chrome.send('setHandlersEnabled', [enabled]);
    },

    /** @override */
    setProtocolDefault: function(protocol, url) {
      chrome.send('setDefault', [[protocol, url]]);
    },

    /** @override */
    removeProtocolHandler: function(protocol, url) {
      chrome.send('removeHandler', [[protocol, url]]);
    },

    /** @override */
    fetchUsbDevices: function() {
      return cr.sendWithPromise('fetchUsbDevices');
    },

    /** @override */
    removeUsbDevice: function(origin, embeddingOrigin, usbDevice) {
      chrome.send('removeUsbDevice', [origin, embeddingOrigin, usbDevice]);
    },
  };

  return {
    SiteSettingsPrefsBrowserProxy: SiteSettingsPrefsBrowserProxy,
    SiteSettingsPrefsBrowserProxyImpl: SiteSettingsPrefsBrowserProxyImpl,
  };
});

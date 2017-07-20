// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "Site Settings" section to
 * interact with the content settings prefs.
 */

/**
 * The handler will send a policy source that is similar, but not exactly the
 * same as a ControlledBy value. If the ContentSettingProvider is omitted it
 * should be treated as 'default'.
 * @enum {string}
 */
var ContentSettingProvider = {
  EXTENSION: 'extension',
  PREFERENCE: 'preference',
};

/**
 * The site exception information passed from the C++ handler.
 * See also: SiteException.
 * TODO(patricialor): Investigate making the |source| field an enum type.
 * @typedef {{embeddingOrigin: string,
 *            incognito: boolean,
 *            origin: string,
 *            displayName: string,
 *            setting: !settings.ContentSetting,
 *            source: string}}
 */
var RawSiteException;

/**
 * The site exception after it has been converted/filtered for UI use.
 * See also: RawSiteException.
 * @typedef {{category: !settings.ContentSettingsTypes,
 *            embeddingOrigin: string,
 *            incognito: boolean,
 *            origin: string,
 *            displayName: string,
 *            setting: !settings.ContentSetting,
 *            enforcement: ?chrome.settingsPrivate.Enforcement,
 *            controlledBy: !chrome.settingsPrivate.ControlledBy}}
 */
var SiteException;

/**
 * @typedef {{setting: !settings.ContentSetting,
 *            source: !ContentSettingProvider}}
 */
var DefaultContentSetting;

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

/**
 * @typedef {{origin: string,
 *            setting: string,
 *            source: string,
 *            zoom: string}}
 */
var ZoomLevelEntry;

cr.define('settings', function() {
  /** @interface */
  class SiteSettingsPrefsBrowserProxy {
    /**
     * Sets the default value for a site settings category.
     * @param {string} contentType The name of the category to change.
     * @param {string} defaultValue The name of the value to set as default.
     */
    setDefaultValueForContentType(contentType, defaultValue) {}

    /**
     * Gets the cookie details for a particular site.
     * @param {string} site The name of the site.
     * @return {!Promise<!CookieList>}
     */
    getCookieDetails(site) {}

    /**
     * Gets the default value for a site settings category.
     * @param {string} contentType The name of the category to query.
     * @return {!Promise<!DefaultContentSetting>}
     */
    getDefaultValueForContentType(contentType) {}

    /**
     * Gets the exceptions (site list) for a particular category.
     * @param {string} contentType The name of the category to query.
     * @return {!Promise<!Array<!RawSiteException>>}
     */
    getExceptionList(contentType) {}

    /**
     * Resets the category permission for a given origin (expressed as primary
     *    and secondary patterns).
     * @param {string} primaryPattern The origin to change (primary pattern).
     * @param {string} secondaryPattern The embedding origin to change
     *    (secondary pattern).
     * @param {string} contentType The name of the category to reset.
     * @param {boolean} incognito Whether this applies only to a current
     *     incognito session exception.
     */
    resetCategoryPermissionForOrigin(
        primaryPattern, secondaryPattern, contentType, incognito) {}

    /**
     * Gets a list of category permissions for a given origin. Note that this
     * may be different to the results retrieved by getExceptionList(), since it
     * combines different sources of data to get a permission's value.
     * @param {string} origin The origin to look up permissions for.
     * @param {!Array<string>} contentTypes A list of categories to retrieve
     *     the ContentSetting for.
     * @return {!Promise<!NodeList<!RawSiteException>>}
     */
    getOriginPermissions(origin, contentTypes) {}

    /**
     * Sets the category permission for a given origin (expressed as primary
     *    and secondary patterns).
     * @param {string} primaryPattern The origin to change (primary pattern).
     * @param {string} secondaryPattern The embedding origin to change
     *    (secondary pattern).
     * @param {string} contentType The name of the category to change.
     * @param {string} value The value to change the permission to.
     * @param {boolean} incognito Whether this rule applies only to the current
     *     incognito session.
     */
    setCategoryPermissionForOrigin(
        primaryPattern, secondaryPattern, contentType, value, incognito) {}

    /**
     * Checks whether a pattern is valid.
     * @param {string} pattern The pattern to check
     * @return {!Promise<boolean>} True if the pattern is valid.
     */
    isPatternValid(pattern) {}

    /**
     * Gets the list of default capture devices for a given type of media. List
     * is returned through a JS call to updateDevicesMenu.
     * @param {string} type The type to look up.
     */
    getDefaultCaptureDevices(type) {}

    /**
     * Sets a default devices for a given type of media.
     * @param {string} type The type of media to configure.
     * @param {string} defaultValue The id of the media device to set.
     */
    setDefaultCaptureDevice(type, defaultValue) {}

    /**
     * Reloads all cookies.
     * @return {!Promise<!CookieList>} Returns the full cookie
     *     list.
     */
    reloadCookies() {}

    /**
     * Fetches all children of a given cookie.
     * @param {string} path The path to the parent cookie.
     * @return {!Promise<!Array<!CookieDataSummaryItem>>} Returns a cookie list
     *     for the given path.
     */
    loadCookieChildren(path) {}

    /**
     * Removes a given cookie.
     * @param {string} path The path to the parent cookie.
     */
    removeCookie(path) {}

    /**
     * Removes all cookies.
     * @return {!Promise<!CookieList>} Returns the up to date
     *     cookie list once deletion is complete (empty list).
     */
    removeAllCookies() {}

    /**
     * observes _all_ of the the protocol handler state, which includes a list
     * that is returned through JS calls to 'setProtocolHandlers' along with
     * other state sent with the messages 'setIgnoredProtocolHandler' and
     * 'setHandlersEnabled'.
     */
    observeProtocolHandlers() {}

    /**
     * Observes one aspect of the protocol handler so that updates to the
     * enabled/disabled state are sent. A 'setHandlersEnabled' will be sent
     * from C++ immediately after receiving this observe request and updates
     * may follow via additional 'setHandlersEnabled' messages.
     *
     * If |observeProtocolHandlers| is called, there's no need to call this
     * observe as well.
     */
    observeProtocolHandlersEnabledState() {}

    /**
     * Enables or disables the ability for sites to ask to become the default
     * protocol handlers.
     * @param {boolean} enabled Whether sites can ask to become default.
     */
    setProtocolHandlerDefault(enabled) {}

    /**
     * Sets a certain url as default for a given protocol handler.
     * @param {string} protocol The protocol to set a default for.
     * @param {string} url The url to use as the default.
     */
    setProtocolDefault(protocol, url) {}

    /**
     * Deletes a certain protocol handler by url.
     * @param {string} protocol The protocol to delete the url from.
     * @param {string} url The url to delete.
     */
    removeProtocolHandler(protocol, url) {}

    /**
     * Fetches a list of all USB devices and the sites permitted to use them.
     * @return {!Promise<!Array<!UsbDeviceEntry>>} The list of USB devices.
     */
    fetchUsbDevices() {}

    /**
     * Removes a particular USB device object permission by origin and embedding
     * origin.
     * @param {string} origin The origin to look up the permission for.
     * @param {string} embeddingOrigin the embedding origin to look up.
     * @param {!UsbDeviceDetails} usbDevice The USB device to revoke permission
     *     for.
     */
    removeUsbDevice(origin, embeddingOrigin, usbDevice) {}

    /**
     * Fetches the incognito status of the current profile (whether an icognito
     * profile exists). Returns the results via onIncognitoStatusChanged.
     */
    updateIncognitoStatus() {}

    /**
     * Fetches the currently defined zoom levels for sites. Returns the results
     * via onZoomLevelsChanged.
     */
    fetchZoomLevels() {}

    /**
     * Removes a zoom levels for a given host.
     * @param {string} host The host to remove zoom levels for.
     */
    removeZoomLevel(host) {}
  }

  /**
   * @implements {settings.SiteSettingsPrefsBrowserProxy}
   */
  class SiteSettingsPrefsBrowserProxyImpl {
    /** @override */
    setDefaultValueForContentType(contentType, defaultValue) {
      chrome.send('setDefaultValueForContentType', [contentType, defaultValue]);
    }

    /** @override */
    getCookieDetails(site) {
      return cr.sendWithPromise('getCookieDetails', site);
    }

    /** @override */
    getDefaultValueForContentType(contentType) {
      return cr.sendWithPromise('getDefaultValueForContentType', contentType);
    }

    /** @override */
    getExceptionList(contentType) {
      return cr.sendWithPromise('getExceptionList', contentType);
    }

    /** @override */
    resetCategoryPermissionForOrigin(
        primaryPattern, secondaryPattern, contentType, incognito) {
      chrome.send(
          'resetCategoryPermissionForOrigin',
          [primaryPattern, secondaryPattern, contentType, incognito]);
    }

    /** @override */
    getOriginPermissions(origin, contentTypes) {
      return cr.sendWithPromise('getOriginPermissions', origin, contentTypes);
    }

    /** @override */
    setCategoryPermissionForOrigin(
        primaryPattern, secondaryPattern, contentType, value, incognito) {
      // TODO(dschuyler): It may be incorrect for JS to send the embeddingOrigin
      // pattern. Look into removing this parameter from site_settings_handler.
      // Ignoring the |secondaryPattern| and using '' instead is a quick-fix.
      chrome.send(
          'setCategoryPermissionForOrigin',
          [primaryPattern, '', contentType, value, incognito]);
    }

    /** @override */
    isPatternValid(pattern) {
      return cr.sendWithPromise('isPatternValid', pattern);
    }

    /** @override */
    getDefaultCaptureDevices(type) {
      chrome.send('getDefaultCaptureDevices', [type]);
    }

    /** @override */
    setDefaultCaptureDevice(type, defaultValue) {
      chrome.send('setDefaultCaptureDevice', [type, defaultValue]);
    }

    /** @override */
    reloadCookies() {
      return cr.sendWithPromise('reloadCookies');
    }

    /** @override */
    loadCookieChildren(path) {
      return cr.sendWithPromise('loadCookie', path);
    }

    /** @override */
    removeCookie(path) {
      chrome.send('removeCookie', [path]);
    }

    /** @override */
    removeAllCookies() {
      return cr.sendWithPromise('removeAllCookies');
    }

    /** @override */
    observeProtocolHandlers() {
      chrome.send('observeProtocolHandlers');
    }

    /** @override */
    observeProtocolHandlersEnabledState() {
      chrome.send('observeProtocolHandlersEnabledState');
    }

    /** @override */
    setProtocolHandlerDefault(enabled) {
      chrome.send('setHandlersEnabled', [enabled]);
    }

    /** @override */
    setProtocolDefault(protocol, url) {
      chrome.send('setDefault', [[protocol, url]]);
    }

    /** @override */
    removeProtocolHandler(protocol, url) {
      chrome.send('removeHandler', [[protocol, url]]);
    }

    /** @override */
    fetchUsbDevices() {
      return cr.sendWithPromise('fetchUsbDevices');
    }

    /** @override */
    removeUsbDevice(origin, embeddingOrigin, usbDevice) {
      chrome.send('removeUsbDevice', [origin, embeddingOrigin, usbDevice]);
    }

    /** @override */
    updateIncognitoStatus() {
      chrome.send('updateIncognitoStatus');
    }

    /** @override */
    fetchZoomLevels() {
      chrome.send('fetchZoomLevels');
    }

    /** @override */
    removeZoomLevel(host) {
      chrome.send('removeZoomLevel', [host]);
    }
  }

  // The singleton instance_ is replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(SiteSettingsPrefsBrowserProxyImpl);

  return {
    SiteSettingsPrefsBrowserProxy: SiteSettingsPrefsBrowserProxy,
    SiteSettingsPrefsBrowserProxyImpl: SiteSettingsPrefsBrowserProxyImpl,
  };
});

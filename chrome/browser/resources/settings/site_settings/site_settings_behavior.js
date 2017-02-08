// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior common to Site Settings classes.
 */

/** @polymerBehavior */
var SiteSettingsBehaviorImpl = {
  properties: {
    /**
     * The string ID of the category this element is displaying data for.
     * See site_settings/constants.js for possible values.
     */
    category: String,

    /**
     * The browser proxy used to retrieve and change information about site
     * settings categories and the sites within.
     * @type {settings.SiteSettingsPrefsBrowserProxy}
     */
    browserProxy: Object,
  },

  created: function() {
    this.browserProxy =
        settings.SiteSettingsPrefsBrowserProxyImpl.getInstance();
  },

  ready: function() {
    this.PermissionValues = settings.PermissionValues;
  },

  /**
   * Ensures the URL has a scheme (assumes http if omitted).
   * @param {string} url The URL with or without a scheme.
   * @return {string} The URL with a scheme, or an empty string.
   */
  ensureUrlHasScheme: function(url) {
    if (url.length == 0) return url;
    return url.includes('://') ? url : 'http://' + url;
  },

  /**
   * Removes redundant ports, such as port 80 for http and 443 for https.
   * @param {string} url The URL to sanitize.
   * @return {string} The URL without redundant ports, if any.
   */
  sanitizePort: function(url) {
    var urlWithScheme = this.ensureUrlHasScheme(url);
    if (urlWithScheme.startsWith('https://') &&
        urlWithScheme.endsWith(':443')) {
      return url.slice(0, -4);
    }
    if (urlWithScheme.startsWith('http://') &&
        urlWithScheme.endsWith(':80')) {
      return url.slice(0, -3);
    }
    return url;
  },

  /**
   * Removes the wildcard prefix from a pattern string.
   * @param {string} pattern The pattern to remove the wildcard from.
   * @return {string} The resulting pattern.
   * @private
   */
  removePatternWildcard: function(pattern) {
    if (pattern.startsWith('http://[*.]'))
      return pattern.replace('http://[*.]', 'http://');
    else if (pattern.startsWith('https://[*.]'))
      return pattern.replace('https://[*.]', 'https://');
    else if (pattern.startsWith('[*.]'))
      return pattern.substring(4, pattern.length);
    return pattern;
  },

  /**
   * Looks up the human-friendly embedder string to show in the UI.
   * @param {string} embeddingOrigin The embedding origin to show.
   * @param {string} category The category requesting it.
   * @return {string} The string to show.
   */
  getEmbedderString: function(embeddingOrigin, category) {
    if (embeddingOrigin == '') {
      if (category != settings.ContentSettingsTypes.GEOLOCATION)
        return '';
      return loadTimeData.getStringF('embeddedOnHost', '*');
    }
    return loadTimeData.getStringF(
        'embeddedOnHost', this.sanitizePort(embeddingOrigin));
  },

  /**
   * Returns true if this exception is controlled by, for example, a policy or
   * set by an extension.
   * @param {string} source The source controlling the extension
   * @return {boolean} Whether it is being controlled.
   * @protected
   */
  isExceptionControlled_: function(source) {
    return source != undefined && source != 'preference';
  },

  /**
   * Returns the icon to use for a given site.
   * @param {string} site The url of the site to fetch the icon for.
   * @return {string} The background-image style with the favicon.
   * @private
   */
  computeSiteIcon: function(site) {
    var url = this.ensureUrlHasScheme(site);
    return 'background-image: ' + cr.icon.getFavicon(url);
  },

  /**
   * Returns true if the passed content setting is considered 'enabled'.
   * @param {string} setting
   * @return {boolean}
   * @private
   */
  computeIsSettingEnabled: function(setting) {
    return setting != settings.PermissionValues.BLOCK;
  },

  /**
   * Converts a string origin/pattern to a URL.
   * @param {string} originOrPattern The origin/pattern to convert to URL.
   * @return {URL} The URL to return (or null if origin is not a valid URL).
   * @private
   */
  toUrl: function(originOrPattern) {
    if (originOrPattern.length == 0)
      return null;
    // TODO(finnur): Hmm, it would probably be better to ensure scheme on the
    //     JS/C++ boundary.
    // TODO(dschuyler): I agree. This filtering should be done in one go, rather
    // that during the sort. The URL generation should be wrapped in a try/catch
    // as well.
    originOrPattern = originOrPattern.replace('*://', '');
    originOrPattern = originOrPattern.replace('[*.]', '');
    return new URL(this.ensureUrlHasScheme(originOrPattern));
  },

  /**
   * Convert an exception (received from the C++ handler) to a full
   * SiteException.
   * @param {!Object} exception The raw site exception from C++.
   * @return {SiteException} The expanded (full) SiteException.
   * @private
   */
  expandSiteException: function(exception) {
    var origin = exception.origin;
    var embeddingOrigin = exception.embeddingOrigin;
    var embeddingDisplayName = '';
    if (origin != embeddingOrigin) {
      embeddingDisplayName =
          this.getEmbedderString(embeddingOrigin, this.category);
    }

    return {
      origin: origin,
      displayName: exception.displayName,
      embeddingOrigin: embeddingOrigin,
      embeddingDisplayName: embeddingDisplayName,
      incognito: exception.incognito,
      setting: exception.setting,
      source: exception.source,
    };
  },

};

/** @polymerBehavior */
var SiteSettingsBehavior = [SiteSettingsBehaviorImpl];

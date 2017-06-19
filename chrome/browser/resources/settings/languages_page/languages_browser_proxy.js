// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the languages section
 * to interact with the browser.
 */

cr.define('settings', function() {
  /** @interface */
  function LanguagesBrowserProxy() {}

  LanguagesBrowserProxy.prototype = {
    // <if expr="chromeos or is_win">
    /**
     * Sets the prospective UI language to the chosen language. This won't
     * affect the actual UI language until a restart.
     * @param {string} languageCode
     */
    setProspectiveUILanguage: function(languageCode) {},

    /** @return {!Promise<string>} */
    getProspectiveUILanguage: function() {},
    // </if>

    /** @return {!LanguageSettingsPrivate} */
    getLanguageSettingsPrivate: function() {},

    // <if expr="chromeos">
    /** @return {!InputMethodPrivate} */
    getInputMethodPrivate: function() {},
    // </if>
  };

  /**
   * @constructor
   * @implements {settings.LanguagesBrowserProxy}
   */
  function LanguagesBrowserProxyImpl() {}
  // The singleton instance_ is replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(LanguagesBrowserProxyImpl);

  LanguagesBrowserProxyImpl.prototype = {
    // <if expr="chromeos or is_win">
    /** @override */
    setProspectiveUILanguage: function(languageCode) {
      chrome.send('setProspectiveUILanguage', [languageCode]);
    },

    /** @override */
    getProspectiveUILanguage: function() {
      return cr.sendWithPromise('getProspectiveUILanguage');
    },
    // </if>

    /** @override */
    getLanguageSettingsPrivate: function() {
      return /** @type {!LanguageSettingsPrivate} */ (
          chrome.languageSettingsPrivate);
    },

    // <if expr="chromeos">
    /** @override */
    getInputMethodPrivate: function() {
      return /** @type {!InputMethodPrivate} */ (chrome.inputMethodPrivate);
    },
    // </if>
  };

  return {
    LanguagesBrowserProxy: LanguagesBrowserProxy,
    LanguagesBrowserProxyImpl: LanguagesBrowserProxyImpl,
  };
});

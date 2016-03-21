// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../options_browsertest_base.js']);
GEN('#include "chrome/browser/ui/webui/options/chromeos/' +
    'guest_mode_options_browsertest.h"');

/**
 * TestFixture for guest mode options WebUI testing.
 * @extends {OptionsBrowsertestBase}
 * @constructor
 */
function GuestModeOptionsUIBrowserTest() {}

GuestModeOptionsUIBrowserTest.prototype = {
  __proto__: OptionsBrowsertestBase.prototype,

  /** @override */
  browsePreload: 'chrome://settings-frame',

  /** @override */
  typedefCppFixture: 'GuestModeOptionsUIBrowserTest',

  /**
   * Returns the element that sets a given preference, failing if no such
   * element is found.
   * @param {string} pref Name of the preference.
   * @return {!HTMLElement} The element controlling the preference.
   */
  getControlForPref: function(pref) {
    var control = document.querySelector('[pref="' + pref + '"]');
    assertTrue(!!control);
    return control;
  },

  /** @param {!HTMLElement} el */
  expectHidden: function(el) {
    expectFalse(el.offsetHeight > 0 && el.offsetWidth > 0);
  },
};

/** Test sections that should be hidden in guest mode. */
TEST_F('GuestModeOptionsUIBrowserTest', 'testSections', function() {
  this.expectHidden($('startup-section'));
  this.expectHidden($('appearance-section'));
  this.expectHidden($('android-apps-section'));
  this.expectHidden($('sync-users-section'));
  this.expectHidden($('easy-unlock-section'));
  this.expectHidden($('reset-profile-settings-section'));
});

/** Test controls that should be disabled in guest mode. */
TEST_F('GuestModeOptionsUIBrowserTest', 'testControls', function() {
  // Appearance section.
  var setWallpaper = $('set-wallpaper');
  expectTrue(setWallpaper.disabled);

  // Passwords and autofill section.
  expectTrue($('autofill-enabled').disabled);

  // Date and time section.
  expectTrue($('timezone-value-select').disabled);
  expectFalse($('resolve-timezone-by-geolocation').disabled);
  expectFalse($('use-24hour-clock').disabled);

  // Privacy section.
  expectTrue(this.getControlForPref('search.suggest_enabled').disabled);
  expectTrue($('networkPredictionOptions').disabled);

  // Web content section.
  expectTrue($('defaultZoomFactor').disabled);

  // Downloads section.
  expectTrue(this.getControlForPref('gdata.disabled').disabled);

  // Content settings overlay.
  expectTrue(this.getControlForPref('settings.privacy.drm_enabled').disabled);
  expectTrue($('protected-content-exceptions').disabled);
});

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for category-default-setting. */
suite('CategoryDefaultSetting', function() {
  /**
   * A site settings category created before each test.
   * @type {SiteSettingsCategory}
   */
  var testElement;

  /**
   * The mock proxy object to use during test.
   * @type {TestSiteSettingsPrefsBrowserProxy}
   */
  var browserProxy = null;

  // Initialize a site-settings-category before each test.
  setup(function() {
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    settings.SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    testElement = document.createElement('category-default-setting');
    testElement.subOptionLabel = 'test label';
    document.body.appendChild(testElement);
  });

  test('getDefaultValueForContentType API used', function() {
    testElement.category = settings.ContentSettingsTypes.GEOLOCATION;
    return browserProxy.whenCalled('getDefaultValueForContentType')
        .then(function(contentType) {
          assertEquals(settings.ContentSettingsTypes.GEOLOCATION, contentType);
        });
  });

  // Verifies that the widget works as expected for a given |category|, initial
  // |prefs|, and given expectations.
  function testCategoryEnabled(
      testElement, category, prefs, expectedEnabled,
      expectedEnabledContentSetting) {
    browserProxy.reset();
    browserProxy.setPrefs(prefs);

    testElement.category = category;
    return browserProxy.whenCalled('getDefaultValueForContentType')
        .then(function(contentType) {
          assertEquals(category, contentType);
          assertEquals(expectedEnabled, testElement.categoryEnabled);
          browserProxy.resetResolver('setDefaultValueForContentType');
          MockInteractions.tap(testElement.$.toggle);
          return browserProxy.whenCalled('setDefaultValueForContentType');
        })
        .then(function(args) {
          assertEquals(category, args[0]);
          var oppositeSetting = expectedEnabled ?
              settings.ContentSetting.BLOCK :
              expectedEnabledContentSetting;
          assertEquals(oppositeSetting, args[1]);
          assertNotEquals(expectedEnabled, testElement.categoryEnabled);
        });
  }

  test('categoryEnabled correctly represents prefs (enabled)', function() {
    /**
     * An example pref where the location category is enabled.
     * @type {SiteSettingsPref}
     */
    var prefsLocationEnabled = {
      defaults: {
        geolocation: {
          setting: 'allow',
        },
      },
    };

    return testCategoryEnabled(
        testElement, settings.ContentSettingsTypes.GEOLOCATION,
        prefsLocationEnabled, true, settings.ContentSetting.ASK);
  });

  test('categoryEnabled correctly represents prefs (disabled)', function() {
    /**
     * An example pref where the location category is disabled.
     * @type {SiteSettingsPref}
     */
    var prefsLocationDisabled = {
      defaults: {
        geolocation: {
          setting: 'block',
        },
      },
    };

    return testCategoryEnabled(
        testElement, settings.ContentSettingsTypes.GEOLOCATION,
        prefsLocationDisabled, false, settings.ContentSetting.ASK);
  });

  test('test Flash content setting in DETECT/ASK setting', function() {
    var prefsFlash = {
      defaults: {
        plugins: {
          setting: 'detect_important_content',
        },
      },
    };

    return testCategoryEnabled(
        testElement, settings.ContentSettingsTypes.PLUGINS, prefsFlash, true,
        settings.ContentSetting.IMPORTANT_CONTENT);
  });

  test('test Flash content setting in legacy ALLOW setting', function() {
    var prefsFlash = {
      defaults: {
        plugins: {
          setting: 'allow',
        },
      },
    };

    return testCategoryEnabled(
        testElement, settings.ContentSettingsTypes.PLUGINS, prefsFlash, true,
        settings.ContentSetting.IMPORTANT_CONTENT);
  });

  test('test Flash content setting in BLOCK setting', function() {
    var prefsFlash = {
      defaults: {
        plugins: {
          setting: 'block',
        },
      },
    };

    return testCategoryEnabled(
        testElement, settings.ContentSettingsTypes.PLUGINS, prefsFlash, false,
        settings.ContentSetting.IMPORTANT_CONTENT);
  });

  function testTristateCategory(
      prefs, category, thirdState, secondaryToggleId) {
    browserProxy.setPrefs(prefs);

    testElement.category = category;
    var secondaryToggle = null;

    return browserProxy.whenCalled('getDefaultValueForContentType')
        .then(function(contentType) {
          Polymer.dom.flush();
          secondaryToggle = testElement.$$(secondaryToggleId);
          assertTrue(!!secondaryToggle);

          assertEquals(category, contentType);
          assertTrue(testElement.categoryEnabled);
          assertFalse(secondaryToggle.disabled);
          assertTrue(secondaryToggle.checked);

          browserProxy.resetResolver('setDefaultValueForContentType');
          MockInteractions.tap(testElement.$.toggle);
          return browserProxy.whenCalled('setDefaultValueForContentType');
        })
        .then(function(args) {
          // Check THIRD_STATE => BLOCK transition succeeded.
          Polymer.dom.flush();

          assertEquals(category, args[0]);
          assertEquals(settings.ContentSetting.BLOCK, args[1]);
          assertFalse(testElement.categoryEnabled);
          assertTrue(secondaryToggle.disabled);
          assertTrue(secondaryToggle.checked);

          browserProxy.resetResolver('setDefaultValueForContentType');
          MockInteractions.tap(testElement.$.toggle);
          return browserProxy.whenCalled('setDefaultValueForContentType');
        })
        .then(function(args) {
          // Check BLOCK => THIRD_STATE transition succeeded.
          Polymer.dom.flush();

          assertEquals(category, args[0]);
          assertEquals(thirdState, args[1]);
          assertTrue(testElement.categoryEnabled);
          assertFalse(secondaryToggle.disabled);
          assertTrue(secondaryToggle.checked);

          browserProxy.resetResolver('setDefaultValueForContentType');
          MockInteractions.tap(secondaryToggle);
          return browserProxy.whenCalled('setDefaultValueForContentType');
        })
        .then(function(args) {
          // Check THIRD_STATE => ALLOW transition succeeded.
          Polymer.dom.flush();

          assertEquals(category, args[0]);
          assertEquals(settings.ContentSetting.ALLOW, args[1]);
          assertTrue(testElement.categoryEnabled);
          assertFalse(secondaryToggle.disabled);
          assertFalse(secondaryToggle.checked);

          browserProxy.resetResolver('setDefaultValueForContentType');
          MockInteractions.tap(testElement.$.toggle);
          return browserProxy.whenCalled('setDefaultValueForContentType');
        })
        .then(function(args) {
          // Check ALLOW => BLOCK transition succeeded.
          Polymer.dom.flush();

          assertEquals(category, args[0]);
          assertEquals(settings.ContentSetting.BLOCK, args[1]);
          assertFalse(testElement.categoryEnabled);
          assertTrue(secondaryToggle.disabled);
          assertFalse(secondaryToggle.checked);

          browserProxy.resetResolver('setDefaultValueForContentType');
          MockInteractions.tap(testElement.$.toggle);
          return browserProxy.whenCalled('setDefaultValueForContentType');
        })
        .then(function(args) {
          // Check BLOCK => ALLOW transition succeeded.
          Polymer.dom.flush();

          assertEquals(category, args[0]);
          assertEquals(settings.ContentSetting.ALLOW, args[1]);
          assertTrue(testElement.categoryEnabled);
          assertFalse(secondaryToggle.disabled);
          assertFalse(secondaryToggle.checked);

          browserProxy.resetResolver('setDefaultValueForContentType');
          MockInteractions.tap(secondaryToggle);
          return browserProxy.whenCalled('setDefaultValueForContentType');
        })
        .then(function(args) {
          // Check ALLOW => THIRD_STATE transition succeeded.
          Polymer.dom.flush();

          assertEquals(category, args[0]);
          assertEquals(thirdState, args[1]);
          assertTrue(testElement.categoryEnabled);
          assertFalse(secondaryToggle.disabled);
          assertTrue(secondaryToggle.checked);
        });
  }

  test('test special tri-state Cookies category', function() {
    /**
     * An example pref where the Cookies category is set to delete when
     * session ends.
     */
    var prefsCookiesSessionOnly = {
      defaults: {
        cookies: {
          setting: 'session_only',
        },
      },
    };

    return testTristateCategory(
        prefsCookiesSessionOnly, settings.ContentSettingsTypes.COOKIES,
        settings.ContentSetting.SESSION_ONLY, '#subOptionToggle');
  });
});

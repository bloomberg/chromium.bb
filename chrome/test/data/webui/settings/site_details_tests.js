// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for site-details. */
suite('SiteDetails', function() {
  /**
   * A site list element created before each test.
   * @type {SiteDetails}
   */
  var testElement;

  /**
   * An example pref with 1 pref in each category.
   * @type {SiteSettingsPref}
   */
  var prefs;

  // Initialize a site-details before each test.
  setup(function() {
    prefs = {
      defaults: {
        auto_downloads: {
          setting: settings.ContentSetting.ASK,
        },
        background_sync: {
          setting: settings.ContentSetting.ALLOW,
        },
        camera: {
          setting: settings.ContentSetting.ASK,
        },
        geolocation: {
          setting: settings.ContentSetting.ASK,
        },
        images: {
          setting: settings.ContentSetting.ALLOW,
        },
        javascript: {
          setting: settings.ContentSetting.ALLOW,
        },
        mic: {
          setting: settings.ContentSetting.ASK,
        },
        notifications: {
          setting: settings.ContentSetting.ASK,
        },
        plugins: {
          setting: settings.ContentSetting.ASK,
        },
        popups: {
          setting: settings.ContentSetting.BLOCK,
        },
        unsandboxed_plugins: {
          setting: settings.ContentSetting.ASK,
        },
      },
      exceptions: {
        auto_downloads: [
          {
            embeddingOrigin: 'https://foo.com:443',
            origin: 'https://foo.com:443',
            setting: settings.ContentSetting.ALLOW,
            source: 'preference',
          },
        ],
        background_sync: [
          {
            embeddingOrigin: 'https://foo.com:443',
            origin: 'https://foo.com:443',
            setting: settings.ContentSetting.ALLOW,
            source: 'preference',
          },
        ],
        camera: [
          {
            embeddingOrigin: 'https://foo.com:443',
            origin: 'https://foo.com:443',
            setting: settings.ContentSetting.ALLOW,
            source: 'preference',
          },
        ],
        geolocation: [
          {
            embeddingOrigin: 'https://foo.com:443',
            origin: 'https://foo.com:443',
            setting: settings.ContentSetting.ALLOW,
            source: 'preference',
          },
        ],
        images: [
          {
            embeddingOrigin: 'https://foo.com:443',
            origin: 'https://foo.com:443',
            setting: settings.ContentSetting.ALLOW,
            source: 'default',
          },
        ],
        javascript: [
          {
            embeddingOrigin: 'https://foo.com:443',
            origin: 'https://foo.com:443',
            setting: settings.ContentSetting.ALLOW,
            source: 'preference',
          },
        ],
        mic: [
          {
            embeddingOrigin: 'https://foo.com:443',
            origin: 'https://foo.com:443',
            setting: settings.ContentSetting.ALLOW,
            source: 'preference',
          },
        ],
        notifications: [
          {
            embeddingOrigin: 'https://foo.com:443',
            origin: 'https://foo.com:443',
            setting: settings.ContentSetting.BLOCK,
            source: 'policy',
          },
        ],
        plugins: [
          {
            embeddingOrigin: 'https://foo.com:443',
            origin: 'https://foo.com:443',
            setting: settings.ContentSetting.ALLOW,
            source: 'extension',
          },
        ],
        popups: [
          {
            embeddingOrigin: 'https://foo.com:443',
            origin: 'https://foo.com:443',
            setting: settings.ContentSetting.BLOCK,
            source: 'default',
          },
        ],
        unsandboxed_plugins: [
          {
            embeddingOrigin: 'https://foo.com:443',
            origin: 'https://foo.com:443',
            setting: settings.ContentSetting.ALLOW,
            source: 'preference',
          },
        ],
      }
    };

    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    settings.SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    testElement = document.createElement('site-details');
    document.body.appendChild(testElement);
  });

  test('usage heading shows on storage available', function() {
    // Remove the current website-usage-private-api element.
    var parent = testElement.$.usageApi.parentNode;
    testElement.$.usageApi.remove();

    // Replace it with a mock version.
    Polymer({
      is: 'mock-website-usage-private-api',

      fetchUsageTotal: function(origin) {
        testElement.storedData_ = '1 KB';
      },
    });
    var api = document.createElement('mock-website-usage-private-api');
    testElement.$.usageApi = api;
    Polymer.dom(parent).appendChild(api);

    browserProxy.setPrefs(prefs);
    testElement.origin = 'https://foo.com:443';

    Polymer.dom.flush();

    // Expect usage to be rendered.
    assertTrue(!!testElement.$$('#usage'));
  });

  test('correct pref settings are shown', function() {
    browserProxy.setPrefs(prefs);
    testElement.origin = 'https://foo.com:443';

    return browserProxy.whenCalled('getOriginPermissions').then(function() {
      testElement.root.querySelectorAll('site-details-permission')
          .forEach(function(siteDetailsPermission) {
            // Verify settings match the values specified in |prefs|.
            var expectedSetting = settings.ContentSetting.ALLOW;
            var expectedSource = 'preference';
            var expectedMenuValue = settings.ContentSetting.ALLOW;

            // For all the categories with non-user-set 'Allow' preferences,
            // update expected values.
            if (siteDetailsPermission.category ==
                    settings.ContentSettingsTypes.NOTIFICATIONS ||
                siteDetailsPermission.category ==
                    settings.ContentSettingsTypes.PLUGINS ||
                siteDetailsPermission.category ==
                    settings.ContentSettingsTypes.JAVASCRIPT ||
                siteDetailsPermission.category ==
                    settings.ContentSettingsTypes.IMAGES ||
                siteDetailsPermission.category ==
                    settings.ContentSettingsTypes.POPUPS) {
              expectedSetting =
                  prefs.exceptions[siteDetailsPermission.category][0].setting;
              expectedSource =
                  prefs.exceptions[siteDetailsPermission.category][0].source;
              expectedMenuValue = (expectedSource == 'default') ?
                  settings.ContentSetting.DEFAULT :
                  expectedSetting;
            }
            assertEquals(expectedSetting, siteDetailsPermission.site.setting);
            assertEquals(expectedSource, siteDetailsPermission.site.source);
            assertEquals(
                expectedMenuValue, siteDetailsPermission.$.permission.value);
          });
    });
  });
});

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
        midi_devices: {
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
            source: settings.SiteSettingSource.PREFERENCE,
          },
        ],
        background_sync: [
          {
            embeddingOrigin: 'https://foo.com:443',
            origin: 'https://foo.com:443',
            setting: settings.ContentSetting.ALLOW,
            source: settings.SiteSettingSource.PREFERENCE,
          },
        ],
        camera: [
          {
            embeddingOrigin: 'https://foo.com:443',
            origin: 'https://foo.com:443',
            setting: settings.ContentSetting.ALLOW,
            source: settings.SiteSettingSource.PREFERENCE,
          },
        ],
        geolocation: [
          {
            embeddingOrigin: 'https://foo.com:443',
            origin: 'https://foo.com:443',
            setting: settings.ContentSetting.ALLOW,
            source: settings.SiteSettingSource.PREFERENCE,
          },
        ],
        images: [
          {
            embeddingOrigin: 'https://foo.com:443',
            origin: 'https://foo.com:443',
            setting: settings.ContentSetting.ALLOW,
            source: settings.SiteSettingSource.DEFAULT,
          },
        ],
        javascript: [
          {
            embeddingOrigin: 'https://foo.com:443',
            origin: 'https://foo.com:443',
            setting: settings.ContentSetting.ALLOW,
            source: settings.SiteSettingSource.PREFERENCE,
          },
        ],
        mic: [
          {
            embeddingOrigin: 'https://foo.com:443',
            origin: 'https://foo.com:443',
            setting: settings.ContentSetting.ALLOW,
            source: settings.SiteSettingSource.PREFERENCE,
          },
        ],
        midi_devices: [
          {
            embeddingOrigin: 'https://foo.com:443',
            origin: 'https://foo.com:443',
            setting: settings.ContentSetting.ALLOW,
            source: settings.SiteSettingSource.PREFERENCE,
          },
        ],
        notifications: [
          {
            embeddingOrigin: 'https://foo.com:443',
            origin: 'https://foo.com:443',
            setting: settings.ContentSetting.BLOCK,
            source: settings.SiteSettingSource.POLICY,
          },
        ],
        plugins: [
          {
            embeddingOrigin: 'https://foo.com:443',
            origin: 'https://foo.com:443',
            setting: settings.ContentSetting.ALLOW,
            source: settings.SiteSettingSource.EXTENSION,
          },
        ],
        popups: [
          {
            embeddingOrigin: 'https://foo.com:443',
            origin: 'https://foo.com:443',
            setting: settings.ContentSetting.BLOCK,
            source: settings.SiteSettingSource.DEFAULT,
          },
        ],
        unsandboxed_plugins: [
          {
            embeddingOrigin: 'https://foo.com:443',
            origin: 'https://foo.com:443',
            setting: settings.ContentSetting.ALLOW,
            source: settings.SiteSettingSource.PREFERENCE,
          },
        ],
      }
    };

    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    settings.SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
  });

  function createSiteDetails(origin) {
    var siteDetailsElement = document.createElement('site-details');
    document.body.appendChild(siteDetailsElement);
    siteDetailsElement.origin = origin;
    return siteDetailsElement;
  };

  test('usage heading shows when site settings enabled', function() {
    browserProxy.setPrefs(prefs);
    // Expect usage to be hidden when Site Settings is disabled.
    loadTimeData.overrideValues({enableSiteSettings: false});
    testElement = createSiteDetails('https://foo.com:443');
    Polymer.dom.flush();
    assert(!testElement.$$('#usage'));

    loadTimeData.overrideValues({enableSiteSettings: true});
    testElement = createSiteDetails('https://foo.com:443');
    Polymer.dom.flush();
    assert(!!testElement.$$('#usage'));

    // When there's no usage, there should be a string that says so.
    assertEquals('', testElement.storedData_);
    assertFalse(testElement.$$('#noStorage').hidden);
    assertTrue(testElement.$$('#storage').hidden);
    assertTrue(
        testElement.$$('#usage').innerText.indexOf('No usage data') != -1);

    // If there is, check the correct amount of usage is specified.
    testElement.storedData_ = '1 KB';
    assertTrue(testElement.$$('#noStorage').hidden);
    assertFalse(testElement.$$('#storage').hidden);
    assertTrue(testElement.$$('#usage').innerText.indexOf('1 KB') != -1);
  });

  test('correct pref settings are shown', function() {
    browserProxy.setPrefs(prefs);
    testElement = createSiteDetails('https://foo.com:443');

    return browserProxy.whenCalled('getOriginPermissions').then(function() {
      testElement.root.querySelectorAll('site-details-permission')
          .forEach(function(siteDetailsPermission) {
            // Verify settings match the values specified in |prefs|.
            var expectedSetting = settings.ContentSetting.ALLOW;
            var expectedSource = settings.SiteSettingSource.PREFERENCE;
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
              expectedMenuValue =
                  (expectedSource == settings.SiteSettingSource.DEFAULT) ?
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

  test('show confirmation dialog on reset settings', function() {
    browserProxy.setPrefs(prefs);
    testElement = createSiteDetails('https://foo.com:443');

    // Check both cancelling and accepting the dialog closes it.
    ['cancel-button', 'action-button'].forEach(buttonType => {
      MockInteractions.tap(testElement.$.clearAndReset);
      assertTrue(testElement.$.confirmDeleteDialog.open);
      var actionButtonList =
          testElement.$.confirmDeleteDialog.getElementsByClassName(buttonType);
      assertEquals(1, actionButtonList.length);
      MockInteractions.tap(actionButtonList[0]);
      assertFalse(testElement.$.confirmDeleteDialog.open);
    });

    // Accepting the dialog will make a call to setOriginPermissions.
    return browserProxy.whenCalled('setOriginPermissions').then(function(args) {
      assertEquals(testElement.origin, args[0]);
      assertDeepEquals(testElement.getCategoryList_(), args[1]);
      assertEquals(settings.ContentSetting.DEFAULT, args[2]);
    })
  });
});

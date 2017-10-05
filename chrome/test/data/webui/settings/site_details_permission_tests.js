// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for site-details. */
suite('SiteDetailsPermission', function() {
  /**
   * A site list element created before each test.
   * @type {SiteDetailsPermission}
   */
  var testElement;

  /**
   * An example pref with only camera allowed.
   * @type {SiteSettingsPref}
   */
  var prefs;

  // Initialize a site-details-permission before each test.
  setup(function() {
    prefs = {
      defaults: {
        camera: {
          setting: settings.ContentSetting.ALLOW,
        }
      },
      exceptions: {
        camera: [
          {
            embeddingOrigin: '',
            origin: 'https://www.example.com',
            setting: settings.ContentSetting.ALLOW,
            source: settings.SiteSettingSource.PREFERENCE,
          },
        ]
      }
    };

    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    settings.SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    testElement = document.createElement('site-details-permission');

    // Set the camera icon on <site-details-permission> manually to avoid
    // failures on undefined icons during teardown in PolymerTest.testIronIcons.
    // In practice, this is done from the parent.
    testElement.icon = 'settings:videocam';
    document.body.appendChild(testElement);
  });

  function validatePermissionFlipWorks(origin, expectedContentSetting) {
    browserProxy.resetResolver('setOriginPermissions');

    // Simulate permission change initiated by the user.
    testElement.$.permission.value = expectedContentSetting;
    testElement.$.permission.dispatchEvent(new CustomEvent('change'));

    return browserProxy.whenCalled('setOriginPermissions').then((args) => {
      assertEquals(origin, args[0]);
      assertDeepEquals([testElement.category], args[1]);
      assertEquals(expectedContentSetting, args[2]);
    });
  };

  test('camera category', function() {
    var origin = 'https://www.example.com';
    browserProxy.setPrefs(prefs);
    testElement.category = settings.ContentSettingsTypes.CAMERA;
    testElement.label = 'Camera';
    testElement.site = {
      origin: origin,
      embeddingOrigin: '',
      source: settings.SiteSettingSource.PREFERENCE,
    };

    assertFalse(testElement.$.details.hidden);

    var header = testElement.$.details.querySelector('#permissionHeader');
    assertEquals(
        'Camera', header.innerText.trim(),
        'Widget should be labelled correctly');

    // Flip the permission and validate that prefs stay in sync.
    return validatePermissionFlipWorks(origin, settings.ContentSetting.ALLOW)
        .then(() => {
          return validatePermissionFlipWorks(
              origin, settings.ContentSetting.BLOCK);
        })
        .then(() => {
          return validatePermissionFlipWorks(
              origin, settings.ContentSetting.ALLOW);
        })
        .then(() => {
          return validatePermissionFlipWorks(
              origin, settings.ContentSetting.DEFAULT);
        });
  });

  test('default string is correct', function() {
    var origin = 'https://www.example.com';
    browserProxy.setPrefs(prefs)
    testElement.category = settings.ContentSettingsTypes.CAMERA;
    testElement.label = 'Camera';
    testElement.site = {
      origin: origin,
      embeddingOrigin: '',
      setting: settings.ContentSetting.ALLOW,
      source: settings.SiteSettingSource.PREFERENCE,
    };

    return browserProxy.whenCalled('getDefaultValueForContentType')
        .then((args) => {
          // Check getDefaultValueForContentType was called for camera category.
          assertEquals(settings.ContentSettingsTypes.CAMERA, args);

          // The default option will always be the first in the menu.
          assertEquals(
              'Allow (default)', testElement.$.permission.options[0].text,
              'Default setting string should match prefs');
          browserProxy.resetResolver('getDefaultValueForContentType');
          var defaultPrefs = {
            camera: {
              setting: settings.ContentSetting.BLOCK,
            }
          };
          browserProxy.setDefaultPrefs(defaultPrefs);
          return browserProxy.whenCalled('getDefaultValueForContentType');
        })
        .then((args) => {
          assertEquals(settings.ContentSettingsTypes.CAMERA, args);
          assertEquals(
              'Block (default)', testElement.$.permission.options[0].text,
              'Default setting string should match prefs');
          browserProxy.resetResolver('getDefaultValueForContentType');
          var defaultPrefs = {
            camera: {
              setting: settings.ContentSetting.ASK,
            }
          };
          browserProxy.setDefaultPrefs(defaultPrefs);
          return browserProxy.whenCalled('getDefaultValueForContentType');
        })
        .then((args) => {
          assertEquals(settings.ContentSettingsTypes.CAMERA, args);
          assertEquals(
              'Ask (default)', testElement.$.permission.options[0].text,
              'Default setting string should match prefs');
        });
  });

  test('source string is correct', function() {
    var origin = 'https://www.example.com';
    testElement.category = settings.ContentSettingsTypes.CAMERA;

    // Strings that should be shown for the permission sources that don't depend
    // on the ContentSetting value.
    var permissionSourcesNoSetting = {};
    permissionSourcesNoSetting[settings.SiteSettingSource.DEFAULT] = '';
    permissionSourcesNoSetting[settings.SiteSettingSource.PREFERENCE] = '';
    permissionSourcesNoSetting[settings.SiteSettingSource.EMBARGO] =
        'Automatically blocked';
    permissionSourcesNoSetting[settings.SiteSettingSource.INSECURE_ORIGIN] =
        'Blocked to protect your privacy';
    permissionSourcesNoSetting[settings.SiteSettingSource.KILL_SWITCH] =
        'Temporarily blocked to protect your security';

    for (testSource in permissionSourcesNoSetting) {
      testElement.site = {
        origin: origin,
        embeddingOrigin: origin,
        setting: settings.ContentSetting.BLOCK,
        source: testSource,
      };
      assertEquals(
          permissionSourcesNoSetting[testSource],
          testElement.$.permissionItem.innerText.trim());
      assertEquals(
          permissionSourcesNoSetting[testSource] != '',
          testElement.$.permissionItem.classList.contains('two-line'));

      if (testSource != settings.SiteSettingSource.DEFAULT &&
          testSource != settings.SiteSettingSource.PREFERENCE &&
          testSource != settings.SiteSettingSource.EMBARGO) {
        assertTrue(testElement.$.permission.disabled);
      } else {
        assertFalse(testElement.$.permission.disabled);
      }
    }

    // Permissions that have been set by extensions.
    var extensionSourceStrings = {};
    extensionSourceStrings[settings.ContentSetting.ALLOW] =
        'Allowed by an extension';
    extensionSourceStrings[settings.ContentSetting.BLOCK] =
        'Blocked by an extension';
    extensionSourceStrings[settings.ContentSetting.ASK] =
        'Setting controlled by an extension';

    for (testSetting in extensionSourceStrings) {
      testElement.site = {
        origin: origin,
        embeddingOrigin: origin,
        setting: testSetting,
        source: settings.SiteSettingSource.EXTENSION,
      };
      assertEquals(
          extensionSourceStrings[testSetting],
          testElement.$.permissionItem.innerText.trim());
      assertTrue(testElement.$.permissionItem.classList.contains('two-line'));
      assertTrue(testElement.$.permission.disabled);
      assertEquals(testSetting, testElement.$.permission.value);
    };

    // Permissions that have been set by enterprise policy.
    var policySourceStrings = {};
    policySourceStrings[settings.ContentSetting.ALLOW] =
        'Allowed by your administrator';
    policySourceStrings[settings.ContentSetting.BLOCK] =
        'Blocked by your administrator';
    policySourceStrings[settings.ContentSetting.ASK] =
        'Setting controlled by your administrator';

    for (testSetting in policySourceStrings) {
      testElement.site = {
        origin: origin,
        embeddingOrigin: origin,
        setting: testSetting,
        source: settings.SiteSettingSource.POLICY,
      };
      assertEquals(
          policySourceStrings[testSetting],
          testElement.$.permissionItem.innerText.trim());
      assertTrue(testElement.$.permissionItem.classList.contains('two-line'));
      assertTrue(testElement.$.permission.disabled);
      assertEquals(testSetting, testElement.$.permission.value);
    };

    // Finally, check if changing the source from a non-user-controlled setting
    // (policy) back to a user-controlled one re-enables the control.
    testElement.site = {
      origin: origin,
      embeddingOrigin: origin,
      setting: settings.ContentSetting.ASK,
      source: settings.SiteSettingSource.DEFAULT,
    };
    assertEquals('', testElement.$.permissionItem.innerText.trim());
    assertFalse(testElement.$.permissionItem.classList.contains('two-line'));
    assertFalse(testElement.$.permission.disabled);
  });

  test('source string correct for drm disabled source', function() {
    var origin = 'https://www.example.com';
    testElement.category = settings.ContentSettingsTypes.PROTECTED_CONTENT;
    testElement.site = {
      origin: origin,
      embeddingOrigin: origin,
      setting: settings.ContentSetting.BLOCK,
      source: settings.SiteSettingSource.DRM_DISABLED,
    };
    assertEquals(
        'To change this setting, first turn on identifiers',
        testElement.$.permissionItem.innerText.trim());
    assertTrue(testElement.$.permissionItem.classList.contains('two-line'));
    assertTrue(testElement.$.permission.disabled);
  });
});

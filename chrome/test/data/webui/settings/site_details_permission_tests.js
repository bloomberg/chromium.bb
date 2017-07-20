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
            source: 'preference',
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

  // Tests that the given value is converted to the expected value, for a
  // given prefType.
  function isAllowed(origin, exceptionList) {
    for (var i = 0; i < exceptionList.length; ++i) {
      if (exceptionList[i].origin == origin)
        return exceptionList[i].setting == settings.ContentSetting.ALLOW;
    }
    return false;
  };

  function validatePermissionFlipWorks(origin, expectedContentSetting) {
    // Flipping a permission typically calls setCategoryPermissionForOrigin, but
    // clearing it should call resetCategoryPermissionForOrigin.
    var isReset = expectedContentSetting == settings.ContentSetting.DEFAULT;
    var expectedMethodCalled = isReset ? 'resetCategoryPermissionForOrigin' :
                                         'setCategoryPermissionForOrigin';
    browserProxy.resetResolver(expectedMethodCalled);

    // Simulate permission change initiated by the user.
    testElement.$.permission.value = expectedContentSetting;
    testElement.$.permission.dispatchEvent(new CustomEvent('change'));

    return browserProxy.whenCalled(expectedMethodCalled).then(function(args) {
      assertEquals(origin, args[0]);
      assertEquals('', args[1]);
      assertEquals(testElement.category, args[2]);
      // Since resetting the permission doesn't return its new value, don't
      // test it here - checking that the correct method was called is fine.
      if (!isReset)
        assertEquals(expectedContentSetting, args[3]);
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
    };

    assertFalse(testElement.$.details.hidden);

    var header = testElement.$.details.querySelector('#permissionHeader');
    assertEquals(
        'Camera', header.innerText.trim(),
        'Widget should be labelled correctly');

    // Flip the permission and validate that prefs stay in sync.
    return validatePermissionFlipWorks(origin, settings.ContentSetting.ALLOW)
        .then(function() {
          return validatePermissionFlipWorks(
              origin, settings.ContentSetting.BLOCK);
        })
        .then(function() {
          return validatePermissionFlipWorks(
              origin, settings.ContentSetting.ALLOW);
        })
        .then(function() {
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
    };

    return browserProxy.whenCalled('getDefaultValueForContentType')
        .then(function() {
          // The default option will always be the first in the menu.
          assertEquals(
              'Allow (default)', testElement.$.permission.options[0].text,
              'Default setting string should match prefs');
          browserProxy.resetResolver('getDefaultValueForContentType');
          prefs.defaults.camera.setting = settings.ContentSetting.BLOCK;
          browserProxy.setPrefs(prefs);
          // Trigger a call to siteChanged_() by touching |testElement.site|.
          testElement.site = {
            origin: origin,
            embeddingOrigin: '',
            setting: settings.ContentSetting.BLOCK
          };
          return browserProxy.whenCalled('getDefaultValueForContentType');
        })
        .then(function() {
          assertEquals(
              'Block (default)', testElement.$.permission.options[0].text,
              'Default setting string should match prefs');
          browserProxy.resetResolver('getDefaultValueForContentType');
          prefs.defaults.camera.setting = settings.ContentSetting.ASK;
          browserProxy.setPrefs(prefs);
          // Trigger a call to siteChanged_() by touching |testElement.site|.
          testElement.site = {
            origin: origin,
            embeddingOrigin: '',
            setting: settings.ContentSetting.ASK
          };
          return browserProxy.whenCalled('getDefaultValueForContentType');
        })
        .then(function() {
          assertEquals(
              'Ask (default)', testElement.$.permission.options[0].text,
              'Default setting string should match prefs');
        });
  });
});

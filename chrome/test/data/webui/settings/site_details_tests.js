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
   * An example pref with 1 allowed in each category.
   */
  var prefs = {
    exceptions: {
      auto_downloads: [
        {
          embeddingOrigin: 'https://foo-allow.com:443',
          origin: 'https://foo-allow.com:443',
          setting: 'allow',
          source: 'preference',
        },
      ],
      background_sync: [
        {
          embeddingOrigin: 'https://foo-allow.com:443',
          origin: 'https://foo-allow.com:443',
          setting: 'allow',
          source: 'preference',
        },
      ],
      camera: [
        {
          embeddingOrigin: 'https://foo-allow.com:443',
          origin: 'https://foo-allow.com:443',
          setting: 'allow',
          source: 'extension',
        },
      ],
      geolocation: [
        {
          embeddingOrigin: 'https://foo-allow.com:443',
          origin: 'https://foo-allow.com:443',
          setting: 'block',
          source: 'policy',
        },
      ],
      images: [
        {
          embeddingOrigin: 'https://foo-allow.com:443',
          origin: 'https://foo-allow.com:443',
          setting: 'allow',
          source: 'preference',
        },
      ],
      javascript: [
        {
          embeddingOrigin: 'https://foo-allow.com:443',
          origin: 'https://foo-allow.com:443',
          setting: 'allow',
          source: 'preference',
        },
      ],
      mic: [
        {
          embeddingOrigin: 'https://foo-allow.com:443',
          origin: 'https://foo-allow.com:443',
          setting: 'allow',
          source: 'preference',
        },
      ],
      notifications: [
        {
          embeddingOrigin: 'https://foo-allow.com:443',
          origin: 'https://foo-allow.com:443',
          setting: 'allow',
          source: 'preference',
        },
      ],
      plugins: [
        {
          embeddingOrigin: 'https://foo-allow.com:443',
          origin: 'https://foo-allow.com:443',
          setting: 'allow',
          source: 'preference',
        },
      ],
      popups: [
        {
          embeddingOrigin: 'https://foo-allow.com:443',
          origin: 'https://foo-allow.com:443',
          setting: 'allow',
          source: 'preference',
        },
      ],
      unsandboxed_plugins: [
        {
          embeddingOrigin: 'https://foo-allow.com:443',
          origin: 'https://foo-allow.com:443',
          setting: 'allow',
          source: 'preference',
        },
      ],
    }
  };

  // Initialize a site-details before each test.
  setup(function() {
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
    testElement.origin = 'https://foo-allow.com:443';

    Polymer.dom.flush();

    // Expect usage to be rendered.
    assertTrue(!!testElement.$$('#usage'));
  });

  test('correct pref settings are shown', function() {
    browserProxy.setPrefs(prefs);
    testElement.origin = 'https://foo-allow.com:443';

    return browserProxy.whenCalled('getOriginPermissions').then(function() {
      testElement.root.querySelectorAll('site-details-permission')
          .forEach(function(siteDetailsPermission) {
            // Verify settings match the values specified in |prefs|.
            var setting = 'allow';
            if (siteDetailsPermission.site.category == 'location')
              setting = 'block';
            assertEquals(setting, siteDetailsPermission.site.setting);
            assertEquals(setting, siteDetailsPermission.$.permission.value);
          });
    });
  });
});

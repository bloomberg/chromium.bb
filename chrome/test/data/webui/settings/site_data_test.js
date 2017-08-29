// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('<site-data>', function() {
  /** @type {SiteDataElement} */
  var siteData;

  /** @type {TestSiteSettingsPrefsBrowserProxy} */
  var testBrowserProxy;

  setup(function() {
    testBrowserProxy = new TestSiteSettingsPrefsBrowserProxy;
    settings.SiteSettingsPrefsBrowserProxyImpl.instance_ = testBrowserProxy;
    siteData = document.createElement('site-data');
  });

  test('tapping remove button (trash can) calls remove on origin', function() {
    var GOOGLE_ID = '1';
    siteData.sites = [{site: 'Google', id: GOOGLE_ID, localData: 'Cookiez!'}];
    Polymer.dom.flush();

    MockInteractions.tap(siteData.$$('.icon-delete-gray'));

    return testBrowserProxy.whenCalled('removeCookie').then(function(path) {
      assertEquals(GOOGLE_ID, path);
    });
  });

  test('remove button hidden when no search results', function() {
    siteData.sites = [
      {site: 'Hello', id: '1', localData: 'Cookiez!'},
      {site: 'World', id: '2', localData: 'Cookiez!'},
    ];

    Polymer.dom.flush();
    assertEquals(
        siteData.sites.length,
        siteData.shadowRoot.querySelectorAll('#siteItem').length);

    // Expecting one result, so the button should be shown.
    siteData.filter = 'Hello';
    Polymer.dom.flush();
    assertEquals(1, siteData.shadowRoot.querySelectorAll('#siteItem').length);
    assertFalse(siteData.$.removeShowingSites.hidden);

    // Expecting no results, so the button should be hidden.
    siteData.filter = 'foo';
    Polymer.dom.flush();
    assertEquals(0, siteData.shadowRoot.querySelectorAll('#siteItem').length);
    assertTrue(siteData.$.removeShowingSites.hidden);
  });
});

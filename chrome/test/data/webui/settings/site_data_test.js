// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('<site-data>', function() {
  /** @type {SiteDataElement} */
  var siteData;

  /** @type {TestLocalDataBrowserProxy} */
  var testBrowserProxy;

  setup(function() {
    testBrowserProxy = new TestLocalDataBrowserProxy;
    settings.LocalDataBrowserProxyImpl.instance_ = testBrowserProxy;
    siteData = document.createElement('site-data');
  });

  teardown(function() {
    siteData.remove();
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

  test('calls reloadCookies() when created', function() {
    settings.navigateTo(settings.routes.SITE_SETTINGS_SITE_DATA);
    document.body.appendChild(siteData);
    return testBrowserProxy.whenCalled('reloadCookies');
  });

  test('calls reloadCookies() when visited again', function() {
    settings.navigateTo(settings.routes.SITE_SETTINGS_SITE_DATA);
    document.body.appendChild(siteData);
    settings.navigateTo(settings.routes.SITE_SETTINGS_COOKIES);
    testBrowserProxy.reset();
    settings.navigateTo(settings.routes.SITE_SETTINGS_SITE_DATA);
    return testBrowserProxy.whenCalled('reloadCookies');
  });
});

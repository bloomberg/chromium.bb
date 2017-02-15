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

    MockInteractions.tap(siteData.$$('.remove-site'));

    return testBrowserProxy.whenCalled('removeCookie').then(function(path) {
      assertEquals(GOOGLE_ID, path);
    });
  });
});

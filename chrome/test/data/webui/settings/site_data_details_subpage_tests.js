// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for site-data-details-subpage. */
suite('SiteDataDetailsSubpage', function() {
  /** @type {?SiteDataDetailsSubpageElement} */
  var page = null;

  /** @type {TestSiteSettingsPrefsBrowserProxy} */
  var browserProxy = null;
  /** @type {!CookieDetails} */
  var cookieDetails = {
    accessibleToScript: "Yes",
    content: "dummy_cookie_contents",
    created: "Tuesday, February 7, 2017 at 11:28:45 AM",
    domain: ".foo.com",
    expires: "Wednesday, February 7, 2018 at 11:28:45 AM",
    hasChildren: false,
    id: "328",
    idPath: "74,165,328",
    name: "abcd",
    path: "/",
    sendfor: "Any kind of connection",
    title: "abcd",
    type: "cookie"
  };

  /** @type {!CookieList} */
  var cookieList = {
    id: 'fooId',
    children: [cookieDetails],
  };

  var site = 'foo.com';

  setup(function() {
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    browserProxy.setCookieDetails(cookieList);
    settings.SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    page = document.createElement('site-data-details-subpage');
    settings.navigateTo(
        settings.Route.SITE_SETTINGS_DATA_DETAILS,
        new URLSearchParams('site=' + site));

    document.body.appendChild(page);
  });

  teardown(function() {
    settings.resetRouteForTesting();
  });

  test('DetailsShownForCookie', function() {
    return browserProxy.whenCalled('getCookieDetails').then(
        function(actualSite) {
          assertEquals(site, actualSite);

          Polymer.dom.flush();
          var entries = page.root.querySelectorAll('.settings-box');
          assertEquals(1, entries.length);

          var listItems = page.root.querySelectorAll('.list-item');
          // |cookieInfo| is a global var defined in
          // site_settings/cookie_info.js, and specifies the fields that are
          // shown for a cookie.
          assertEquals(cookieInfo.cookie.length, listItems.length);

          // Check that all the cookie information is presented in the DOM.
          var cookieDetailValues = page.root.querySelectorAll('.secondary');
          cookieDetailValues.forEach(function(div, i) {
            var key = cookieInfo.cookie[i][0];
            assertEquals(cookieDetails[key], div.textContent);
          });
        });
  });
});

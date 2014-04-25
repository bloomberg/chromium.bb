// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestFixture for cookies view WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function CookiesViewWebUITest() {}

CookiesViewWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the cookies view.
   */
  browsePreload: 'chrome://settings-frame/cookies',
};

// Test opening the cookies view has correct location.
TEST_F('CookiesViewWebUITest', 'DISABLED_testOpenCookiesView', function() {
  assertEquals(this.browsePreload, document.location.href);
});

TEST_F('CookiesViewWebUITest', 'testNoCloseOnSearchEnter', function() {
  var cookiesView = CookiesView.getInstance();
  assertTrue(cookiesView.visible);
  var searchBox = cookiesView.pageDiv.querySelector('.cookies-search-box');
  searchBox.dispatchEvent(new KeyboardEvent('keydown', {
    'bubbles': true,
    'cancelable': true,
    'keyIdentifier': 'Enter'
  }));
  assertTrue(cookiesView.visible);
});

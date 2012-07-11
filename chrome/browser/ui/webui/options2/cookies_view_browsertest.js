// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestFixture for cookies view WebUI testing.
 * @extends {testing.Test}
 * @constructor
 **/
function CookiesViewWebUITest() {}

CookiesViewWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the cookies view.
   **/
  browsePreload: 'chrome://settings-frame/cookies',
};

// Test opening the cookies view has correct location.
TEST_F('CookiesViewWebUITest', 'testOpenCookiesView', function() {
  assertEquals(this.browsePreload, document.location.href);
});

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestFixture for browser options WebUI testing.
 * @extends {testing.Test}
 * @constructor
 **/
function BrowserOptionsWebUITest() {}

BrowserOptionsWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to browser options.
   **/
  browsePreload: 'chrome://settings/browser',
};

// Test opening the browser options has correct location.
TEST_F('BrowserOptionsWebUITest', 'testOpenBrowserOptions', function() {
  assertEquals(this.browsePreload, document.location.href);
});

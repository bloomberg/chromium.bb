// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestFixture for website settings WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function WebsiteSettingsWebUITest() {}

WebsiteSettingsWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the website settings.
   */
  browsePreload: 'chrome://settings-frame/websiteSettings',
};

// Test opening the website settings manager has the correct location.
TEST_F('WebsiteSettingsWebUITest', 'testOpenWebsiteSettings', function() {
  assertEquals(this.browsePreload, document.location.href);
});

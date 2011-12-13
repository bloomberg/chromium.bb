// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestFixture for font settings WebUI testing.
 * @extends {testing.Test}
 * @constructor
 **/
function FontSettingsWebUITest() {}

FontSettingsWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the font settings page.
   **/
  browsePreload: 'chrome://settings/fonts',
};

// Test opening font settings has correct location.
TEST_F('FontSettingsWebUITest', 'testOpenFontSettings', function() {
  assertEquals(this.browsePreload, document.location.href);
});

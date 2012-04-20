// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestFixture for extension settings WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function ExtensionSettingsWebUITest() {}

ExtensionSettingsWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  browsePreload: 'chrome://extensions-frame/',
};

// Test opening extension settings has correct location.
TEST_F('ExtensionSettingsWebUITest', 'testOpenExtensionSettings', function() {
  assertEquals(this.browsePreload, document.location.href);
});

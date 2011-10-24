// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestFixture for advanced options WebUI testing.
 * @extends {testing.Test}
 * @constructor
 **/
function AdvancedOptionsWebUITest() {}

AdvancedOptionsWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to advanced options.
   **/
  browsePreload: 'chrome://settings/advanced',
};

// Test opening the advanced options has correct location.
TEST_F('AdvancedOptionsWebUITest', 'testOpenAdvancedOptions', function() {
  assertEquals(this.browsePreload, document.location.href);
});

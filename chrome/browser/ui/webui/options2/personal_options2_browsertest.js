// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestFixture for personal options WebUI testing.
 * @extends {testing.Test}
 * @constructor
 **/
function PersonalOptionsWebUITest() {}

PersonalOptionsWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to personal options.
   **/
  browsePreload: 'chrome://settings/personal',
};

// Test opening personal options has correct location.
TEST_F('PersonalOptionsWebUITest', 'testOpenPersonalOptions', function() {
  assertEquals(this.browsePreload, document.location.href);
});

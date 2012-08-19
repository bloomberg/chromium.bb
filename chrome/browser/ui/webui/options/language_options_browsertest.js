// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestFixture for languages options WebUI testing.
 * @extends {testing.Test}
 * @constructor
 **/
function LanguagesOptionsWebUITest() {}

LanguagesOptionsWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to languages options.
   **/
  browsePreload: 'chrome://settings-frame/languages',
};

// Test opening languages options has correct location.
TEST_F('LanguagesOptionsWebUITest', 'testOpenLanguagesOptions', function() {
  assertEquals(this.browsePreload, document.location.href);
});

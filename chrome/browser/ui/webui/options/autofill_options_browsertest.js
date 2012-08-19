// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestFixture for autofill options WebUI testing.
 * @extends {testing.Test}
 * @constructor
 **/
function AutofillOptionsWebUITest() {}

AutofillOptionsWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to autofill options.
   **/
  browsePreload: 'chrome://settings-frame/autofill',
};

// Test opening the autofill options has correct location.
TEST_F('AutofillOptionsWebUITest', 'testOpenAutofillOptions', function() {
  assertEquals(this.browsePreload, document.location.href);
});

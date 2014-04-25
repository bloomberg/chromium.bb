// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestFixture for password manager WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function PasswordManagerWebUITest() {}

PasswordManagerWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the password manager.
   */
  browsePreload: 'chrome://settings-frame/passwords',
};

// Test opening the password manager has correct location.
TEST_F('PasswordManagerWebUITest', 'testOpenPasswordManager',
       function() {
         assertEquals(this.browsePreload, document.location.href);
       });

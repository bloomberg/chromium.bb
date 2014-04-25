// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestFixture for profile settings reset WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function ProfileSettingsResetWebUITest() {}

ProfileSettingsResetWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the reset profile settings page.
   */
  browsePreload: 'chrome://settings-frame/resetProfileSettings',
};

// Test opening the profile settings reset has correct location.
TEST_F('ProfileSettingsResetWebUITest', 'testOpenProfileSettingsReset',
       function() {
         assertEquals(this.browsePreload, document.location.href);
       });

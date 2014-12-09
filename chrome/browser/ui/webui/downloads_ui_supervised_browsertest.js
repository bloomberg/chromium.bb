// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['downloads_ui_browsertest_base.js']);
GEN('#include "chrome/browser/ui/webui/downloads_ui_supervised_browsertest.h"');

/**
 * Fixture for Downloads WebUI testing for a supervised user.
 * @extends {BaseDownloadsWebUITest}
 * @constructor
 */
function DownloadsWebUIForSupervisedUsersTest() {}

DownloadsWebUIForSupervisedUsersTest.prototype = {
  __proto__: BaseDownloadsWebUITest.prototype,

  /** @override */
  typedefCppFixture: 'DownloadsWebUIForSupervisedUsersTest',
};

// Test UI for supervised users, removing entries should be disabled
// and removal controls should be hidden.
TEST_F('DownloadsWebUIForSupervisedUsersTest', 'SupervisedUsers', function() {
  this.expectDeleteControlsVisible(false);
  testDone();
});

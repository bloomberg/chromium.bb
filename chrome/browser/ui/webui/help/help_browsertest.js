// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestFixture for extension settings WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function HelpPageWebUITest() {}

HelpPageWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  browsePreload: 'chrome://help-frame/',
};

// Test opening extension settings has correct location.
TEST_F('HelpPageWebUITest', 'testOpenHelpPage', function() {
  assertEquals(this.browsePreload, document.location.href);
});

GEN('#if defined(OS_LINUX) || defined(GOOGLE_CHROME_BUILD)');

// Test that repeated calls to setUpdateStatus work.
TEST_F('HelpPageWebUITest', 'testUpdateState', function() {
  help.HelpPage.setUpdateStatus('disabled', '');
  expectTrue($('relaunch').hidden);
  expectTrue($('update-status-container').hidden);

  help.HelpPage.setUpdateStatus('nearly_updated', '');
  expectTrue(!$('relaunch').hidden);
  expectTrue(!$('update-status-container').hidden);

  help.HelpPage.setUpdateStatus('disabled', '');
  expectTrue($('relaunch').hidden);
  expectTrue($('update-status-container').hidden);
});

GEN('#endif');

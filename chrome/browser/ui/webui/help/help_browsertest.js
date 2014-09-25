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

  /** @override */
  runAccessibilityChecks: true,

  /** @override */
  accessibilityIssuesAreErrors: true,

  browsePreload: 'chrome://help-frame/',
};

// Test opening extension settings has correct location.
TEST_F('HelpPageWebUITest', 'testOpenHelpPage', function() {
  assertEquals(this.browsePreload, document.location.href);
});

GEN('#if defined(OS_LINUX) || defined(GOOGLE_CHROME_BUILD)');

// Test that repeated calls to setUpdateStatus work.
TEST_F('HelpPageWebUITest', 'testUpdateState', function() {
  var relaunch = $('relaunch');
  var container = $('update-status-container');
  var update = $('request-update');

  help.HelpPage.setUpdateStatus('updated', '');
  expectTrue(relaunch.hidden);
  expectTrue(cr.isChromeOS == container.hidden);
  expectTrue(!cr.isChromeOS || !update.hidden && !update.disabled);

  help.HelpPage.setUpdateStatus('disabled', '');
  expectTrue(relaunch.hidden);
  expectTrue(container.hidden);
  expectTrue(!cr.isChromeOS || update.hidden);

  help.HelpPage.setUpdateStatus('nearly_updated', '');
  expectTrue(!relaunch.hidden);
  expectTrue(!container.hidden);
  expectTrue(!cr.isChromeOS || update.hidden);

  help.HelpPage.setUpdateStatus('disabled', '');
  expectTrue($('relaunch').hidden);
  expectTrue($('update-status-container').hidden);
  expectTrue(!cr.isChromeOS || update.hidden);
});

GEN('#endif');

GEN('#if defined(OS_CHROMEOS)');

// Test that the request update button is shown and hidden properly.
TEST_F('HelpPageWebUITest', 'testRequestUpdate', function() {
  var container = $('update-status-container');
  var update = $('request-update');

  help.HelpPage.setUpdateStatus('updated', '');
  expectTrue(container.hidden);
  expectTrue(!update.hidden && !update.disabled);

  update.click();
  expectTrue(!update.hidden && update.disabled);
  expectFalse(container.hidden);

  help.HelpPage.setUpdateStatus('checking', '');
  expectFalse(container.hidden);
  expectTrue(!update.hidden && update.disabled);

  help.HelpPage.setUpdateStatus('failed', 'Error');
  expectFalse(container.hidden);
  expectTrue(!update.hidden && !update.disabled);

  update.click();
  help.HelpPage.setUpdateStatus('checking', '');
  expectFalse(container.hidden);
  expectTrue(!update.hidden && update.disabled);

  help.HelpPage.setUpdateStatus('nearly_updated', '');
  expectFalse(container.hidden);
  expectTrue(update.hidden);

  help.HelpPage.setUpdateStatus('updated', '');
  expectFalse(container.hidden);
  expectTrue(!update.hidden && update.disabled);
});

GEN('#endif');

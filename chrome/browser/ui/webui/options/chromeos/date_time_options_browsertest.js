// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['chrome/browser/ui/webui/options/options_browsertest_base.js']);

GEN('#if defined(OS_CHROMEOS)');

/**
 * DateTimeOptionsWebUITest tests the date and time section of the options page.
 * @constructor
 * @extends {testing.Test}
 */
function DateTimeOptionsWebUITest() {}

DateTimeOptionsWebUITest.prototype = {
  __proto__: OptionsBrowsertestBase.prototype,

  /**
   * Browse to date/time options.
   * @override
   */
  browsePreload: 'chrome://settings-frame/search#date',
};

TEST_F('DateTimeOptionsWebUITest', 'testShowSetTimeButton', function() {
  assertEquals(this.browsePreload, document.location.href);

  // Show button.
  BrowserOptions.setCanSetTime(true);
  expectFalse($('set-time').hidden);

  // Hide button.
  BrowserOptions.setCanSetTime(false);
  expectTrue($('set-time').hidden);
});

GEN('#endif');

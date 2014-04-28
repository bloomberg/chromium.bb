// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN('#if defined(OS_CHROMEOS)');

/**
 * DateTimeOptionsWebUITest tests the date and time section of the options page.
 * @constructor
 * @extends {testing.Test}
 */
function DateTimeOptionsWebUITest() {}

DateTimeOptionsWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to date/time options.
   * @override
   */
  browsePreload: 'chrome://settings-frame/search#date',
};

TEST_F('DateTimeOptionsWebUITest', 'testShowSetTimeButton', function() {
  assertEquals(this.browsePreload, document.location.href);

  // Hide label and show button.
  BrowserOptions.setCanSetTime(true);
  expectTrue($('time-synced-explanation').hidden);
  expectFalse($('set-time').hidden);

  // Show label and hide button.
  BrowserOptions.setCanSetTime(false);
  expectFalse($('time-synced-explanation').hidden);
  expectTrue($('set-time').hidden);
});

GEN('#endif');

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestFixture for OptionsPage WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function OptionsWebUITest() {}

OptionsWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the options page & call our preLoad().
   */
  browsePreload: 'chrome://settings',

  /**
   * Register a mock handler to ensure expectations are met and options pages
   * behave correctly.
   */
  preLoad: function() {
    this.makeAndRegisterMockHandler(
        ['coreOptionsInitialize',
         'fetchPrefs',
         'observePrefs',
         'setBooleanPref',
         'setIntegerPref',
         'setDoublePref',
         'setStringPref',
         'setObjectPref',
         'clearPref',
         'coreOptionsUserMetricsAction',
         // TODO(scr): Handle this new message:
         // getInstantFieldTrialStatus: function() {},
        ]);

    // Register stubs for methods expected to be called before our tests run.
    // Specific expectations can be made in the tests themselves.
    this.mockHandler.stubs().fetchPrefs(ANYTHING);
    this.mockHandler.stubs().observePrefs(ANYTHING);
    this.mockHandler.stubs().coreOptionsInitialize();
  },
};

// Crashes on Mac only. See http://crbug.com/79181
GEN('#if defined(OS_MACOSX)');
GEN('#define MAYBE_testSetBooleanPrefTriggers ' +
    'DISABLED_testSetBooleanPrefTriggers');
GEN('#else');
GEN('#define MAYBE_testSetBooleanPrefTriggers testSetBooleanPrefTriggers');
GEN('#endif  // defined(OS_MACOSX)');

TEST_F('OptionsWebUITest', 'MAYBE_testSetBooleanPrefTriggers', function() {
  // TODO(dtseng): make generic to click all buttons.
  var showHomeButton = $('toolbarShowHomeButton');
  var trueListValue = [
    'browser.show_home_button',
    true,
    'Options_Homepage_HomeButton',
  ];
  // Note: this expectation is checked in testing::Test::tearDown.
  this.mockHandler.expects(once()).setBooleanPref(trueListValue);

  // Cause the handler to be called.
  showHomeButton.click();
  showHomeButton.blur();
});

// Not meant to run on ChromeOS at this time.
// Not finishing in windows. http://crbug.com/81723
GEN('#if defined(OS_CHROMEOS) || defined(OS_MACOSX) || defined(OS_WIN)');
GEN('#define MAYBE_testRefreshStaysOnCurrentPage \\');
GEN('    DISABLED_testRefreshStaysOnCurrentPage');
GEN('#else');
GEN('#define MAYBE_testRefreshStaysOnCurrentPage ' +
    'testRefreshStaysOnCurrentPage');
GEN('#endif');

TEST_F('OptionsWebUITest', 'MAYBE_testRefreshStaysOnCurrentPage', function() {
  var item = $('advancedPageNav');
  item.onclick();
  window.location.reload();
  var pageInstance = AdvancedOptions.getInstance();
  var topPage = OptionsPage.getTopmostVisiblePage();
  var expectedTitle = pageInstance.title;
  var actualTitle = document.title;
  assertEquals("chrome://settings/advanced", document.location.href);
  assertEquals(expectedTitle, actualTitle);
  assertEquals(pageInstance, topPage);
});

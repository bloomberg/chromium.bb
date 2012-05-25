// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
  browsePreload: 'chrome://settings-frame',

  /**
   * Register a mock handler to ensure expectations are met and options pages
   * behave correctly.
   */
  preLoad: function() {
    this.makeAndRegisterMockHandler(
        ['defaultZoomFactorAction',
         'fetchPrefs',
         'observePrefs',
         'setBooleanPref',
         'setIntegerPref',
         'setDoublePref',
         'setStringPref',
         'setObjectPref',
         'clearPref',
         'coreOptionsUserMetricsAction',
        ]);

    // Register stubs for methods expected to be called before/during tests.
    // Specific expectations can be made in the tests themselves.
    this.mockHandler.stubs().fetchPrefs(ANYTHING);
    this.mockHandler.stubs().observePrefs(ANYTHING);
    this.mockHandler.stubs().coreOptionsUserMetricsAction(ANYTHING);
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
  var showHomeButton = $('show-home-button');
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
TEST_F('OptionsWebUITest', 'DISABLED_testRefreshStaysOnCurrentPage',
    function() {
  assertTrue($('search-engine-manager-page').hidden);
  var item = $('manage-default-search-engines');
  item.click();

  assertFalse($('search-engine-manager-page').hidden);

  window.location.reload();

  assertEquals('chrome://settings-frame/searchEngines', document.location.href);
  assertFalse($('search-engine-manager-page').hidden);
});

/**
 * Test the default zoom factor select element.
 */
TEST_F('OptionsWebUITest', 'testDefaultZoomFactor', function() {
  // The expected minimum length of the |defaultZoomFactor| element.
  var defaultZoomFactorMinimumLength = 10;
  // Verify that the zoom factor element exists.
  var defaultZoomFactor = $('defaultZoomFactor');
  assertNotEquals(defaultZoomFactor, null);

  // Verify that the zoom factor element has a reasonable number of choices.
  expectGE(defaultZoomFactor.options.length, defaultZoomFactorMinimumLength);

  // Simulate a change event, selecting the highest zoom value.  Verify that
  // the javascript handler was invoked once.
  this.mockHandler.expects(once()).defaultZoomFactorAction(NOT_NULL).
      will(callFunction(function() { }));
  defaultZoomFactor.selectedIndex = defaultZoomFactor.options.length - 1;
  var event = { target: defaultZoomFactor };
  if (defaultZoomFactor.onchange) defaultZoomFactor.onchange(event);
});

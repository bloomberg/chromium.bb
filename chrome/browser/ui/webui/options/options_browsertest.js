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

  isAsync: true,

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
  testDone();
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
  testDone();
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
  testDone();
});

// If |confirmInterstitial| is true, the OK button of the Do Not Track
// interstitial is pressed, otherwise the abort button is pressed.
OptionsWebUITest.prototype.testDoNotTrackInterstitial =
    function(confirmInterstitial) {
  Preferences.prefsFetchedCallback({'enable_do_not_track': {'value': false } });
  var buttonToClick = confirmInterstitial ? $('do-not-track-confirm-ok')
                                          : $('do-not-track-confirm-cancel');
  var dntCheckbox = $('do-not-track-enabled');
  var dntOverlay = OptionsPage.registeredOverlayPages['donottrackconfirm'];
  assertFalse(dntCheckbox.checked);

  var visibleChangeCounter = 0;
  var visibleChangeHandler = function() {
    ++visibleChangeCounter;
    switch (visibleChangeCounter) {
      case 1:
        window.setTimeout(function() {
          assertTrue(dntOverlay.visible);
          buttonToClick.click();
        }, 0);
        break;
      case 2:
        window.setTimeout(function() {
          assertFalse(dntOverlay.visible);
          assertEquals(confirmInterstitial, dntCheckbox.checked);
          dntOverlay.removeEventListener(visibleChangeHandler);
          testDone();
        }, 0);
        break;
      default:
        assertTrue(false);
    }
  }
  dntOverlay.addEventListener('visibleChange', visibleChangeHandler);

  if (confirmInterstitial) {
    this.mockHandler.expects(once()).setBooleanPref(
        ['enable_do_not_track', true, 'Options_DoNotTrackCheckbox']);
  } else {
    // The mock handler complains if setBooleanPref is called even though
    // it should not be.
  }

  dntCheckbox.click();
}

TEST_F('OptionsWebUITest', 'EnableDoNotTrackAndConfirmInterstitial',
       function() {
  this.testDoNotTrackInterstitial(true);
});

TEST_F('OptionsWebUITest', 'EnableDoNotTrackAndCancelInterstitial',
       function() {
  this.testDoNotTrackInterstitial(false);
});

// Check that the "Do not Track" preference can be correctly disabled.
// In order to do that, we need to enable it first.
TEST_F('OptionsWebUITest', 'EnableAndDisableDoNotTrack', function() {
  Preferences.prefsFetchedCallback({'enable_do_not_track': {'value': false } });
  var dntCheckbox = $('do-not-track-enabled');
  var dntOverlay = OptionsPage.registeredOverlayPages['donottrackconfirm'];
  assertFalse(dntCheckbox.checked);

  var visibleChangeCounter = 0;
  var visibleChangeHandler = function() {
    ++visibleChangeCounter;
    switch (visibleChangeCounter) {
      case 1:
        window.setTimeout(function() {
          assertTrue(dntOverlay.visible);
          $('do-not-track-confirm-ok').click();
        }, 0);
        break;
      case 2:
        window.setTimeout(function() {
          assertFalse(dntOverlay.visible);
          assertTrue(dntCheckbox.checked);
          dntOverlay.removeEventListener(visibleChangeHandler);
          dntCheckbox.click();
        }, 0);
        break;
      default:
        assertNotReached();
    }
  }
  dntOverlay.addEventListener('visibleChange', visibleChangeHandler);

  this.mockHandler.expects(once()).setBooleanPref(
      eq(["enable_do_not_track", true, 'Options_DoNotTrackCheckbox']));

  var verifyCorrectEndState = function() {
    window.setTimeout(function() {
      assertFalse(dntOverlay.visible);
      assertFalse(dntCheckbox.checked);
      testDone();
    }, 0)
  }
  this.mockHandler.expects(once()).setBooleanPref(
      eq(["enable_do_not_track", false, 'Options_DoNotTrackCheckbox'])).will(
          callFunction(verifyCorrectEndState));

  dntCheckbox.click();
});

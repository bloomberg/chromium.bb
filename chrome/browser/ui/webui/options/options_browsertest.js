// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['options_browsertest_base.js']);
GEN('#include "chrome/browser/ui/webui/options/options_browsertest.h"');

/** @const */ var SUPERVISED_USERS_PREF = 'profile.managed_users';

/**
 * Wait for the method specified by |methodName|, on the |object| object, to be
 * called, then execute |afterFunction|.
 * @param {*} object Object with callable property named |methodName|.
 * @param {string} methodName The name of the property on |object| to use as a
 *     callback.
 * @param {!Function} afterFunction A function to call after object.methodName()
 *     is called.
 */
function waitForResponse(object, methodName, afterFunction) {
  var originalCallback = object[methodName];

  // Install a wrapper that temporarily replaces the original function.
  object[methodName] = function() {
    object[methodName] = originalCallback;
    originalCallback.apply(this, arguments);
    afterFunction();
  };
}

/**
  * Wait for the global window.onpopstate callback to be called (after a tab
  * history navigation), then execute |afterFunction|.
  * @param {!Function} afterFunction A function to call after pop state events.
  */
function waitForPopstate(afterFunction) {
  waitForResponse(window, 'onpopstate', afterFunction);
}

/**
 * TestFixture for OptionsPage WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function OptionsWebUITest() {}

OptionsWebUITest.prototype = {
  __proto__: OptionsBrowsertestBase.prototype,

  /**
   * Browse to the options page & call our preLoad().
   * @override
   */
  browsePreload: 'chrome://settings-frame',

  /** @override */
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

  /** @override */
  setUp: function() {
    OptionsBrowsertestBase.prototype.setUp.call(this);

    // Enable when failure is resolved.
    // AX_ARIA_10: http://crbug.com/559329
    this.accessibilityAuditConfig.ignoreSelectors(
        'unsupportedAriaAttribute',
        '#profiles-list');

    var linkWithUnclearPurposeSelectors = [
      '#sync-overview > A',
      '#privacy-explanation > A',
      '#languages-section > .settings-row > A',
      '#cloudprint-options-mdns > .settings-row > A',
      '#do-not-track-confirm-overlay > .action-area > .hbox.stretch > A',
    ];

    // Enable when failure is resolved.
    // AX_TEXT_04: http://crbug.com/559318
    this.accessibilityAuditConfig.ignoreSelectors(
        'linkWithUnclearPurpose',
        linkWithUnclearPurposeSelectors);
  },
};

/**
 * Wait for all targets to be hidden.
 * @param {Array<Element>} targets
 */
function waitUntilHidden(targets) {
  function isHidden(el) { return el.hidden; }
  function ensureTransition(el) { ensureTransitionEndEvent(el, 500); }

  document.addEventListener('webkitTransitionEnd', function f(e) {
    if (targets.indexOf(e.target) >= 0 && targets.every(isHidden)) {
      document.removeEventListener(f, 'webkitTransitionEnd');
      testDone();
    }
  });

  targets.forEach(ensureTransition);
}

// Crashes on Mac only. See http://crbug.com/79181
GEN('#if defined(OS_MACOSX)');
GEN('#define MAYBE_testSetBooleanPrefTriggers ' +
    'DISABLED_testSetBooleanPrefTriggers');
GEN('#else');
GEN('#define MAYBE_testSetBooleanPrefTriggers testSetBooleanPrefTriggers');
GEN('#endif  // defined(OS_MACOSX)');

TEST_F('OptionsWebUITest', 'MAYBE_testSetBooleanPrefTriggers', function() {
  // TODO(dtseng): make generic to click all buttons.
  var showHomeButton =
      document.querySelector('input[pref="browser.show_home_button"]');
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
  var event = {target: defaultZoomFactor};
  if (defaultZoomFactor.onchange) defaultZoomFactor.onchange(event);
  testDone();
});

/**
 * If |confirmInterstitial| is true, the OK button of the Do Not Track
 * interstitial is pressed, otherwise the abort button is pressed.
 * @param {boolean} confirmInterstitial Whether to confirm the Do Not Track
 *     interstitial.
 */
OptionsWebUITest.prototype.testDoNotTrackInterstitial =
    function(confirmInterstitial) {
  Preferences.prefsFetchedCallback({'enable_do_not_track': {'value': false}});
  var buttonToClick = confirmInterstitial ? $('do-not-track-confirm-ok') :
                                            $('do-not-track-confirm-cancel');
  var dntCheckbox = $('do-not-track-enabled');
  var dntOverlay = PageManager.registeredOverlayPages['donottrackconfirm'];
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
          dntOverlay.removeEventListener('visibleChange', visibleChangeHandler);
          testDone();
        }, 0);
        break;
      default:
        assertTrue(false);
    }
  };
  dntOverlay.addEventListener('visibleChange', visibleChangeHandler);

  if (confirmInterstitial) {
    this.mockHandler.expects(once()).setBooleanPref(
        ['enable_do_not_track', true, 'Options_DoNotTrackCheckbox']);
  } else {
    // The mock handler complains if setBooleanPref is called even though
    // it should not be.
  }

  dntCheckbox.click();
};

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
  Preferences.prefsFetchedCallback({'enable_do_not_track': {'value': false}});
  var dntCheckbox = $('do-not-track-enabled');
  var dntOverlay = PageManager.registeredOverlayPages.donottrackconfirm;
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
          dntOverlay.removeEventListener('visibleChange', visibleChangeHandler);
          dntCheckbox.click();
        }, 0);
        break;
      default:
        assertNotReached();
    }
  };
  dntOverlay.addEventListener('visibleChange', visibleChangeHandler);

  this.mockHandler.expects(once()).setBooleanPref(
      eq(['enable_do_not_track', true, 'Options_DoNotTrackCheckbox']));

  var verifyCorrectEndState = function() {
    window.setTimeout(function() {
      assertFalse(dntOverlay.visible);
      assertFalse(dntCheckbox.checked);
      testDone();
    }, 0);
  };
  this.mockHandler.expects(once()).setBooleanPref(
      eq(['enable_do_not_track', false, 'Options_DoNotTrackCheckbox'])).will(
          callFunction(verifyCorrectEndState));

  dntCheckbox.click();
});

// Verify that preventDefault() is called on 'Enter' keydown events that trigger
// the default button. If this doesn't happen, other elements that may get
// focus (by the overlay closing for instance), will execute in addition to the
// default button. See crbug.com/268336.
TEST_F('OptionsWebUITest', 'EnterPreventsDefault', function() {
  var page = HomePageOverlay.getInstance();
  PageManager.showPageByName(page.name);
  var event = new KeyboardEvent('keydown', {
    'bubbles': true,
    'cancelable': true,
    'keyIdentifier': 'Enter'
  });
  assertFalse(event.defaultPrevented);
  page.pageDiv.dispatchEvent(event);
  assertTrue(event.defaultPrevented);
  testDone();
});

// Verifies that sending an empty list of indexes to move doesn't crash chrome.
TEST_F('OptionsWebUITest', 'emptySelectedIndexesDoesntCrash', function() {
  chrome.send('dragDropStartupPage', [0, []]);
  setTimeout(testDone);
});

// This test turns out to be flaky on all platforms.
// See http://crbug.com/315250.

// An overlay's position should remain the same as it shows.
TEST_F('OptionsWebUITest', 'DISABLED_OverlayShowDoesntShift', function() {
  var overlayName = 'startup';
  var overlay = $('startup-overlay');
  var frozenPages = document.getElementsByClassName('frozen');  // Gets updated.
  expectEquals(0, frozenPages.length);

  document.addEventListener('webkitTransitionEnd', function(e) {
    if (e.target != overlay)
      return;

    assertFalse(overlay.classList.contains('transparent'));
    expectEquals(numFrozenPages, frozenPages.length);
    testDone();
  });

  PageManager.showPageByName(overlayName);
  var numFrozenPages = frozenPages.length;
  expectGT(numFrozenPages, 0);
});

GEN('#if defined(OS_CHROMEOS)');
// Verify that range inputs respond to touch events. Currently only Chrome OS
// uses slider options.
TEST_F('OptionsWebUITest', 'RangeInputHandlesTouchEvents', function() {
  this.mockHandler.expects(once()).setIntegerPref([
    'settings.touchpad.sensitivity2', 1]);

  var touchpadRange = $('touchpad-sensitivity-range');
  var event = document.createEvent('UIEvent');
  event.initUIEvent('touchstart', true, true, window);
  touchpadRange.dispatchEvent(event);

  event = document.createEvent('UIEvent');
  event.initUIEvent('touchmove', true, true, window);
  touchpadRange.dispatchEvent(event);

  touchpadRange.value = 1;

  event = document.createEvent('UIEvent');
  event.initUIEvent('touchend', true, true, window);
  touchpadRange.dispatchEvent(event);

  // touchcancel should also trigger the handler, since it
  // changes the slider position.
  this.mockHandler.expects(once()).setIntegerPref([
    'settings.touchpad.sensitivity2', 2]);

  event = document.createEvent('UIEvent');
  event.initUIEvent('touchstart', true, true, window);
  touchpadRange.dispatchEvent(event);

  touchpadRange.value = 2;

  event = document.createEvent('UIEvent');
  event.initUIEvent('touchcancel', true, true, window);
  touchpadRange.dispatchEvent(event);

  testDone();
});
GEN('#endif');  // defined(OS_CHROMEOS)

/**
 * TestFixture for OptionsPage WebUI testing including tab history and support
 * for preference manipulation. If you don't need the features in the C++
 * fixture, use the simpler OptionsWebUITest (above) instead.
 * @extends {testing.Test}
 * @constructor
 */
function OptionsWebUIExtendedTest() {}

OptionsWebUIExtendedTest.prototype = {
  __proto__: OptionsWebUITest.prototype,

  /** @override */
  typedefCppFixture: 'OptionsBrowserTest',

  /** @override */
  setUp: function() {
    OptionsWebUITest.prototype.setUp.call(this);

    // Enable when failure is resolved.
    // AX_ARIA_10: http://crbug.com/559329
    this.accessibilityAuditConfig.ignoreSelectors(
        'unsupportedAriaAttribute',
        '#profiles-list');

    var controlsWithoutLabelSelectors = [
      '#cookies-view-page > .content-area.cookies-list-content-area > *',
      '#other-search-engine-list > .deletable-item > DIV > *',
    ];

    // Enable when failure is resolved.
    // AX_TEXT_01: http://crbug.com/559330
    this.accessibilityAuditConfig.ignoreSelectors(
        'controlsWithoutLabel',
        controlsWithoutLabelSelectors);

    var linkWithUnclearPurposeSelectors = [
      '#sync-overview > A',
      '#privacy-explanation > A',
      '#languages-section > .settings-row > A',
      '#cloudprint-options-mdns > .settings-row > A',
      // Selectors below only affect ChromeOS tests.
      '#privacy-section > DIV > DIV:nth-of-type(9) > A',
      '#accessibility-learn-more',
    ];

    // Enable when failure is resolved.
    // AX_TEXT_04: http://crbug.com/559326
    this.accessibilityAuditConfig.ignoreSelectors(
        'linkWithUnclearPurpose',
        linkWithUnclearPurposeSelectors);

    var requiredOwnedAriaRoleMissingSelectors = [
        '#default-search-engine-list',
        '#other-search-engine-list',
    ];

    // Enable when failure is resolved.
    // AX_ARIA_08: http://crbug.com/605689
    this.accessibilityAuditConfig.ignoreSelectors(
        'requiredOwnedAriaRoleMissing',
        requiredOwnedAriaRoleMissingSelectors);
  },

  testGenPreamble: function() {
    // Start with no supervised users managed by this profile.
    GEN('  ClearPref("' + SUPERVISED_USERS_PREF + '");');
  },

  /**
   * Asserts that two non-nested arrays are equal. The arrays must contain only
   * plain data types, no nested arrays or other objects.
   * @param {Array} expected An array of expected values.
   * @param {Array} result An array of actual values.
   * @param {boolean} doSort If true, the arrays will be sorted before being
   *     compared.
   * @param {string} description A brief description for the array of actual
   *     values, to use in an error message if the arrays differ.
   * @private
   */
  compareArrays_: function(expected, result, doSort, description) {
    var errorMessage = '\n' + description + ': ' + result +
                       '\nExpected: ' + expected;
    assertEquals(expected.length, result.length, errorMessage);

    var expectedSorted = expected.slice();
    var resultSorted = result.slice();
    if (doSort) {
      expectedSorted.sort();
      resultSorted.sort();
    }

    for (var i = 0; i < expectedSorted.length; ++i) {
      assertEquals(expectedSorted[i], resultSorted[i], errorMessage);
    }
  },

  /**
   * Verifies that the correct pages are currently open/visible.
   * @param {!Array<string>} expectedPages An array of page names expected to
   *     be open, with the topmost listed last.
   * @param {string=} opt_expectedUrl The URL path, including hash, expected to
   *     be open. If undefined, the topmost (last) page name in |expectedPages|
   *     will be used. In either case, 'chrome://settings-frame/' will be
   *     prepended.
   * @private
   */
  verifyOpenPages_: function(expectedPages, opt_expectedUrl) {
    // Check the topmost page.
    expectEquals(null, PageManager.getVisibleBubble());
    var currentPage = PageManager.getTopmostVisiblePage();

    var lastExpected = expectedPages[expectedPages.length - 1];
    expectEquals(lastExpected, currentPage.name);
    // We'd like to check the title too, but we have to load the settings-frame
    // instead of the outer settings page in order to have access to
    // OptionsPage, and setting the title from within the settings-frame fails
    // because of cross-origin access restrictions.
    // TODO(pamg): Add a test fixture that loads chrome://settings and uses
    // UI elements to access sub-pages, so we can test the titles and
    // search-page URLs.
    var expectedUrl = (typeof opt_expectedUrl == 'undefined') ?
        lastExpected : opt_expectedUrl;
    var fullExpectedUrl = 'chrome://settings-frame/' + expectedUrl;
    expectEquals(fullExpectedUrl, window.location.href);

    // Collect open pages.
    var allPageNames = Object.keys(PageManager.registeredPages).concat(
                       Object.keys(PageManager.registeredOverlayPages));
    var openPages = [];
    for (var i = 0; i < allPageNames.length; ++i) {
      var name = allPageNames[i];
      var page = PageManager.registeredPages[name] ||
                 PageManager.registeredOverlayPages[name];
      if (page.visible)
        openPages.push(page.name);
    }

    this.compareArrays_(expectedPages, openPages, true, 'Open pages');
  },

  /*
   * Verifies that the correct URLs are listed in the history. Asynchronous.
   * @param {!Array<string>} expectedHistory An array of URL paths expected to
   *     be in the tab navigation history, sorted by visit time, including the
   *     current page as the last entry. The base URL (chrome://settings-frame/)
   *     will be prepended to each. An initial 'about:blank' history entry is
   *     assumed and should not be included in this list.
   * @param {Function=} callback A function to be called after the history has
   *     been verified successfully. May be undefined.
   * @private
   */
  verifyHistory_: function(expectedHistory, callback) {
    var self = this;
    OptionsWebUIExtendedTest.verifyHistoryCallback = function(results) {
      // The history always starts with a blank page.
      assertEquals('about:blank', results.shift());
      var fullExpectedHistory = [];
      for (var i = 0; i < expectedHistory.length; ++i) {
        fullExpectedHistory.push(
            'chrome://settings-frame/' + expectedHistory[i]);
      }
      self.compareArrays_(fullExpectedHistory, results, false, 'History');
      callback();
    };

    // The C++ fixture will call verifyHistoryCallback with the results.
    chrome.send('optionsTestReportHistory');
  },

  /**
   * Overrides the page callbacks for the given PageManager overlay to verify
   * that they are not called.
   * @param {Object} overlay The singleton instance of the overlay.
   * @private
   */
  prohibitChangesToOverlay_: function(overlay) {
    overlay.initializePage =
        overlay.didShowPage =
        overlay.didClosePage = function() {
          assertTrue(false,
                     'Overlay was affected when changes were prohibited.');
        };
  },
};

/**
 * Set by verifyHistory_ to incorporate a followup callback, then called by the
 * C++ fixture with the navigation history to be verified.
 * @type {Function}
 */
OptionsWebUIExtendedTest.verifyHistoryCallback = null;

// Show the search page with no query string, to fall back to the settings page.
// Test disabled because it's flaky. crbug.com/303841
TEST_F('OptionsWebUIExtendedTest', 'DISABLED_ShowSearchPageNoQuery',
       function() {
  PageManager.showPageByName('search');
  this.verifyOpenPages_(['settings']);
  this.verifyHistory_(['settings'], testDone);
});

// Manipulate the search page via the search field.
TEST_F('OptionsWebUIExtendedTest', 'ShowSearchFromField', function() {
  $('search-field').onsearch({currentTarget: {value: 'query'}});
  this.verifyOpenPages_(['settings', 'search'], 'search#query');
  this.verifyHistory_(['', 'search#query'], function() {
    $('search-field').onsearch({currentTarget: {value: 'query2'}});
    this.verifyOpenPages_(['settings', 'search'], 'search#query2');
    this.verifyHistory_(['', 'search#query', 'search#query2'], function() {
      $('search-field').onsearch({currentTarget: {value: ''}});
      this.verifyOpenPages_(['settings'], '');
      this.verifyHistory_(['', 'search#query', 'search#query2', ''], testDone);
    }.bind(this));
  }.bind(this));
});

// Show a page without updating history.
TEST_F('OptionsWebUIExtendedTest', 'ShowPageNoHistory', function() {
  this.verifyOpenPages_(['settings'], '');
  PageManager.showPageByName('search', true, {hash: '#query'});

  // The settings page is also still "open" (i.e., visible), in order to show
  // the search results. Furthermore, the URL hasn't been updated in the parent
  // page, because we've loaded the chrome-settings frame instead of the whole
  // settings page, so the cross-origin call to set the URL fails.
  this.verifyOpenPages_(['settings', 'search'], 'search#query');
  var self = this;
  this.verifyHistory_(['', 'search#query'], function() {
    PageManager.showPageByName('settings', false);
    self.verifyOpenPages_(['settings'], 'search#query');
    self.verifyHistory_(['', 'search#query'], testDone);
  });
});

TEST_F('OptionsWebUIExtendedTest', 'ShowPageWithHistory', function() {
  PageManager.showPageByName('search', true, {hash: '#query'});
  var self = this;
  this.verifyHistory_(['', 'search#query'], function() {
    PageManager.showPageByName('settings', true);
    self.verifyOpenPages_(['settings'], '');
    self.verifyHistory_(['', 'search#query', ''],
                        testDone);
  });
});

TEST_F('OptionsWebUIExtendedTest', 'ShowPageReplaceHistory', function() {
  PageManager.showPageByName('search', true, {hash: '#query'});
  var self = this;
  this.verifyHistory_(['', 'search#query'], function() {
    PageManager.showPageByName('settings', true, {'replaceState': true});
    self.verifyOpenPages_(['settings'], '');
    self.verifyHistory_(['', ''], testDone);
  });
});

// This should be identical to ShowPageWithHisory.
TEST_F('OptionsWebUIExtendedTest', 'NavigateToPage', function() {
  PageManager.showPageByName('search', true, {hash: '#query'});
  var self = this;
  this.verifyHistory_(['', 'search#query'], function() {
    PageManager.showPageByName('settings');
    self.verifyOpenPages_(['settings'], '');
    self.verifyHistory_(['', 'search#query', ''], testDone);
  });
});

// Settings overlays are much more straightforward than settings pages, opening
// normally with none of the latter's quirks in the expected history or URL.
TEST_F('OptionsWebUIExtendedTest', 'ShowOverlayNoHistory', function() {
  // Open a layer-1 overlay, not updating history.
  PageManager.showPageByName('languages', false);
  this.verifyOpenPages_(['settings', 'languages'], '');

  var self = this;
  this.verifyHistory_([''], function() {
    // Open a layer-2 overlay for which the layer-1 is a parent, not updating
    // history.
    PageManager.showPageByName('addLanguage', false);
    self.verifyOpenPages_(['settings', 'languages', 'addLanguage'],
                          '');
    self.verifyHistory_([''], testDone);
  });
});

TEST_F('OptionsWebUIExtendedTest', 'ShowOverlayWithHistory', function() {
  // Open a layer-1 overlay, updating history.
  PageManager.showPageByName('languages', true);
  this.verifyOpenPages_(['settings', 'languages']);

  var self = this;
  this.verifyHistory_(['', 'languages'], function() {
    // Open a layer-2 overlay, updating history.
    PageManager.showPageByName('addLanguage', true);
    self.verifyOpenPages_(['settings', 'languages', 'addLanguage']);
    self.verifyHistory_(['', 'languages', 'addLanguage'], testDone);
  });
});

TEST_F('OptionsWebUIExtendedTest', 'ShowOverlayReplaceHistory', function() {
  // Open a layer-1 overlay, updating history.
  PageManager.showPageByName('languages', true);
  var self = this;
  this.verifyHistory_(['', 'languages'], function() {
    // Open a layer-2 overlay, replacing history.
    PageManager.showPageByName('addLanguage', true, {'replaceState': true});
    self.verifyOpenPages_(['settings', 'languages', 'addLanguage']);
    self.verifyHistory_(['', 'addLanguage'], testDone);
  });
});

// Directly show an overlay further above this page, i.e. one for which the
// current page is an ancestor but not a parent.
TEST_F('OptionsWebUIExtendedTest', 'ShowOverlayFurtherAbove', function() {
  // Open a layer-2 overlay directly.
  PageManager.showPageByName('addLanguage', true);
  this.verifyOpenPages_(['settings', 'languages', 'addLanguage']);
  var self = this;
  this.verifyHistory_(['', 'addLanguage'], testDone);
});

// Directly show a layer-2 overlay for which the layer-1 overlay is not a
// parent.
TEST_F('OptionsWebUIExtendedTest', 'ShowUnrelatedOverlay', function() {
  // Open a layer-1 overlay.
  PageManager.showPageByName('languages', true);
  this.verifyOpenPages_(['settings', 'languages']);

  var self = this;
  this.verifyHistory_(['', 'languages'], function() {
    // Open an unrelated layer-2 overlay.
    PageManager.showPageByName('cookies', true);
    self.verifyOpenPages_(['settings', 'content', 'cookies']);
    self.verifyHistory_(['', 'languages', 'cookies'], testDone);
  });
});

// Close an overlay.
TEST_F('OptionsWebUIExtendedTest', 'CloseOverlay', function() {
  // Open a layer-1 overlay, then a layer-2 overlay on top of it.
  PageManager.showPageByName('languages', true);
  this.verifyOpenPages_(['settings', 'languages']);
  PageManager.showPageByName('addLanguage', true);
  this.verifyOpenPages_(['settings', 'languages', 'addLanguage']);

  var self = this;
  this.verifyHistory_(['', 'languages', 'addLanguage'], function() {
    // Close the layer-2 overlay.
    PageManager.closeOverlay();
    self.verifyOpenPages_(['settings', 'languages']);
    self.verifyHistory_(
        ['', 'languages', 'addLanguage', 'languages'],
        function() {
      // Close the layer-1 overlay.
      PageManager.closeOverlay();
      self.verifyOpenPages_(['settings'], '');
      self.verifyHistory_(
          ['', 'languages', 'addLanguage', 'languages', ''],
          function noop() {});
      waitUntilHidden([$('overlay-container-1'), $('overlay-container-2')]);
    });
  });
});

// Hashes are maintained separately for each page and are preserved when
// overlays close.
TEST_F('OptionsWebUIExtendedTest', 'CloseOverlayWithHashes', function() {
  // Open an overlay on top of the search page.
  PageManager.showPageByName('search', true, {hash: '#1'});
  this.verifyOpenPages_(['settings', 'search'], 'search#1');
  PageManager.showPageByName('languages', true, {hash: '#2'});
  this.verifyOpenPages_(['settings', 'search', 'languages'],
                        'languages#2');
  PageManager.showPageByName('addLanguage', true, {hash: '#3'});
  this.verifyOpenPages_(['settings', 'search', 'languages', 'addLanguage'],
                       'addLanguage#3');

  this.verifyHistory_(['', 'search#1', 'languages#2', 'addLanguage#3'],
                      function() {
    // Close the layer-2 overlay.
    PageManager.closeOverlay();
    this.verifyOpenPages_(['settings', 'search', 'languages'], 'languages#2');
    this.verifyHistory_(
        ['', 'search#1', 'languages#2', 'addLanguage#3', 'languages#2'],
        function() {
      // Close the layer-1 overlay.
      PageManager.closeOverlay();
      this.verifyOpenPages_(['settings', 'search'], 'search#1');
      this.verifyHistory_(
          ['', 'search#1', 'languages#2', 'addLanguage#3', 'languages#2',
           'search#1'],
          function noop() {});
      waitUntilHidden([$('overlay-container-1'), $('overlay-container-2')]);
    }.bind(this));
  }.bind(this));
});

// Test that closing an overlay that did not push history when opening does not
// again push history.
TEST_F('OptionsWebUIExtendedTest', 'CloseOverlayNoHistory', function() {
  // Open the do not track confirmation prompt.
  PageManager.showPageByName('doNotTrackConfirm', false);

  // Opening the prompt does not add to the history.
  this.verifyHistory_([''], function() {
    // Close the overlay.
    PageManager.closeOverlay();
    // Still no history changes.
    this.verifyHistory_([''], function noop() {});
    waitUntilHidden([$('overlay-container-1')]);
  }.bind(this));
});

// Make sure an overlay isn't closed (even temporarily) when another overlay is
// opened on top.
TEST_F('OptionsWebUIExtendedTest', 'OverlayAboveNoReset', function() {
  // Open a layer-1 overlay.
  PageManager.showPageByName('languages', true);
  this.verifyOpenPages_(['settings', 'languages']);

  // Open a layer-2 overlay on top. This should not close 'languages'.
  this.prohibitChangesToOverlay_(options.LanguageOptions.getInstance());
  PageManager.showPageByName('addLanguage', true);
  this.verifyOpenPages_(['settings', 'languages', 'addLanguage']);
  testDone();
});

TEST_F('OptionsWebUIExtendedTest', 'OverlayTabNavigation', function() {
  // Open a layer-1 overlay, then a layer-2 overlay on top of it.
  PageManager.showPageByName('languages', true);
  PageManager.showPageByName('addLanguage', true);
  var self = this;

  // Go back twice, then forward twice.
  self.verifyOpenPages_(['settings', 'languages', 'addLanguage']);
  self.verifyHistory_(['', 'languages', 'addLanguage'], function() {
    window.history.back();
    waitForPopstate(function() {
      self.verifyOpenPages_(['settings', 'languages']);
      self.verifyHistory_(['', 'languages'], function() {
        window.history.back();
        waitForPopstate(function() {
          self.verifyOpenPages_(['settings'], '');
          self.verifyHistory_([''], function() {
            window.history.forward();
            waitForPopstate(function() {
              self.verifyOpenPages_(['settings', 'languages']);
              self.verifyHistory_(['', 'languages'], function() {
                window.history.forward();
                waitForPopstate(function() {
                  self.verifyOpenPages_(
                      ['settings', 'languages', 'addLanguage']);
                  self.verifyHistory_(
                      ['', 'languages', 'addLanguage'], testDone);
                });
              });
            });
          });
        });
      });
    });
  });
});

// Going "back" to an overlay that's a child of the current overlay shouldn't
// close the current one.
TEST_F('OptionsWebUIExtendedTest', 'OverlayBackToChild', function() {
  // Open a layer-1 overlay, then a layer-2 overlay on top of it.
  PageManager.showPageByName('languages', true);
  PageManager.showPageByName('addLanguage', true);
  var self = this;

  self.verifyOpenPages_(['settings', 'languages', 'addLanguage']);
  self.verifyHistory_(['', 'languages', 'addLanguage'], function() {
    // Close the top overlay, then go back to it.
    PageManager.closeOverlay();
    self.verifyOpenPages_(['settings', 'languages']);
    self.verifyHistory_(
        ['', 'languages', 'addLanguage', 'languages'],
        function() {
      // Going back to the 'addLanguage' page should not close 'languages'.
      self.prohibitChangesToOverlay_(options.LanguageOptions.getInstance());
      window.history.back();
      waitForPopstate(function() {
        self.verifyOpenPages_(['settings', 'languages', 'addLanguage']);
        self.verifyHistory_(['', 'languages', 'addLanguage'],
                            testDone);
      });
    });
  });
});

// Going back to an unrelated overlay should close the overlay and its parent.
TEST_F('OptionsWebUIExtendedTest', 'OverlayBackToUnrelated', function() {
  // Open a layer-1 overlay, then an unrelated layer-2 overlay.
  PageManager.showPageByName('languages', true);
  PageManager.showPageByName('cookies', true);
  var self = this;
  self.verifyOpenPages_(['settings', 'content', 'cookies']);
  self.verifyHistory_(['', 'languages', 'cookies'], function() {
    window.history.back();
    waitForPopstate(function() {
      self.verifyOpenPages_(['settings', 'languages']);
      testDone();
    });
  });
});

// Verify history changes properly while the page is loading.
TEST_F('OptionsWebUIExtendedTest', 'HistoryUpdatedAfterLoading', function() {
  var loc = location.href;

  document.documentElement.classList.add('loading');
  assertTrue(PageManager.isLoading());
  PageManager.showPageByName('searchEngines');
  expectNotEquals(loc, location.href);

  document.documentElement.classList.remove('loading');
  assertFalse(PageManager.isLoading());
  PageManager.showDefaultPage();
  expectEquals(loc, location.href);

  testDone();
});

// A tip should be shown or hidden depending on whether this profile manages any
// supervised users.
TEST_F('OptionsWebUIExtendedTest', 'SupervisingUsers', function() {
  // We start managing no supervised users.
  assertTrue($('profiles-supervised-dashboard-tip').hidden);

  // Remove all supervised users, then add some, watching for the pref change
  // notifications and UI updates in each case. Any non-empty pref dictionary
  // is interpreted as having supervised users.
  chrome.send('optionsTestSetPref', [SUPERVISED_USERS_PREF, {key: 'value'}]);
  waitForResponse(BrowserOptions, 'updateManagesSupervisedUsers', function() {
    assertFalse($('profiles-supervised-dashboard-tip').hidden);
    chrome.send('optionsTestSetPref', [SUPERVISED_USERS_PREF, {}]);
    waitForResponse(BrowserOptions, 'updateManagesSupervisedUsers', function() {
      assertTrue($('profiles-supervised-dashboard-tip').hidden);
      testDone();
    });
  });
});

/**
 * TestFixture that loads the options page at a bogus URL.
 * @extends {OptionsWebUIExtendedTest}
 * @constructor
 */
function OptionsWebUIRedirectTest() {
  OptionsWebUIExtendedTest.call(this);
}

OptionsWebUIRedirectTest.prototype = {
  __proto__: OptionsWebUIExtendedTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings-frame/nonexistantPage',
};

TEST_F('OptionsWebUIRedirectTest', 'TestURL', function() {
  assertEquals('chrome://settings-frame/', document.location.href);
  this.verifyHistory_([''], testDone);
});

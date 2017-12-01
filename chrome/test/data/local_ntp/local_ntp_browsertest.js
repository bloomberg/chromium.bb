// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Tests the local NTP.
 */


/**
 * Local NTP's object for test and setup functions.
 */
test.localNtp = {};


/**
 * Sets up the page for each individual test.
 */
test.localNtp.setUp = function() {
  setUpPage('local-ntp-template');
};


// ******************************* SIMPLE TESTS *******************************
// These are run by runSimpleTests above.
// Functions from test_utils.js are automatically imported.


/**
 * Tests that Google NTPs show a fakebox and logo.
 */
test.localNtp.testShowsFakeboxAndLogoIfGoogle = function() {
  initLocalNTP(/*isGooglePage=*/true);
  assertTrue(elementIsVisible($('fakebox')));
  assertTrue(elementIsVisible($('logo')));
};


/**
 * Tests that non-Google NTPs do not show a fakebox.
 */
test.localNtp.testDoesNotShowFakeboxIfNotGoogle = function() {
  initLocalNTP(/*isGooglePage=*/false);
  assertFalse(elementIsVisible($('fakebox')));
  assertFalse(elementIsVisible($('logo')));
};


/**
 * Tests that the embeddedSearch.newTabPage.mostVisited API is
 * hooked up, and provides the correct data for the tiles (i.e. only
 * IDs, no URLs).
 */
test.localNtp.testMostVisitedContents = function() {
  // Check that the API is available and properly hooked up, so that it returns
  // some data (see history::PrepopulatedPageList for the default contents).
  assert(window.chrome.embeddedSearch.newTabPage.mostVisited.length > 0);

  // Check that the items have the required fields: We expect a "restricted ID"
  // (rid), but there mustn't be url, title, etc. Those are only available
  // through getMostVisitedItemData(rid).
  for (var mvItem of window.chrome.embeddedSearch.newTabPage.mostVisited) {
    assertTrue(isFinite(mvItem.rid));
    assert(!mvItem.url);
    assert(!mvItem.title);
    assert(!mvItem.domain);
  }

  // Try to get an item's details via getMostVisitedItemData. This should fail,
  // because that API is only available to the MV iframe.
  assert(!window.chrome.embeddedSearch.newTabPage.getMostVisitedItemData(
      window.chrome.embeddedSearch.newTabPage.mostVisited[0].rid));
};


// ***************************** HELPER FUNCTIONS *****************************
// Helper functions used in tests.


/**
 * Creates and initializes a LocalNTP object.
 * @param {boolean} isGooglePage Whether to make it a Google-branded NTP.
 */
function initLocalNTP(isGooglePage) {
  configData.isGooglePage = isGooglePage;
  var localNTP = LocalNTP();
  localNTP.init();
}


// ****************************** ADVANCED TESTS ******************************
// Advanced tests are controlled from the native side. The helpers here are
// called from native code to set up the page and to check results.


function setupAdvancedTest() {
  setUpPage('local-ntp-template');
  initLocalNTP(/*isGooglePage=*/true);

  assertTrue(elementIsVisible($('fakebox')));

  return true;
}


function getFakeboxPositionX() {
  assertTrue(elementIsVisible($('fakebox')));
  var rect = $('fakebox').getBoundingClientRect();
  return rect.left;
}


function getFakeboxPositionY() {
  assertTrue(elementIsVisible($('fakebox')));
  var rect = $('fakebox').getBoundingClientRect();
  return rect.top;
}


function fakeboxIsVisible() {
  return elementIsVisible($('fakebox'));
}


function fakeboxIsFocused() {
  return fakeboxIsVisible() &&
      document.body.classList.contains('fakebox-focused');
}

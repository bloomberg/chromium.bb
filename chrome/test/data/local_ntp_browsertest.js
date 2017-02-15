// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Tests the local NTP.
 */


/**
 * Shortcut for document.getElementById.
 * @param {string} id of the element.
 * @return {HTMLElement} with the id.
 */
function $(id) {
  return document.getElementById(id);
}


/**
 * Sets up for the next test case. Recreates the default local NTP DOM.
 */
function setUp() {
  // First, clear up the DOM and state left over from any previous test case.
  document.body.innerHTML = '';
  // The NTP stores some state such as fakebox focus in the body's classList.
  document.body.classList = '';

  document.body.appendChild($('local-ntp-body').content.cloneNode(true));
}

/**
 * Aborts a test if a condition is not met.
 * @param {boolean} condition The condition that must be true.
 * @param {string=} opt_message A message to log if the condition is not met.
 */
function assert(condition, opt_message) {
  if (!condition)
    throw new Error(opt_message || 'Assertion failed');
}

/**
 * Runs all simple tests, i.e. those that don't require interaction from the
 * native side.
 * @return {boolean} True if all tests pass and false otherwise.
 */
function runSimpleTests() {
  var pass = true;
  for (var testName in window) {
    if (/^test.+/.test(testName) && typeof window[testName] == 'function') {
      try {
        setUp();
        window[testName].call(window);
      } catch (err) {
        window.console.log(testName + ' ' + err);
        pass = false;
      }
    }
  }
  return pass;
}


/**
 * Creates and initializes a LocalNTP object.
 * @param {boolean} isGooglePage Whether to make it a Google-branded NTP.
 */
function initLocalNTP(isGooglePage) {
  configData.isGooglePage = isGooglePage;
  var localNTP = LocalNTP();
  localNTP.init();
}


/**
 * Checks whether a given HTMLElement exists and is visible.
 * @param {HTMLElement|undefined} elem An HTMLElement.
 * @return {boolean} True if the element exists and is visible.
 */
function elementIsVisible(elem) {
  return elem && elem.offsetWidth > 0 && elem.offsetHeight > 0 &&
      window.getComputedStyle(elem).visibility != 'hidden';
}


// ******************************* SIMPLE TESTS *******************************
// These are run by runSimpleTests above.


/**
 * Tests that Google NTPs show a fakebox and logo.
 */
function testShowsFakeboxAndLogoIfGoogle() {
  initLocalNTP(/*isGooglePage=*/true);
  assert(elementIsVisible($('fakebox')));
  assert(elementIsVisible($('logo')));
}


/**
 * Tests that non-Google NTPs do not show a fakebox.
 */
function testDoesNotShowFakeboxIfNotGoogle() {
  initLocalNTP(/*isGooglePage=*/false);
  assert(!elementIsVisible($('fakebox')));
  assert(!elementIsVisible($('logo')));
}



// ****************************** ADVANCED TESTS ******************************
// Advanced tests are controlled from the native side. The helpers here are
// called from native code to set up the page and to check results.

function setupAdvancedTest() {
  setUp();
  initLocalNTP(/*isGooglePage=*/true);

  assert(elementIsVisible($('fakebox')));

  return true;
}

function getFakeboxPositionX() {
  assert(elementIsVisible($('fakebox')));
  var rect = $('fakebox').getBoundingClientRect();
  return rect.left;
}

function getFakeboxPositionY() {
  assert(elementIsVisible($('fakebox')));
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

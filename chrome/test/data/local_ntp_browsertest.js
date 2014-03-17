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
  document.body.innerHTML = '';
  document.body.appendChild($('local-ntp-body').content.cloneNode(true));
}


/**
 * Cleans up after test execution.
 */
function tearDown() {
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
 * Runs all tests.
 * @return {boolean} True if all tests pass and false otherwise.
 */
function runTests() {
  var pass = true;
  for (var testName in window) {
    if (/^test.+/.test(testName) && typeof window[testName] == 'function') {
      try {
        setUp();
        window[testName].call(window);
        tearDown();
      } catch (err) {
        window.console.log(testName + ' ' + err);
        pass = false;
      }
    }
  }
  return pass;
}


/**
 * Tests that Google NTPs show a fakebox and logo.
 */
function testShowsFakeboxAndLogoIfGoogle() {
  var localNTP = LocalNTP();
  configData.isGooglePage = true;
  localNTP.init();
  assert($('fakebox'));
  assert($('logo'));
}


/**
 * Tests that non-Google NTPs do not show a fakebox.
 */
function testDoesNotShowFakeboxIfNotGoogle() {
  var localNTP = LocalNTP();
  configData.isGooglePage = false;
  localNTP.init();
  assert(!$('fakebox'));
  assert(!$('logo'));
}


/**
 * Tests that clicking on a Most Visited link calls navigateContentWindow.
 */
function testMostVisitedLinkCallsNavigateContentWindow() {
  var ntpHandle = chrome.embeddedSearch.newTabPage;
  var originalNavigateContentWindow = ntpHandle.navigateContentWindow;

  var navigateContentWindowCalls = 0;
  ntpHandle.navigateContentWindow = function() {
    navigateContentWindowCalls++;
  }

  var params = {};
  var href = 'file:///some/local/file';
  var title = 'Title';
  var text = 'text';
  var provider = 'foobar';
  var link = createMostVisitedLink(params, href, title, text, provider);

  link.click();

  ntpHandle.navigateContentWindow = originalNavigateContentWindow;
  assert(navigateContentWindowCalls > 0);
}

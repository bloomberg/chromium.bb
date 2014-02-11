// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestFixture for Invalidations WebUI testing.
 * @extends {testing.Test}
 * @constructor
 **/
function InvalidationsWebUITest() {}

InvalidationsWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the Invalidations page.
   **/
  browsePreload: 'chrome://invalidations',
  runAccessibilityChecks: false,
  accessibilityIssuesAreErrors: false,
};

// Test that registering an invalidations appears properly on the textarea.
TEST_F('InvalidationsWebUITest', 'testRegisteringNewInvalidation', function() {
  var invalidationsLog = $('invalidations-log');
  var invalidation = [{
    isUnknownVersion: 'true',
    objectId: {name: 'EXTENSIONS', source: 1004}
    }];
  invalidationsLog.value = '';
  chrome.invalidations.logInvalidations(invalidation);
  var isContained =
    invalidationsLog.value.indexOf(
      'Received Invalidation with type ' +
      '"EXTENSIONS" version "Unknown" with payload "undefined"') != -1;
  expectTrue(isContained, 'Actual log is:' + invalidationsLog.value);

});

// Test that changing the Invalidations Service state appears both in the
// span and in the textarea.
TEST_F('InvalidationsWebUITest', 'testChangingInvalidationsState', function() {
  var invalidationsState = $('invalidations-state');
  var invalidationsLog = $('invalidations-log');
  var newState = 'INVALIDATIONS_ENABLED';
  var newNewState = 'TRANSIENT_INVALIDATION_ERROR';

  chrome.invalidations.updateState(newState);
  expectEquals(invalidationsState.textContent,
    'INVALIDATIONS_ENABLED',
    'could not change the invalidations text');
  invalidationsLog.value = '';
  chrome.invalidations.updateState(newNewState);
  expectEquals(invalidationsState.textContent,
    'TRANSIENT_INVALIDATION_ERROR');
  var isContained =
    invalidationsLog.value.indexOf(
      'Invalidations service state changed to '+
      '"TRANSIENT_INVALIDATION_ERROR"') != -1;
  expectTrue(isContained, 'Actual log is:' + invalidationsLog.value);
});


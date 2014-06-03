// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @type {string}
 * @const
 */
var UNTIL_ADDED = 'untilAdded';

/**
 * @type {string}
 * @const
 */
var UNTIL_REMOVED = 'untilRemoved';

/**
 * Waits for the 'move to profileId' menu to appear or to disappear.
 * @param {string} windowId Window ID.
 * @param {string} profileId Profile ID.
 * @param {string} waitMode UNTIL_ADDED or UNTIL_REMOVED.
 * @return {Promise} Promise to be fulfilled when the menu appears or
 *     disappears.
 */
function waitForVisitDesktopMenu(windowId, profileId, waitMode) {
  if (waitMode !== UNTIL_ADDED && waitMode !== UNTIL_REMOVED)
    throw new Error('Wait mode is invalid: ' + waitMode);
  return repeatUntil(function() {
    return callRemoteTestUtil('queryAllElements', windowId, ['.visit-desktop']).
        then(function(elements) {
          var found = false;
          for (var i = 0; i < elements.length; i++) {
            if (elements[i].text.indexOf(profileId) !== -1) {
              found = true;
              break;
            }
          }
          if ((waitMode === UNTIL_ADDED && !found) ||
              (waitMode === UNTIL_REMOVED && found))
            return pending(
                'Waiting for the visit desktop menu to %s: ' +
                '%s, but currently it is %j.',
                waitMode === UNTIL_ADDED ? 'contain' : 'exclude',
                profileId,
                elements);
        });
  });
}

testcase.multiProfileBadge = function() {
  var appId;
  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS, this.next);
    },
    // Add all users.
    function(inAppId) {
      appId = inAppId;
      chrome.test.sendMessage(JSON.stringify({name: 'addAllUsers'}),
                              this.next);
    },
    // Get the badge element.
    function(json) {
      chrome.test.assertTrue(JSON.parse(json));
      waitForElement(appId, '#profile-badge').then(this.next);
    },
    // Verify no badge image is shown yet.  Move to other deskop.
    function(element) {
      chrome.test.assertTrue(element.hidden, 'Badge hidden initially');
      callRemoteTestUtil('visitDesktop',
                         appId,
                         ['bob@invalid.domain'],
                         this.next);
    },
    // Get the badge element again.
    function(result) {
      chrome.test.assertTrue(result);
      waitForElement(appId, '#profile-badge:not([hidden])').then(this.next);
    },
    // Verify an image source is filled. Go back to the original desktop
    function(element) {
      callRemoteTestUtil('queryAllElements',
                         appId,
                         ['#profile-badge',
                          null,
                          ['background']]).then(this.next);
    },
    function(elements) {
      chrome.test.assertTrue(
          elements[0].styles.background.indexOf('data:image') !== -1,
          'Badge shown after moving desktop');
      callRemoteTestUtil('visitDesktop',
                         appId,
                         ['alice@invalid.domain'],
                         this.next);
    },
    // Wait for #profile-badge element to disappear.
    function(result) {
      chrome.test.assertTrue(result);
      waitForElementLost(appId, '#profile-badge:not([hidden])').then(this.next);
    },
    // The image is gone.
    function(result) {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

testcase.multiProfileVisitDesktopMenu = function() {
  var appId;
  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS, this.next);
    },
    // Add all users.
    function(inAppId) {
      appId = inAppId;
      chrome.test.sendMessage(JSON.stringify({name: 'addAllUsers'}),
                              this.next);
    },
    // Wait for menu items. Profiles for non-current desktop should appear and
    // the profile for the current desktop (alice) should be hidden.
    function(json) {
      chrome.test.assertTrue(JSON.parse(json));
      Promise.all([
        waitForVisitDesktopMenu(appId, 'bob@invalid.domain', UNTIL_ADDED),
        waitForVisitDesktopMenu(appId, 'charlie@invalid.domain', UNTIL_ADDED),
        waitForVisitDesktopMenu(appId, 'alice@invalid.domain', UNTIL_REMOVED)
      ]).
      then(this.next, function(error) {
        chrome.test.fail(error.stack || error);
      });
    },
    // Activate the visit desktop menu.
    function() {
      callRemoteTestUtil('runVisitDesktopMenu',
                         appId,
                         ['bob@invalid.domain'],
                         this.next);
    },
    // Wait for the new menu state. Now 'alice' is shown and 'bob' is hidden.
    function(result) {
      chrome.test.assertTrue(result);
      Promise.all([
        waitForVisitDesktopMenu(appId, 'bob@invalid.domain', UNTIL_REMOVED),
        waitForVisitDesktopMenu(appId, 'charlie@invalid.domain', UNTIL_ADDED),
        waitForVisitDesktopMenu(appId, 'alice@invalid.domain', UNTIL_ADDED)
      ]).
      then(this.next, function(error) {
        chrome.test.fail(error.stack || error);
      });
    },
    // Check that the window owner has indeed been changed.
    function() {
      chrome.test.sendMessage(JSON.stringify({name: 'getWindowOwnerId'}),
                              this.next);
    },
    function(result) {
      chrome.test.assertEq('bob@invalid.domain', result);
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

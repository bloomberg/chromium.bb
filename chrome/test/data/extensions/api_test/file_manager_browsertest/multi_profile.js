// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

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
      callRemoteTestUtil('waitForElement',
                         appId,
                         ['#profile-badge'],
                         this.next);
    },
    // Verify no badge image is shown yet.  Move to other deskop.
    function(element) {
      chrome.test.assertTrue(!element.attributes.src, 'Badge hidden initially');
      callRemoteTestUtil('visitDesktop',
                         appId,
                         ['bob@invalid.domain'],
                         this.next);
    },
    // Get the badge element again.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('waitForElement',
                         appId,
                         ['#profile-badge[src]'],
                         this.next);
    },
    // Verify an image source is filled. Go back to the original desktop
    function(element) {
      chrome.test.assertTrue(element.attributes.src.indexOf('data:image') === 0,
                             'Badge shown after moving desktop');
      callRemoteTestUtil('visitDesktop',
                         appId,
                         ['alice@invalid.domain'],
                         this.next);
    },
    // Wait for #profile-badge element with .src to disappear.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('waitForElement',
                         appId,
                         ['#profile-badge[src]', null, true],
                         this.next);
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
      callRemoteTestUtil('waitForVisitDesktopMenu',
                         appId,
                         ['bob@invalid.domain', true],
                         this.next);
    },
    function() {
      callRemoteTestUtil('waitForVisitDesktopMenu',
                         appId,
                         ['charlie@invalid.domain', true],
                         this.next);
    },
    function() {
      callRemoteTestUtil('waitForVisitDesktopMenu',
                         appId,
                         ['alice@invalid.domain', false],
                         this.next);
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
      callRemoteTestUtil('waitForVisitDesktopMenu',
                         appId,
                         ['alice@invalid.domain', true],
                         this.next);
    },
    function() {
      callRemoteTestUtil('waitForVisitDesktopMenu',
                         appId,
                         ['charlie@invalid.domain', true],
                         this.next);
    },
    function() {
      callRemoteTestUtil('waitForVisitDesktopMenu',
                         appId,
                         ['bob@invalid.domain', false],
                         this.next);
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

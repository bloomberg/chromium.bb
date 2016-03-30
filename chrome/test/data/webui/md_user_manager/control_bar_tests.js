// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('user_manager.control_bar_tests', function() {
  function registerTests() {
    suite('ControlBarTests', function() {
      /** @type {?TestProfileBrowserProxy} */
      var browserProxy = null;

      /** @type {?ControlBarElement} */
      var controlBarElement = null;

      setup(function() {
        browserProxy = new TestProfileBrowserProxy();
        // Replace real proxy with mock proxy.
        signin.ProfileBrowserProxyImpl.instance_ = browserProxy;

        PolymerTest.clearBody();
        controlBarElement = document.createElement('control-bar');
        document.body.appendChild(controlBarElement);
      });

      teardown(function() { controlBarElement.remove(); });

      test('Actions are hidden by default', function() {
        assertTrue(controlBarElement.$.launchGuest.hidden);
        assertTrue(controlBarElement.$.addUser.hidden);

        controlBarElement.showGuest = true;
        controlBarElement.showAddPerson = true;
        Polymer.dom.flush();

        assertFalse(controlBarElement.$.launchGuest.hidden);
        assertFalse(controlBarElement.$.addUser.hidden);
      });

      test('Can create profile', function() {
        return new Promise(function(resolve, reject) {
          // We expect to go to the 'create-profile' page.
          controlBarElement.addEventListener('change-page', function(event) {
            if (event.detail.page == 'create-user-page')
              resolve();
          });

          // Simulate clicking 'Create Profile'.
          MockInteractions.tap(controlBarElement.$.addUser);
        });
      });

      test('Can launch guest profile', function() {
        // Simulate clicking 'Browse as guest'.
        MockInteractions.tap(controlBarElement.$.launchGuest);
        return browserProxy.whenCalled('launchGuestUser');
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});

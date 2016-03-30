// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('user_manager.create_profile_tests', function() {
  /** @return {!CreateProfileElement} */
  function createElement() {
    PolymerTest.clearBody();
    var createProfileElement = document.createElement('create-profile');
    document.body.appendChild(createProfileElement);
    return createProfileElement;
  }

  function registerTests() {
    /** @type {?TestProfileBrowserProxy} */
    var browserProxy = null;

    /** @type {?CreateProfileElement} */
    var createProfileElement = null;

    suite('CreateProfileTests', function() {
      setup(function() {
        browserProxy = new TestProfileBrowserProxy();

        // Replace real proxy with mock proxy.
        signin.ProfileBrowserProxyImpl.instance_ = browserProxy;
        browserProxy.setIconUrls(['icon1.png', 'icon2.png']);
        browserProxy.setSignedInUsers([{username: 'username',
                                        profilePath: 'path/to/profile'}]);

        createProfileElement = createElement();

        // Make sure DOM is up to date.
        Polymer.dom.flush();
      });

      teardown(function() { createProfileElement.remove(); });

      test('Handles available profile icons', function() {
        return browserProxy.whenCalled('getAvailableIcons').then(function() {
          assertEquals('icon1.png', createProfileElement.profileIconUrl_);
        });
      });

      test('Handles signed in users', function() {
        return browserProxy.whenCalled('getSignedInUsers').then(function() {
          assertEquals(1, createProfileElement.signedInUsers_.length);
          assertEquals('username',
                       createProfileElement.signedInUsers_[0].username);
          assertEquals('path/to/profile',
                       createProfileElement.signedInUsers_[0].profilePath);

          // The dropdown menu is populated.
          Polymer.dom.flush();
          var users = createProfileElement.querySelectorAll('paper-item');
          assertEquals(1, users.length);

          assertTrue(createProfileElement.$.noSignedInUserContainer.hidden);
        });
      });

      test('Name is empty by default', function() {
        assertEquals('', createProfileElement.$.nameInput.value);
      });

      test('Create button is disabled if name is empty or invalid', function() {
        assertEquals('', createProfileElement.$.nameInput.value);
        assertTrue(createProfileElement.$.save.disabled);

        createProfileElement.$.nameInput.value = ' ';
        assertTrue(createProfileElement.$.nameInput.invalid);
        assertTrue(createProfileElement.$.save.disabled);

        createProfileElement.$.nameInput.value = 'foo';
        assertFalse(createProfileElement.$.nameInput.invalid);
        assertFalse(createProfileElement.$.save.disabled);
      });

      test('Create a profile', function() {
        // Set the text in the name field.
        createProfileElement.$.nameInput.value = 'foo';

        // Simulate clicking 'Create'.
        MockInteractions.tap(createProfileElement.$.save);

        return browserProxy.whenCalled('createProfile').then(function(args) {
          assertEquals('foo', args.profileName);
          assertEquals('icon1.png', args.profileIconUrl);
          assertFalse(args.isSupervised);
          assertEquals('path/to/profile', args.supervisorProfilePath);
        });
      });

      test('Create supervised profile', function() {
        // Set the text in the name field.
        createProfileElement.$.nameInput.value = 'foo';

        // Simulate checking the checkbox.
        MockInteractions.tap(createProfileElement.$$('paper-checkbox'));

        // Simulate clicking 'Create'.
        MockInteractions.tap(createProfileElement.$.save);

        return browserProxy.whenCalled('createProfile').then(function(args) {
          assertEquals('foo', args.profileName);
          assertEquals('icon1.png', args.profileIconUrl);
          assertTrue(args.isSupervised);
          assertEquals('path/to/profile', args.supervisorProfilePath);
        });
      });

      test('Cancel creating a profile', function() {
        // Set the text in the name field.
        createProfileElement.$.nameInput.value = 'foo';

        // Simulate clicking 'Create'.
        MockInteractions.tap(createProfileElement.$.save);

        return browserProxy.whenCalled('createProfile').then(function(args) {
          // The 'Save' button is disabled when create is in progress.
          assertTrue(createProfileElement.createInProgress_);
          assertTrue(createProfileElement.$.save.disabled);

          // Simulate clicking 'Cancel'.
          MockInteractions.tap(createProfileElement.$.cancel);
          return browserProxy.whenCalled('cancelCreateProfile').then(
              function() {
                // The 'Save' button is enabled when create is not in progress.
                assertFalse(createProfileElement.createInProgress_);
                assertFalse(createProfileElement.$.save.disabled);
              });
        });
      });

      test('Leave the page by clicking the Cancel button', function() {
        return new Promise(function(resolve, reject) {
          // Create is not in progress. We expect to leave the page.
          createProfileElement.addEventListener('change-page', function() {
            // This should not be called if create is not in progress.
            if (!browserProxy.cancelCreateProfileCalled)
              resolve();
          });

          // Simulate clicking 'Cancel'.
          MockInteractions.tap(createProfileElement.$.cancel);
        });
      });

      test('Create profile success', function() {
        return new Promise(function(resolve, reject) {
          // Create was successful. We expect to leave the page.
          createProfileElement.addEventListener('change-page', resolve);

          // Set the text in the name field.
          createProfileElement.$.nameInput.value = 'foo';

          // Simulate clicking 'Create'.
          MockInteractions.tap(createProfileElement.$.save);

          browserProxy.whenCalled('createProfile').then(function(args) {
            // The paper-spinner is active when create is in progress.
            assertTrue(createProfileElement.createInProgress_);
            assertTrue(createProfileElement.$$('paper-spinner').active);

            cr.webUIListenerCallback('create-profile-success');
            // The paper-spinner is not active when create is not in progress.
            assertFalse(createProfileElement.createInProgress_);
            assertFalse(createProfileElement.$$('paper-spinner').active);
          });
        });
      });

      test('Create profile error', function() {
        // Set the text in the name field.
        createProfileElement.$.nameInput.value = 'foo';

        // Simulate clicking 'Create'.
        MockInteractions.tap(createProfileElement.$.save);

        return browserProxy.whenCalled('createProfile').then(function(args) {
          cr.webUIListenerCallback('create-profile-error', 'Error Message');

          // Create is no longer in progress.
          assertFalse(createProfileElement.createInProgress_);
          // Error message is set.
          assertEquals('Error Message',
                       createProfileElement.$.messageBubble.innerHTML);
        });
      });

      test('Create profile warning', function() {
        // Set the text in the name field.
        createProfileElement.$.nameInput.value = 'foo';

        // Simulate clicking 'Create'.
        MockInteractions.tap(createProfileElement.$.save);

        return browserProxy.whenCalled('createProfile').then(function(args) {
          cr.webUIListenerCallback('create-profile-warning',
                                   'Warning Message');

          // Create is no longer in progress.
          assertFalse(createProfileElement.createInProgress_);
          // Warning message is set.
          assertEquals('Warning Message',
                       createProfileElement.$.messageBubble.innerHTML);
        });
      });
    });

    suite('CreateProfileTestsNoSignedInUser', function() {
      setup(function() {
        browserProxy = new TestProfileBrowserProxy();
        // Replace real proxy with mock proxy.
        signin.ProfileBrowserProxyImpl.instance_ = browserProxy;

        createProfileElement = createElement();

        // Make sure DOM is up to date.
        Polymer.dom.flush();
      });

      teardown(function() { createProfileElement.remove(); });

      test('Handles no signed in users', function() {
        return browserProxy.whenCalled('getSignedInUsers').then(function() {
          assertEquals(0, createProfileElement.signedInUsers_.length);

          // The dropdown menu is empty.
          var users = createProfileElement.querySelectorAll('paper-item');
          assertEquals(0, users.length);

          assertFalse(createProfileElement.$.noSignedInUserContainer.hidden);
        });
      });

      test('Create button is disabled', function() {
        assertTrue(createProfileElement.$.save.disabled);
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});

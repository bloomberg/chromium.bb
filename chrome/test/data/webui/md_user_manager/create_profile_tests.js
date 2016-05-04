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
        browserProxy.setDefaultProfileInfo({name: 'profile name'});
        browserProxy.setIconUrls(['icon1.png', 'icon2.png']);
        browserProxy.setSignedInUsers([{username: 'username',
                                        profilePath: 'path/to/profile'}]);
        browserProxy.setExistingSupervisedUsers([{name: 'existing name 1',
                                                  onCurrentDevice: true},
                                                 {name: 'existing name 2',
                                                  onCurrentDevice: false}]);

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

          // The 'learn more' link is visible.
          assertTrue(!!createProfileElement.$$('#learn-more > a'));

          // The dropdown menu becomes visible when the checkbox is checked.
          assertFalse(!!createProfileElement.$$('paper-dropdown-menu'));

          // Simulate checking the checkbox.
          MockInteractions.tap(createProfileElement.$$('paper-checkbox'));
          Polymer.dom.flush();

          // The dropdown menu is visible and is populated with the sentinel
          // item as well as the signed in users.
          var dropdownMenu = createProfileElement.$$('paper-dropdown-menu');
          assertTrue(!!dropdownMenu);
          var users = dropdownMenu.querySelectorAll('paper-item');
          assertEquals(2, users.length);
        });
      });

      test('Sentinel item is the initially selected item', function() {
        return browserProxy.whenCalled('getSignedInUsers').then(function() {
          // Simulate checking the checkbox.
          MockInteractions.tap(createProfileElement.$$('paper-checkbox'));
          Polymer.dom.flush();

          var dropdownMenu = createProfileElement.$$('paper-dropdown-menu');
          var selector = dropdownMenu.querySelector('paper-listbox');
          assertEquals(loadTimeData.getString('selectAnAccount'),
                       selector.selectedItem.textContent.trim());
        });
      });

      test('Name is non-empty by default', function() {
        assertEquals('profile name', createProfileElement.$.nameInput.value);
      });

      test('Create button is disabled if name is empty or invalid', function() {
        assertEquals('profile name', createProfileElement.$.nameInput.value);
        assertFalse(createProfileElement.$.nameInput.invalid);
        assertFalse(createProfileElement.$.save.disabled);

        createProfileElement.$.nameInput.value = '';
        assertTrue(createProfileElement.$.save.disabled);

        createProfileElement.$.nameInput.value = ' ';
        assertTrue(createProfileElement.$.nameInput.invalid);
        assertTrue(createProfileElement.$.save.disabled);
      });

      test('Create a profile', function() {
        // Simulate clicking 'Create'.
        MockInteractions.tap(createProfileElement.$.save);

        return browserProxy.whenCalled('createProfile').then(function(args) {
          assertEquals('profile name', args.profileName);
          assertEquals('icon1.png', args.profileIconUrl);
          assertFalse(args.isSupervised);
          assertEquals('', args.supervisedUserId);
          assertEquals('', args.custodianProfilePath);
        });
      });

      test('Has to select a custodian for the supervised profile', function() {
        // Simulate checking the checkbox.
        MockInteractions.tap(createProfileElement.$$('paper-checkbox'));
        Polymer.dom.flush();

        // Simulate clicking 'Create'.
        MockInteractions.tap(createProfileElement.$.save);

        // Create is not in progress.
        assertFalse(createProfileElement.createInProgress_);
        // Message container is visible.
        assertFalse(createProfileElement.$$('#message-container').hidden);
        // Error message is set.
        assertEquals(
            loadTimeData.getString('custodianAccountNotSelectedError'),
            createProfileElement.$.message.innerHTML);
      });

      test('Supervised profile name is duplicate (on the device)', function() {
        // Simulate checking the checkbox.
        MockInteractions.tap(createProfileElement.$$('paper-checkbox'));
        Polymer.dom.flush();

        // There is an existing supervised user with this name on the device.
        createProfileElement.$.nameInput.value = 'existing name 1';

        // Select the first signed in user.
        var dropdownMenu = createProfileElement.$$('paper-dropdown-menu');
        var selector = dropdownMenu.querySelector('paper-listbox');
        selector.selected = 0;

        // Simulate clicking 'Create'.
        MockInteractions.tap(createProfileElement.$.save);

        return browserProxy.whenCalled('getExistingSupervisedUsers').then(
            function(args) {
              // Create is not in progress.
              assertFalse(createProfileElement.createInProgress_);
              // Message container is visible.
              assertFalse(createProfileElement.$$('#message-container').hidden);
              // Error message is set.
              var message = loadTimeData.getString(
                  'managedProfilesExistingLocalSupervisedUser');
              assertEquals(message, createProfileElement.$.message.innerHTML);
            });
      });

      test('Supervised profile name is duplicate (remote)', function() {
        // Simulate checking the checkbox.
        MockInteractions.tap(createProfileElement.$$('paper-checkbox'));
        Polymer.dom.flush();

        // There is an existing supervised user with this name on the device.
        createProfileElement.$.nameInput.value = 'existing name 2';

        // Select the first signed in user.
        var dropdownMenu = createProfileElement.$$('paper-dropdown-menu');
        var selector = dropdownMenu.querySelector('paper-listbox');
        selector.selected = 0;

        // Simulate clicking 'Create'.
        MockInteractions.tap(createProfileElement.$.save);

        return browserProxy.whenCalled('getExistingSupervisedUsers').then(
            function(args) {
              // Create is not in progress.
              assertFalse(createProfileElement.createInProgress_);
              // Message container is visible.
              assertFalse(createProfileElement.$$('#message-container').hidden);
              // Error message contains a link to import the supervised user.
              var message = createProfileElement.$.message;
              assertTrue(
                  !!message.querySelector('#supervised-user-import-existing'));
            });
      });

      test('Create supervised profile', function() {
        // Simulate checking the checkbox.
        MockInteractions.tap(createProfileElement.$$('paper-checkbox'));
        Polymer.dom.flush();

        // Select the first signed in user.
        var dropdownMenu = createProfileElement.$$('paper-dropdown-menu');
        var selector = dropdownMenu.querySelector('paper-listbox');
        selector.selected = 0;

        // Simulate clicking 'Create'.
        MockInteractions.tap(createProfileElement.$.save);

        return browserProxy.whenCalled('createProfile').then(function(args) {
          assertEquals('profile name', args.profileName);
          assertEquals('icon1.png', args.profileIconUrl);
          assertTrue(args.isSupervised);
          assertEquals('', args.supervisedUserId);
          assertEquals('path/to/profile', args.custodianProfilePath);
        });
      });

      test('Cancel creating a profile', function() {
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
          createProfileElement.addEventListener('change-page', function(event) {
            // This should not be called if create is not in progress.
            if (!browserProxy.cancelCreateProfileCalled &&
                event.detail.page == 'user-pods-page') {
              resolve();
            }
          });

          // Simulate clicking 'Cancel'.
          MockInteractions.tap(createProfileElement.$.cancel);
        });
      });

      test('Create profile success', function() {
        return new Promise(function(resolve, reject) {
          // Create was successful. We expect to leave the page.
          createProfileElement.addEventListener('change-page', function(event) {
            if (event.detail.page == 'user-pods-page')
              resolve();
          });

          // Simulate clicking 'Create'.
          MockInteractions.tap(createProfileElement.$.save);

          browserProxy.whenCalled('createProfile').then(function(args) {
            // The paper-spinner is active when create is in progress.
            assertTrue(createProfileElement.createInProgress_);
            assertTrue(createProfileElement.$$('paper-spinner').active);

            cr.webUIListenerCallback('create-profile-success',
                                     {name: 'profile name',
                                      filePath: 'path/to/profile'});

            // The paper-spinner is not active when create is not in progress.
            assertFalse(createProfileElement.createInProgress_);
            assertFalse(createProfileElement.$$('paper-spinner').active);
          });
        });
      });

      test('Create supervised profile success', function() {
        return new Promise(function(resolve, reject) {
          /**
           * Profile Info of the successfully created supervised user.
           * @type {!ProfileInfo}
           */
          var profileInfo = {name: 'profile name',
                             filePath: 'path/to/profile',
                             showConfirmation: true};

          // Create was successful. We expect to leave the page.
          createProfileElement.addEventListener('change-page', function(event) {
            if (event.detail.page == 'supervised-create-confirm-page' &&
                event.detail.data == profileInfo) {
              resolve();
            }
          });

          // Simulate clicking 'Create'.
          MockInteractions.tap(createProfileElement.$.save);

          browserProxy.whenCalled('createProfile').then(function(args) {
            // The paper-spinner is active when create is in progress.
            assertTrue(createProfileElement.createInProgress_);
            assertTrue(createProfileElement.$$('paper-spinner').active);

            cr.webUIListenerCallback('create-profile-success', profileInfo);

            // The paper-spinner is not active when create is not in progress.
            assertFalse(createProfileElement.createInProgress_);
            assertFalse(createProfileElement.$$('paper-spinner').active);
          });
        });
      });

      test('Create profile error', function() {
        // Simulate clicking 'Create'.
        MockInteractions.tap(createProfileElement.$.save);

        return browserProxy.whenCalled('createProfile').then(function(args) {
          cr.webUIListenerCallback('create-profile-error', 'Error Message');

          // Create is no longer in progress.
          assertFalse(createProfileElement.createInProgress_);
          // Error message is set.
          assertEquals('Error Message',
                       createProfileElement.$.message.innerHTML);
        });
      });

      test('Create profile warning', function() {
        // Set the text in the name field.
        createProfileElement.$.nameInput.value = 'foo';

        // Simulate clicking 'Create'.
        MockInteractions.tap(createProfileElement.$.save);

        return browserProxy.whenCalled('createProfile').then(function(args) {
          cr.webUIListenerCallback('create-profile-warning', 'Warning Message');

          // Create is no longer in progress.
          assertFalse(createProfileElement.createInProgress_);
          // Warning message is set.
          assertEquals('Warning Message',
                       createProfileElement.$.message.innerHTML);
        });
      });

      test('Learn more link takes you to the correct page', function() {
        return new Promise(function(resolve, reject) {
          // Create is not in progress. We expect to leave the page.
          createProfileElement.addEventListener('change-page', function(event) {
            if (event.detail.page == 'supervised-learn-more-page')
              resolve();
          });

          // Simulate clicking 'Learn more'.
          MockInteractions.tap(createProfileElement.$$('#learn-more > a'));
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

          // Simulate checking the checkbox.
          MockInteractions.tap(createProfileElement.$$('paper-checkbox'));
          Polymer.dom.flush();

          // The dropdown menu is not visible when there are no signed in users.
          assertFalse(!!createProfileElement.$$('paper-dropdown-menu'));

          // Instead a message containing a link to the Help Center on how
          // to sign in to Chrome is displaying.
          assertTrue(!!createProfileElement.$$('#sign-in-to-chrome'));
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

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('user_manager.import_supervised_user_tests', function() {
  function registerTests() {
    suite('ImportSupervisedUserTests', function() {
      /** @type {?ImportSupervisedUserElement} */
      var importElement = null;

      /**
       * @param {!HTMLElement} element
       * @return {string}
       */
      var getProfileName = function(element) {
        return element.querySelector('.profile-name').textContent.trim();
      }

      setup(function() {
        PolymerTest.clearBody();
        importElement = document.createElement('import-supervised-user');
        document.body.appendChild(importElement);

        // Make sure DOM is up to date.
        Polymer.dom.flush();
      });

      teardown(function() { importElement.remove(); });

      test('Dialog does not show if no signed-in user is provided', function() {
        // The dialog is initially not visible.
        assertFalse(!!importElement.$$('#backdrop'));

        importElement.show(undefined, []);
        Polymer.dom.flush();

        // The dialog is still not visible.
        assertFalse(!!importElement.$$('#backdrop'));
      });

      test('Dialog shows when there are no supervised users', function() {
        // The dialog is initially not visible.
        assertFalse(!!importElement.$$('#backdrop'));

        importElement.show({username: 'username',
                            profilePath: 'path/to/profile'},
                           []);
        Polymer.dom.flush();

        // The dialog becomes visible.
        assertLT(0, importElement.$$('#backdrop').offsetHeight);

        // The correct message is displayed.
        assertEquals(loadTimeData.getString('noSupervisedUserImportText'),
                     importElement.$$('#message').textContent.trim());

        var selectorElement = importElement.$$('iron-selector');
        assertTrue(!!selectorElement);

        // There are no supervised users to choose from.
        var items = selectorElement.querySelectorAll('.list-item');
        assertEquals(0, items.length);

        // Simulate clicking 'Cancel'
        MockInteractions.tap(importElement.$$('#cancel'));

        Polymer.dom.flush();
        // The dialog is no longer visible.
        assertEquals(0, importElement.$$('#backdrop').offsetHeight);
      });

      test('Can import supervised user', function() {
        return new Promise(function(resolve, reject) {
          /** @type {!SignedInUser} */
          var signedInUser = {username: 'username',
                              profilePath: 'path/to/profile'};

          /** @type {!Array<!SupervisedUser>} */
          var supervisedUsers = [{name: 'supervised user 1',
                                  onCurrentDevice: true},
                                 {name: 'supervised user 3',
                                  onCurrentDevice: false},
                                 {name: 'supervised user 2',
                                  onCurrentDevice: false}];

          // Expect an event to import the selected supervised user to be fired.
          importElement.addEventListener('import', function(event) {
            if (event.detail.signedInUser == signedInUser &&
                event.detail.supervisedUser.name == 'supervised user 2') {
              Polymer.dom.flush();
              // The dialog is no longer visible.
              assertEquals(0, importElement.$$('#backdrop').offsetHeight);

              resolve();
            }
          });

          // The dialog is initially not visible.
          assertFalse(!!importElement.$$('#backdrop'));

          importElement.show(signedInUser, supervisedUsers);
          Polymer.dom.flush();

          // The dialog becomes visible.
          assertLT(0, importElement.$$('#backdrop').offsetHeight);

          // The correct message is displayed.
          assertEquals(loadTimeData.getString('supervisedUserImportText'),
                       importElement.$$('#message').textContent.trim());

          var selectorElement = importElement.$$('iron-selector');
          assertTrue(!!selectorElement);

          // Supervised users are ordered correctly (Ones that are not on the
          // current device appear first, then they are alphabetically order in
          // ascending order).
          var items = selectorElement.querySelectorAll('.list-item');
          assertEquals(3, items.length);
          assertEquals('supervised user 2', getProfileName(items[0]));
          assertEquals('supervised user 3', getProfileName(items[1]));
          assertEquals('supervised user 1', getProfileName(items[2]));

          // Only supervised users that are not on this device are selectable.
          var selectableItems = selectorElement.querySelectorAll('.selectable');
          assertEquals(2, selectableItems.length);
          assertEquals('supervised user 2', getProfileName(selectableItems[0]));
          assertEquals('supervised user 3', getProfileName(selectableItems[1]));

          // No user is initially selected.
          assertEquals(-1, selectorElement.selected);
          // The import button is disabled if no supervised user is selected.
          assertTrue(importElement.$$('#import').disabled);

          // Simulate selecting the third user which is not selectable.
          MockInteractions.tap(items[2]);
          // Confirm no user is selected.
          assertEquals(-1, selectorElement.selected);

          // Simulate selecting the first user.
          MockInteractions.tap(items[0]);
          // Confirm the user is selected.
          assertEquals(0, selectorElement.selected);
          // The import button becomes enabled once a user is selected.
          assertFalse(importElement.$$('#import').disabled);

          // Simulate clicking 'Import'.
          MockInteractions.tap(importElement.$$('#import'));
        });
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});

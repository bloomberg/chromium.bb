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
        importElement = document.createElement('import-supervised-user');
        document.body.appendChild(importElement);

        // Make sure DOM is up to date.
        Polymer.dom.flush();
      });

      teardown(function(done) {
        importElement.remove();
        // Allow asynchronous tasks to finish.
        setTimeout(done);
      });

      test('Dialog does not show if no signed-in user is provided', function() {
        // The dialog is initially not visible.
        assertFalse(importElement.$.dialog.opened);

        importElement.show(undefined, []);
        Polymer.dom.flush();

        // The dialog is still not visible.
        assertFalse(importElement.$.dialog.opened);
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
              assertFalse(importElement.$.dialog.opened);

              resolve();
            }
          });

          // The dialog is initially not visible.
          assertFalse(importElement.$.dialog.opened);

          importElement.show(signedInUser, supervisedUsers);
          Polymer.dom.flush();

          // The dialog becomes visible.
          assertTrue(importElement.$.dialog.opened);

          // The correct message is displayed.
          assertEquals(loadTimeData.getString('supervisedUserImportText'),
                       importElement.$$('#message').textContent.trim());

          var selectorElement = importElement.$$('paper-listbox');
          assertTrue(!!selectorElement);

          // Supervised users are ordered correctly (Ones that are not on the
          // current device appear first, then they are alphabetically order in
          // ascending order).
          var items = selectorElement.querySelectorAll('paper-item');
          assertEquals(3, items.length);
          assertEquals('supervised user 2', getProfileName(items[0]));
          assertEquals('supervised user 3', getProfileName(items[1]));
          assertEquals('supervised user 1', getProfileName(items[2]));

          // Supervised users that are on this device are disabled.
          var selectableItems = selectorElement.querySelectorAll('[disabled]');
          assertEquals(1, selectableItems.length);
          assertEquals('supervised user 1', getProfileName(selectableItems[0]));

          // No user is initially selected.
          assertEquals(-1, selectorElement.selected);
          // The import button is disabled if no supervised user is selected.
          assertTrue(importElement.$$('#import').disabled);

          // Simulate selecting the third user which is disabled.
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

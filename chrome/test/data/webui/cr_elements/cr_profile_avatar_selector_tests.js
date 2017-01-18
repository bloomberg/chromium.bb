// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for cr-profile-avatar-selector. */
cr.define('cr_profile_avatar_selector', function() {
  var TestNames = {
    Basic: 'basic',
    Focus: 'Ignores modified key events',
  };

  function registerTests() {
    suite('cr-profile-avatar-selector', function() {
      /** @type {CrProfileAvatarSelectorElement} */
      var avatarSelector = null;

      /** @return {CrProfileAvatarSelectorElement} */
      function createElement() {
        var avatarSelector =
            document.createElement('cr-profile-avatar-selector');
        avatarSelector.avatars =
            [{url: 'chrome://avatar1.png', label: 'avatar1'},
             {url: 'chrome://avatar2.png', label: 'avatar2'},
             {url: 'chrome://avatar3.png', label: 'avatar3'}];
        return avatarSelector;
      }

      setup(function() {
        avatarSelector = createElement();
        document.body.appendChild(avatarSelector);
        Polymer.dom.flush();
      });

      teardown(function() {
        avatarSelector.remove();
      });

      // Run in CrElementsProfileAvatarSelectorTest.All as a browser_test.
      suite(TestNames.Basic, function() {

        test('Displays avatars', function() {
          assertEquals(3, avatarSelector.$['avatar-grid'].items.length);
        });

        test('Can update avatars', function() {
          avatarSelector.pop('avatars');
          Polymer.dom.flush();
          assertEquals(2, avatarSelector.$['avatar-grid'].items.length);
        });

        test('No avatar is initially selected', function() {
          var selector = avatarSelector.$['avatar-grid'];

          assertFalse(!!avatarSelector.selectedAvatarUrl);
          assertFalse(selector.items[0].classList.contains('iron-selected'));
          assertFalse(selector.items[1].classList.contains('iron-selected'));
          assertFalse(selector.items[2].classList.contains('iron-selected'));
        });

        test('An avatar is initially selected if selectedAvatarUrl is set',
             function() {
          var anotherAvatarSelector = createElement();
          anotherAvatarSelector.selectedAvatarUrl =
              anotherAvatarSelector.avatars[0].url;
          document.body.appendChild(anotherAvatarSelector);
          Polymer.dom.flush();

          var selector = anotherAvatarSelector.$['avatar-grid'];

          assertEquals('chrome://avatar1.png',
                       anotherAvatarSelector.selectedAvatarUrl);
          assertTrue(selector.items[0].classList.contains('iron-selected'));
          assertFalse(selector.items[1].classList.contains('iron-selected'));
          assertFalse(selector.items[2].classList.contains('iron-selected'));
        });

        test('Can select avatar', function() {
          var selector = avatarSelector.$['avatar-grid'];

          // Simulate tapping the third avatar.
          MockInteractions.tap(selector.items[2]);
          assertEquals(
              'chrome://avatar3.png', avatarSelector.selectedAvatarUrl);
          assertFalse(selector.items[0].classList.contains('iron-selected'));
          assertFalse(selector.items[1].classList.contains('iron-selected'));
          assertTrue(selector.items[2].classList.contains('iron-selected'));
        });

        test('Fires iron-activate event when an avatar is activated',
             function(done) {
          avatarSelector.addEventListener('iron-activate',
                                          function(event) {
            assertEquals(event.detail.selected, 'chrome://avatar2.png');
            done();
          });

          // Simulate tapping the second avatar.
          MockInteractions.tap(avatarSelector.$['avatar-grid'].items[1]);
        });
      });

      // Run in CrElementsProfileAvatarSelectorFocusTest.All as an
      // interactive_ui_test.
      test(TestNames.Focus, function() {
        var selector = avatarSelector.$['avatar-grid'];
        var items = selector.items;

        selector._setFocusedItem(items[0]);
        assertTrue(items[0].focused);

        MockInteractions.keyDownOn(items[0], 39, [], 'ArrowRight');
        assertTrue(items[1].focused);

        MockInteractions.keyDownOn(items[0], 37, [], 'ArrowLeft');
        assertTrue(items[0].focused);

        avatarSelector.ignoreModifiedKeyEvents = true;

        MockInteractions.keyDownOn(items[0], 39, 'alt', 'ArrowRight');
        assertTrue(items[0].focused);

        MockInteractions.keyDownOn(items[0], 39, 'ctrl', 'ArrowRight');
        assertTrue(items[0].focused);

        MockInteractions.keyDownOn(items[0], 39, 'meta', 'ArrowRight');
        assertTrue(items[0].focused);

        MockInteractions.keyDownOn(items[0], 39, 'shift', 'ArrowRight');
        assertTrue(items[0].focused);
      });
    });
  }

  return {
    registerTests: registerTests,
    TestNames: TestNames,
  };
});

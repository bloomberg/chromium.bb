// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_people_page_manage_profile', function() {
  /**
   * @constructor
   * @implements {settings.ManageProfileBrowserProxy}
   * @extends {settings.TestBrowserProxy}
   */
  var TestManageProfileBrowserProxy = function() {
    settings.TestBrowserProxy.call(this, [
      'getAvailableIcons',
      'setProfileIconAndName',
    ]);
  };

  TestManageProfileBrowserProxy.prototype = {
    __proto__: settings.TestBrowserProxy.prototype,

    /** @override */
    getAvailableIcons: function() {
      this.methodCalled('getAvailableIcons');
      return Promise.resolve([{url: 'fake-icon-1.png', label: 'fake-icon-1'},
                              {url: 'fake-icon-2.png', label: 'fake-icon-2'}]);
    },

    /** @override */
    setProfileIconAndName: function(iconUrl, name) {
      this.methodCalled('setProfileIconAndName', [iconUrl, name]);
    },
  };

  function registerManageProfileTests() {
    suite('ManageProfileTests', function() {
      var manageProfile = null;
      var browserProxy = null;

      setup(function() {
        browserProxy = new TestManageProfileBrowserProxy();
        settings.ManageProfileBrowserProxyImpl.instance_ = browserProxy;
        PolymerTest.clearBody();
        manageProfile = document.createElement('settings-manage-profile');
        manageProfile.profileIconUrl = 'fake-icon-1.png';
        manageProfile.profileName = 'Initial Fake Name';
        document.body.appendChild(manageProfile);
      });

      teardown(function() { manageProfile.remove(); });

      // Tests that the manage profile subpage
      //  - gets and receives all the available icons
      //  - has the correct icon selected
      //  - can select a new icon
      test('ManageProfileChangeIcon', function() {
        var selector = manageProfile.$.selector.$.selector;
        assertTrue(!!selector);

        return browserProxy.whenCalled('getAvailableIcons').then(function() {
          Polymer.dom.flush();

          assertEquals('fake-icon-1.png', manageProfile.profileIconUrl);
          assertEquals(2, selector.items.length);
          assertTrue(selector.items[0].classList.contains('iron-selected'));
          assertFalse(selector.items[1].classList.contains('iron-selected'));

          MockInteractions.tap(selector.items[1]);
          return browserProxy.whenCalled('setProfileIconAndName').then(
              function(args) {
                assertEquals('fake-icon-2.png', args[0]);
                assertEquals('Initial Fake Name', args[1]);
              });
        });
      });

      // Tests profile icon updates pushed from the browser.
      test('ManageProfileIconUpdated', function() {
        var selector = manageProfile.$.selector.$.selector;
        assertTrue(!!selector);

        return browserProxy.whenCalled('getAvailableIcons').then(function() {
          manageProfile.profileIconUrl = 'fake-icon-2.png';

          Polymer.dom.flush();

          assertEquals('fake-icon-2.png', manageProfile.profileIconUrl);
          assertEquals(2, selector.items.length);
          assertFalse(selector.items[0].classList.contains('iron-selected'));
          assertTrue(selector.items[1].classList.contains('iron-selected'));
        });
      });

      test('ManageProfileChangeName', function() {
        var nameField = manageProfile.$.name;
        assertTrue(!!nameField);

        assertEquals('Initial Fake Name', nameField.value);

        nameField.value = 'New Name';
        nameField.fire('change');

        return browserProxy.whenCalled('setProfileIconAndName').then(
            function(args) {
              assertEquals('fake-icon-1.png', args[0]);
              assertEquals('New Name', args[1]);
            });
      });

      // Tests profile name updates pushed from the browser.
      test('ManageProfileNameUpdated', function() {
        var nameField = manageProfile.$.name;
        assertTrue(!!nameField);

        return browserProxy.whenCalled('getAvailableIcons').then(function() {
          manageProfile.profileName = 'New Name From Browser';

          Polymer.dom.flush();

          assertEquals('New Name From Browser', nameField.value);
        });
      });
    });
  }

  return {
    registerTests: function() {
      registerManageProfileTests();
    },
  };
});

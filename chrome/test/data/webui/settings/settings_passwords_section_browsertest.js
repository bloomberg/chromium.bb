// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Password Settings tests. */

/** @const {string} Path to root from chrome/test/data/webui/settings/. */
var ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);

/**
 * @constructor
 * @extends {PolymerTest}
 */
function SettingsPasswordSectionBrowserTest() {}

SettingsPasswordSectionBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  browsePreload:
      'chrome://md-settings/passwords_and_forms_page/passwords_section.html',

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH),
};

/**
 * This test will validate that the section is loaded with data.
 */
TEST_F('SettingsPasswordSectionBrowserTest', 'uiTests', function() {
  var self = this;

  suite('PasswordsSection', function() {
    test('doWork', function(done) {
      assertEquals(self.browsePreload, document.location.href,
                   'Unexpected URL loaded');

      var passwordList = [{origin: 'anotherwebsite.com',
                           username: 'luigi',
                           password: '*******'},
                          {origin: 'longwebsite.com',
                           username: 'peach',
                           password: '***'},
                          {origin: 'website.com',
                           username: 'mario',
                           password: '*******'}];

      // Create a passwords-section to use for testing.
      var passwordsSection = document.createElement('passwords-section');
      passwordsSection.savedPasswords = passwordList;
      document.body.appendChild(passwordsSection);

      // TODO(hcarmona): use an event listener rather than a setTimeout(0).
      window.setTimeout(function() {
        // Assert that the data is passed into the iron list. If this fails,
        // then other expectations will also fail.
        assertEquals(passwordList, passwordsSection.$.passwordList.items,
                     'Failed to pass list of passwords to iron-list');

        var list = Polymer.dom(passwordsSection.$.passwordList);
        assertTrue(!!list, "Failed to get the password list");
        // Skip the first child because it's the template.
        var listChildren = list.children.slice(1);

        var validate = function(nodes, passwordList, index) {
          assertTrue(!!nodes[index], 'Failed to get nodes[' + index + ']');
          assertEquals(passwordList[index].origin,
                       nodes[index].querySelector('#origin').textContent,
                       'origin mismatch in nodes[' + index + ']');
          assertEquals(passwordList[index].username,
                       nodes[index].querySelector('#username').textContent,
                       'username mismatch in nodes[' + index + ']');
          assertEquals(passwordList[index].password,
                       nodes[index].querySelector('#password').textContent,
                       'password mismatch in nodes[' + index + ']');
        };

        validate(listChildren, passwordList, 0);
        validate(listChildren, passwordList, 1);
        validate(listChildren, passwordList, 2);
        done();
      }, 0);
    });
  });

  mocha.run();
});

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

  /** @override */
  setUp: function() {
    PolymerTest.prototype.setUp.call(this);

    // Test is run on an individual element that won't have a page language.
    this.accessibilityAuditConfig.auditRulesToIgnore.push('humanLangMissing');
  },

  /**
   * Creates a single item for the list of passwords.
   * @param {string} url
   * @param {string} username
   * @param {number} passwordLength
   * @return {chrome.passwordsPrivate.PasswordUiEntry}
   * @private
   */
  createPasswordItem_: function(url, username, passwordLength) {
    return {
      loginPair: {originUrl: url, username: username},
      numCharactersInPassword: passwordLength
    };
  },

  /**
   * Helper method that validates a node matches the data for an index.
   * @param {!Array<!Element>} nodes The nodes that will be checked.
   * @param {!Array<!Object>} passwordList The expected data.
   * @param {number} index The index that should match the node and data.
   * @private
   */
  validate_: function(nodes, passwordList, index) {
    var node = nodes[index];
    var passwordInfo = passwordList[index];
    assertTrue(!!node, 'Failed to get nodes[' + index + ']');
    assertTrue(!!passwordInfo, 'Failed to get passwordList[' + index + ']');
    assertEquals(
        passwordInfo.loginPair.originUrl,
        node.querySelector('#originUrl').textContent,
        'originUrl mismatch in nodes[' + index + ']');
    assertEquals(
        passwordInfo.loginPair.username,
        node.querySelector('#username').textContent,
        'username mismatch in nodes[' + index + ']');
    assertEquals(
        passwordInfo.numCharactersInPassword,
        node.querySelector('#password').value.length,
        'password size mismatch in nodes[' + index + ']');
  },
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

      var passwordList = [
        self.createPasswordItem_('anotherwebsite.com', 'luigi', 1),
        self.createPasswordItem_('longwebsite.com', 'peach', 7),
        self.createPasswordItem_('website.com', 'mario', 70)
      ];

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
        assertTrue(!!list, 'Failed to get the password list');
        // Skip the first child because it's the template.
        var listChildren = list.children.slice(1);

        self.validate_(listChildren, passwordList, 0);
        self.validate_(listChildren, passwordList, 1);
        self.validate_(listChildren, passwordList, 2);
        done();
      }, 0);
    });
  });

  mocha.run();
});

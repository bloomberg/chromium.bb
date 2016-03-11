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
   * Helper method that validates a that elements in the password list match
   * the expected data.
   * @param {!Array<!Element>} nodes The nodes that will be checked.
   * @param {!Array<!Object>} passwordList The expected data.
   * @private
   */
  validatePasswordList: function(nodes, passwordList) {
    assertEquals(passwordList.length, nodes.length);
    for (var index = 0; index < passwordList.length; ++index) {
      var node = nodes[index];
      var passwordInfo = passwordList[index];
      assertEquals(passwordInfo.loginPair.originUrl,
                   node.querySelector('#originUrl').textContent);
      assertEquals(passwordInfo.loginPair.username,
                   node.querySelector('#username').textContent);
      assertEquals(passwordInfo.numCharactersInPassword,
                   node.querySelector('#password').value.length);
    }
  },

  /**
   * Helper method that validates a that elements in the exception list match
   * the expected data.
   * @param {!Array<!Element>} nodes The nodes that will be checked.
   * @param {!Array<!Object>} exceptionList The expected data.
   * @private
   */
  validateExceptionList_: function(nodes, exceptionList) {
    assertEquals(exceptionList.length, nodes.length);
    for (var index = 0; index < exceptionList.length; ++index) {
      var node = nodes[index];
      var exception = exceptionList[index];
      assertEquals(exception, node.querySelector('#exception').textContent);
    }
  },

  /**
   * Skips over the template and returns all visible children of an iron-list.
   * @param {!Element} ironList
   * @return {!Array<!Element>}
   * @private
   */
  getIronListChildren_: function(ironList) {
    return Polymer.dom(ironList).children.filter(function(child, i) {
      return i != 0 && !child.hidden;
    });
  },

  /**
   * Helper method used to create a password section for the given lists.
   * @param {!Array<!chrome.passwordsPrivate.PasswordUiEntry>} passwordList
   * @param {!Array<!string>} exceptionList
   * @return {!Object}
   * @private
   */
  createPasswordsSection_: function(passwordList, exceptionList) {
    // Create a passwords-section to use for testing.
    var passwordsSection = document.createElement('passwords-section');
    passwordsSection.savedPasswords = passwordList;
    passwordsSection.passwordExceptions = exceptionList;
    document.body.appendChild(passwordsSection);

    // Allow polymer binding to finish.
    Polymer.dom.flush();

    return passwordsSection;
  },

  /**
   * Helper method used to test for a url in a list of passwords.
   * @param {!Array<!chrome.passwordsPrivate.PasswordUiEntry>} passwordList
   * @param {!String} url The URL that is being searched for.
   */
  listContainsUrl(passwordList, url) {
    for (var i = 0; i < passwordList.length; ++i) {
      if (passwordList[i].loginPair.originUrl == url)
        return true;
    }
    return false;
  },
};

/**
 * This test will validate that the section is loaded with data.
 */
TEST_F('SettingsPasswordSectionBrowserTest', 'uiTests', function() {
  var self = this;

  suite('PasswordsSection', function() {
    test('verifySavedPasswordLength', function() {
      assertEquals(self.browsePreload, document.location.href);

      var passwordList = [
        self.createPasswordItem_('site1.com', 'luigi', 1),
        self.createPasswordItem_('longwebsite.com', 'peach', 7),
        self.createPasswordItem_('site2.com', 'mario', 70),
        self.createPasswordItem_('site1.com', 'peach', 11),
        self.createPasswordItem_('google.com', 'mario', 7),
        self.createPasswordItem_('site2.com', 'luigi', 8),
      ];

      var passwordsSection = self.createPasswordsSection_(passwordList, []);

      // Assert that the data is passed into the iron list. If this fails,
      // then other expectations will also fail.
      assertEquals(passwordList, passwordsSection.$.passwordList.items);

      self.validatePasswordList(
          self.getIronListChildren_(passwordsSection.$.passwordList),
          passwordList);
    });

    // Test verifies that removing a password will update the elements.
    test('verifyPasswordListRemove', function() {
      var passwordList = [
        self.createPasswordItem_('anotherwebsite.com', 'luigi', 1),
        self.createPasswordItem_('longwebsite.com', 'peach', 7),
        self.createPasswordItem_('website.com', 'mario', 70)
      ];

      var passwordsSection = self.createPasswordsSection_(passwordList, []);

      self.validatePasswordList(
          self.getIronListChildren_(passwordsSection.$.passwordList),
          passwordList);
      // Simulate 'longwebsite.com' being removed from the list.
      passwordsSection.splice('savedPasswords', 1, 1);
      Polymer.dom.flush();

      assertFalse(self.listContainsUrl(passwordsSection.savedPasswords,
                                       'longwebsite.com'));
      assertFalse(self.listContainsUrl(passwordList, 'longwebsite.com'));

      self.validatePasswordList(
          self.getIronListChildren_(passwordsSection.$.passwordList),
          passwordList);
    });

    // Test verifies that pressing the 'remove' button will trigger a remove
    // event. Does not actually remove any passwords.
    test('verifyPasswordItemRemoveButton', function(done) {
      var passwordList = [
        self.createPasswordItem_('one', 'six', 5),
        self.createPasswordItem_('two', 'five', 3),
        self.createPasswordItem_('three', 'four', 1),
        self.createPasswordItem_('four', 'three', 2),
        self.createPasswordItem_('five', 'two', 4),
        self.createPasswordItem_('six', 'one', 6),
      ];

      var passwordsSection = self.createPasswordsSection_(passwordList, []);

      var passwords =
          self.getIronListChildren_(passwordsSection.$.passwordList);

      // The index of the button currently being checked.
      var index = 0;

      var clickRemoveButton = function() {
        passwords[index].querySelector('#passwordMenu').click();
        passwordsSection.$.menuRemovePassword.click();
      };

      // Listen for the remove event. If this event isn't received, the test
      // will time out and fail.
      passwordsSection.addEventListener('remove-saved-password',
                                        function(event) {
        // Verify that the event matches the expected value.
        assertTrue(index < passwordList.length);
        assertEquals(passwordList[index].loginPair.originUrl,
                     event.detail.originUrl);
        assertEquals(passwordList[index].loginPair.username,
                     event.detail.username);

        if (++index < passwordList.length) {
          // Click 'remove' on all passwords, one by one.
          window.setTimeout(clickRemoveButton, 0);
        } else {
          done();
        }
      });

      // Start removing.
      clickRemoveButton();
    });

    test('verifyPasswordExceptions', function() {
      var exceptionList = [
        'docs.google.com',
        'mail.com',
        'google.com',
        'inbox.google.com',
        'maps.google.com',
        'plus.google.com',
      ];

      var passwordsSection = self.createPasswordsSection_([], exceptionList);

      // Assert that the data is passed into the iron list. If this fails,
      // then other expectations will also fail.
      assertEquals(exceptionList,
                   passwordsSection.$.passwordExceptionsList.items);

      self.validateExceptionList_(
          self.getIronListChildren_(passwordsSection.$.passwordExceptionsList),
          exceptionList);
    });

    // Test verifies that removing an exception will update the elements.
    test('verifyPasswordExceptionRemove', function() {
      var exceptionList = [
        'docs.google.com',
        'mail.com',
        'google.com',
        'inbox.google.com',
        'maps.google.com',
        'plus.google.com',
      ];

      var passwordsSection = self.createPasswordsSection_([], exceptionList);

      self.validateExceptionList_(
          self.getIronListChildren_(passwordsSection.$.passwordExceptionsList),
          exceptionList);

      // Simulate 'mail.com' being removed from the list.
      passwordsSection.splice('passwordExceptions', 1, 1);
      assertEquals(-1, passwordsSection.passwordExceptions.indexOf('mail.com'));
      assertEquals(-1, exceptionList.indexOf('mail.com'));
      Polymer.dom.flush();

      self.validateExceptionList_(
          self.getIronListChildren_(passwordsSection.$.passwordExceptionsList),
          exceptionList);
    });

    // Test verifies that pressing the 'remove' button will trigger a remove
    // event. Does not actually remove any exceptions.
    test('verifyPasswordExceptionRemoveButton', function(done) {
      var exceptionList = [
        'docs.google.com',
        'mail.com',
        'google.com',
        'inbox.google.com',
        'maps.google.com',
        'plus.google.com',
      ];

      var passwordsSection = self.createPasswordsSection_([], exceptionList);

      var exceptions =
          self.getIronListChildren_(passwordsSection.$.passwordExceptionsList);

      // The index of the button currently being checked.
      var index = 0;

      var clickRemoveButton = function() {
        exceptions[index].querySelector('#removeExceptionButton').click();
      };

      // Listen for the remove event. If this event isn't received, the test
      // will time out and fail.
      passwordsSection.addEventListener('remove-password-exception',
                                        function(event) {
        // Verify that the event matches the expected value.
        assertTrue(index < exceptionList.length);
        assertEquals(exceptionList[index], event.detail);

        if (++index < exceptionList.length)
          clickRemoveButton();  // Click 'remove' on all passwords, one by one.
        else
          done();
      });

      // Start removing.
      clickRemoveButton();
    });
  });

  mocha.run();
});

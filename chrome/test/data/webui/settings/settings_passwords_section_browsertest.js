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
      linkUrl: 'http://' + url + '/login',
      numCharactersInPassword: passwordLength
    };
  },

  /**
   * Creates a single item for the list of password exceptions.
   * @param {string} url
   * @return {chrome.passwordsPrivate.ExceptionPair}
   * @private
   */
  createExceptionItem_: function(url) {
    return {
      exceptionUrl: url,
      linkUrl: 'http://' + url + '/login',
    };
  },

  /**
   * Helper method that validates a that elements in the password list match
   * the expected data.
   * @param {!Array<!Element>} nodes The nodes that will be checked.
   * @param {!Array<!chrome.passwordsPrivate.PasswordUiEntry>} passwordList The
   *     expected data.
   * @private
   */
  validatePasswordList: function(nodes, passwordList) {
    assertEquals(passwordList.length, nodes.length);
    for (var index = 0; index < passwordList.length; ++index) {
      var node = nodes[index];
      var passwordInfo = passwordList[index];
      assertEquals(passwordInfo.loginPair.originUrl,
                   node.querySelector('#originUrl').textContent);
      assertEquals(passwordInfo.linkUrl,
                   node.querySelector('#originUrl').href);
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
   * @param {!Array<!chrome.passwordsPrivate.ExceptionPair>} exceptionList The
   *     expected data.
   * @private
   */
  validateExceptionList_: function(nodes, exceptionList) {
    assertEquals(exceptionList.length, nodes.length);
    for (var index = 0; index < exceptionList.length; ++index) {
      var node = nodes[index];
      var exception = exceptionList[index];
      assertEquals(exception.exceptionUrl,
                   node.querySelector('#exception').textContent);
      assertEquals(exception.linkUrl,
                   node.querySelector('#exception').href);
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
   * @param {!Array<!chrome.passwordsPrivate.ExceptionPair>} exceptionList
   * @return {!Object}
   * @private
   */
  createPasswordsSection_: function(passwordList, exceptionList) {
    // Create a passwords-section to use for testing.
    var passwordsSection = document.createElement('passwords-section');
    passwordsSection.savedPasswords = passwordList;
    passwordsSection.passwordExceptions = exceptionList;
    document.body.appendChild(passwordsSection);
    this.flushPasswordSection_(passwordsSection);
    return passwordsSection;
  },

  /**
   * Helper method used to create a password editing dialog.
   * @param {!chrome.passwordsPrivate.PasswordUiEntry} passwordItem
   * @return {!Object}
   * @private
   */
  createPasswordDialog_: function(passwordItem) {
    var passwordDialog = document.createElement('password-edit-dialog');
    passwordDialog.item = passwordItem;
    document.body.appendChild(passwordDialog);
    Polymer.dom.flush();
    return passwordDialog;
  },

  /**
   * Helper method used to test for a url in a list of passwords.
   * @param {!Array<!chrome.passwordsPrivate.PasswordUiEntry>} passwordList
   * @param {!string} url The URL that is being searched for.
   */
  listContainsUrl(passwordList, url) {
    for (var i = 0; i < passwordList.length; ++i) {
      if (passwordList[i].loginPair.originUrl == url)
        return true;
    }
    return false;
  },

  /**
   * Helper method used to test for a url in a list of passwords.
   * @param {!Array<!chrome.passwordsPrivate.ExceptionPair>} exceptionList
   * @param {!string} url The URL that is being searched for.
   */
  exceptionsListContainsUrl(exceptionList, url) {
    for (var i = 0; i < exceptionList.length; ++i) {
      if (exceptionList[i].exceptionUrl == url)
        return true;
    }
    return false;
  },

  /**
   * Allow the iron-list to be sized properly.
   * @param {!Object} passwordsSection
   * @private
   */
  flushPasswordSection_: function(passwordsSection) {
    passwordsSection.$.passwordList.notifyResize();
    passwordsSection.$.passwordExceptionsList.notifyResize();
    Polymer.dom.flush();
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
      self.flushPasswordSection_(passwordsSection);

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
        MockInteractions.tap(passwords[index].querySelector('#passwordMenu'));
        MockInteractions.tap(passwordsSection.$.menuRemovePassword);
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
        self.createExceptionItem_('docs.google.com'),
        self.createExceptionItem_('mail.com'),
        self.createExceptionItem_('google.com'),
        self.createExceptionItem_('inbox.google.com'),
        self.createExceptionItem_('maps.google.com'),
        self.createExceptionItem_('plus.google.com'),
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
        self.createExceptionItem_('docs.google.com'),
        self.createExceptionItem_('mail.com'),
        self.createExceptionItem_('google.com'),
        self.createExceptionItem_('inbox.google.com'),
        self.createExceptionItem_('maps.google.com'),
        self.createExceptionItem_('plus.google.com'),
      ];

      var passwordsSection = self.createPasswordsSection_([], exceptionList);

      self.validateExceptionList_(
          self.getIronListChildren_(passwordsSection.$.passwordExceptionsList),
          exceptionList);

      // Simulate 'mail.com' being removed from the list.
      passwordsSection.splice('passwordExceptions', 1, 1);
      assertFalse(self.exceptionsListContainsUrl(
            passwordsSection.passwordExceptions, 'mail.com'));
      assertFalse(self.exceptionsListContainsUrl(exceptionList, 'mail.com'));
      self.flushPasswordSection_(passwordsSection);

      self.validateExceptionList_(
          self.getIronListChildren_(passwordsSection.$.passwordExceptionsList),
          exceptionList);
    });

    // Test verifies that pressing the 'remove' button will trigger a remove
    // event. Does not actually remove any exceptions.
    test('verifyPasswordExceptionRemoveButton', function(done) {
      var exceptionList = [
        self.createExceptionItem_('docs.google.com'),
        self.createExceptionItem_('mail.com'),
        self.createExceptionItem_('google.com'),
        self.createExceptionItem_('inbox.google.com'),
        self.createExceptionItem_('maps.google.com'),
        self.createExceptionItem_('plus.google.com'),
      ];

      var passwordsSection = self.createPasswordsSection_([], exceptionList);

      var exceptions =
          self.getIronListChildren_(passwordsSection.$.passwordExceptionsList);

      // The index of the button currently being checked.
      var index = 0;

      var clickRemoveButton = function() {
        MockInteractions.tap(
            exceptions[index].querySelector('#removeExceptionButton'));
      };

      // Listen for the remove event. If this event isn't received, the test
      // will time out and fail.
      passwordsSection.addEventListener('remove-password-exception',
                                        function(event) {
        // Verify that the event matches the expected value.
        assertTrue(index < exceptionList.length);
        assertEquals(exceptionList[index].exceptionUrl, event.detail);

        if (++index < exceptionList.length)
          clickRemoveButton();  // Click 'remove' on all passwords, one by one.
        else
          done();
      });

      // Start removing.
      clickRemoveButton();
    });

    test('usePasswordDialogTwice', function() {
      var BLANK_PASSWORD = '       ';
      var item = self.createPasswordItem_('google.com', 'homer',
                                          BLANK_PASSWORD.length);
      var passwordDialog = self.createPasswordDialog_(item);

      passwordDialog.open();
      Polymer.dom.flush();

      assertEquals(item.loginPair.originUrl,
                   passwordDialog.$.websiteInput.value);
      assertEquals(item.loginPair.username,
                   passwordDialog.$.usernameInput.value);
      assertEquals(BLANK_PASSWORD,
                   passwordDialog.$.passwordInput.value);
      // Password should NOT be visible.
      assertEquals('password',
                   passwordDialog.$.passwordInput.type);

      passwordDialog.close();
      Polymer.dom.flush();

      var blankPassword2 = ' '.repeat(17);
      var item2 = self.createPasswordItem_('drive.google.com', 'marge',
                                           blankPassword2.length);

      passwordDialog.item = item2;
      passwordDialog.open();
      Polymer.dom.flush();

      assertEquals(item2.loginPair.originUrl,
                   passwordDialog.$.websiteInput.value);
      assertEquals(item2.loginPair.username,
                   passwordDialog.$.usernameInput.value);
      assertEquals(blankPassword2,
                   passwordDialog.$.passwordInput.value);
      // Password should NOT be visible.
      assertEquals('password',
                   passwordDialog.$.passwordInput.type);
    });

    test('showSavedPassword', function() {
      var PASSWORD = 'bAn@n@5';
      var item = self.createPasswordItem_('goo.gl', 'bart', PASSWORD.length);
      var passwordDialog = self.createPasswordDialog_(item);

      passwordDialog.open();
      Polymer.dom.flush();

      passwordDialog.password = PASSWORD;
      passwordDialog.showPassword = true;

      Polymer.dom.flush();

      assertEquals(PASSWORD,
                   passwordDialog.$.passwordInput.value);
      // Password should be visible.
      assertEquals('text',
                   passwordDialog.$.passwordInput.type);
    });

    // Test will timeout if event is not received.
    test('onShowSavedPassword', function(done) {
      var item = self.createPasswordItem_('goo.gl', 'bart', 1);
      var passwordDialog = self.createPasswordDialog_(item);

      passwordDialog.open();
      Polymer.dom.flush();

      passwordDialog.addEventListener('show-password', function(event) {
        assertEquals(item.loginPair.originUrl, event.detail.originUrl);
        assertEquals(item.loginPair.username, event.detail.username);
        done();
      });

      MockInteractions.tap(passwordDialog.$.showPasswordButton);
    });
  });

  mocha.run();
});

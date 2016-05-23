// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Passwords and Forms tests. */

/** @const {string} Path to root from chrome/test/data/webui/settings/. */
var ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);

// Fake data generator.
GEN_INCLUDE([ROOT_PATH +
    'chrome/test/data/webui/settings/passwords_and_autofill_fake_data.js']);

function PasswordManagerExpectations() {};
PasswordManagerExpectations.prototype = {
  requested: {
    passwords: 0,
    exceptions: 0,
    plaintextPassword: 0,
  },

  removed: {
    passwords: 0,
    exceptions: 0,
  },

  listening: {
    passwords: 0,
    exceptions: 0,
  },
};

/**
 * Test implementation
 * @implements {PasswordManager}
 * @constructor
 */
function TestPasswordManager() {
  this.actual_ = new PasswordManagerExpectations();
};
TestPasswordManager.prototype = {
  /** @override */
  addSavedPasswordListChangedListener: function(listener) {
    this.actual_.listening.passwords++;
    this.lastCallback.addSavedPasswordListChangedListener = listener;
  },

  /** @override */
  removeSavedPasswordListChangedListener: function(listener) {
    this.actual_.listening.passwords--;
  },

  /** @override */
  getSavedPasswordList: function(callback) {
    this.actual_.requested.passwords++;
    callback(this.data.passwords);
  },

  /** @override */
  removeSavedPassword: function(loginPair) {
    this.actual_.removed.passwords++;
  },

  /** @override */
  addExceptionListChangedListener: function(listener) {
    this.actual_.listening.exceptions++;
    this.lastCallback.addExceptionListChangedListener = listener;
  },

  /** @override */
  removeExceptionListChangedListener: function(listener) {
    this.actual_.listening.exceptions--;
  },

  /** @override */
  getExceptionList: function(callback) {
    this.actual_.requested.exceptions++;
    callback(this.data.exceptions);
  },

  /** @override */
  removeException: function(exception) {
    this.actual_.removed.exceptions++;
  },

  /** @override */
  getPlaintextPassword: function(loginPair, callback) {
    this.actual_.requested.plaintextPassword++;
    this.lastCallback.getPlaintextPassword = callback;
  },

  /**
   * Verifies expectations.
   * @param {!PasswordManagerExpectations} expected
   */
  assertExpectations: function(expected) {
    var actual = this.actual_;

    assertEquals(expected.requested.passwords, actual.requested.passwords);
    assertEquals(expected.requested.exceptions, actual.requested.exceptions);
    assertEquals(expected.requested.plaintextPassword,
                 actual.requested.plaintextPassword);

    assertEquals(expected.removed.passwords, actual.removed.passwords);
    assertEquals(expected.removed.exceptions, actual.removed.exceptions);

    assertEquals(expected.listening.passwords, actual.listening.passwords);
    assertEquals(expected.listening.exceptions, actual.listening.exceptions);
  },

  // Set these to have non-empty data.
  data: {
    passwords: [],
    exceptions: [],
  },

  // Holds the last callbacks so they can be called when needed/
  lastCallback: {
    addSavedPasswordListChangedListener: null,
    addExceptionListChangedListener: null,
    getPlaintextPassword: null,
  },
};

function AutofillManagerExpectations() {};
AutofillManagerExpectations.prototype = {
  requested: {
    addresses: 0,
    creditCards: 0,
  },

  listening: {
    addresses: 0,
    creditCards: 0,
  },
};

/**
 * Test implementation
 * @implements {AutofillManager}
 * @constructor
 */
function TestAutofillManager() {
  this.actual_ = new AutofillManagerExpectations();
};
TestAutofillManager.prototype = {
  /** @override */
  addAddressListChangedListener: function(listener) {
    this.actual_.listening.addresses++;
    this.lastCallback.addAddressListChangedListener = listener;
  },

  /** @override */
  removeAddressListChangedListener: function(listener) {
    this.actual_.listening.addresses--;
  },

  /** @override */
  getAddressList: function(callback) {
    this.actual_.requested.addresses++;
    callback(this.data.addresses);
  },

  /** @override */
  addCreditCardListChangedListener: function(listener) {
    this.actual_.listening.creditCards++;
    this.lastCallback.addCreditCardListChangedListener = listener;
  },

  /** @override */
  removeCreditCardListChangedListener: function(listener) {
    this.actual_.listening.creditCards--;
  },

  /** @override */
  getCreditCardList: function(callback) {
    this.actual_.requested.creditCards++;
    callback(this.data.creditCards);
  },

  /**
   * Verifies expectations.
   * @param {!AutofillManagerExpectations} expected
   */
  assertExpectations: function(expected) {
    var actual = this.actual_;

    assertEquals(expected.requested.addresses, actual.requested.addresses);
    assertEquals(expected.requested.creditCards, actual.requested.creditCards);

    assertEquals(expected.listening.addresses, actual.listening.addresses);
    assertEquals(expected.listening.creditCards, actual.listening.creditCards);
  },

  // Set these to have non-empty data.
  data: {
    addresses: [],
    creditCards: [],
  },

  // Holds the last callbacks so they can be called when needed/
  lastCallback: {
    addAddressListChangedListener: null,
    addCreditCardListChangedListener: null,
  },
};

/**
 * @constructor
 * @extends {PolymerTest}
 */
function PasswordsAndFormsBrowserTest() {}

PasswordsAndFormsBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/passwords_and_forms_page/' +
      'passwords_and_forms_page.html',

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH),

  /** @override */
  setUp: function() {
    PolymerTest.prototype.setUp.call(this);

    // Test is run on an individual element that won't have a page language.
    this.accessibilityAuditConfig.auditRulesToIgnore.push('humanLangMissing');

    // Override the PasswordManagerImpl for testing.
    this.passwordManager = new TestPasswordManager();
    PasswordManagerImpl.instance_ = this.passwordManager;

    // Override the AutofillManagerImpl for testing.
    this.autofillManager = new TestAutofillManager();
    AutofillManagerImpl.instance_ = this.autofillManager;
  },

  /**
   * Creates a new passwords and forms element.
   * @return {!Object}
   */
  createPasswordsAndFormsElement: function() {
    var element = document.createElement('settings-passwords-and-forms-page');
    document.body.appendChild(element);
    Polymer.dom.flush();
    return element;
  },

  /**
   * Creates PasswordManagerExpectations with the values expected after first
   * creating the element.
   * @return {!PasswordManagerExpectations}
   */
  basePasswordExpectations: function() {
    var expected = new PasswordManagerExpectations();
    expected.requested.passwords = 1;
    expected.requested.exceptions = 1;
    expected.listening.passwords = 1;
    expected.listening.exceptions = 1;
    return expected;
  },

  /**
   * Creates AutofillManagerExpectations with the values expected after first
   * creating the element.
   * @return {!AutofillManagerExpectations}
   */
  baseAutofillExpectations: function() {
    var expected = new AutofillManagerExpectations();
    expected.requested.addresses = 1;
    expected.requested.creditCards = 1;
    expected.listening.addresses = 1;
    expected.listening.creditCards = 1;
    return expected;
  },
};

/**
 * This test will validate that the section is loaded with data.
 */
TEST_F('PasswordsAndFormsBrowserTest', 'uiTests', function() {
  var self = this;

  suite('PasswordsAndForms', function() {
    test('baseLoadAndRemove', function() {
      var element = self.createPasswordsAndFormsElement();

      var passwordsExpectations = self.basePasswordExpectations();
      self.passwordManager.assertExpectations(passwordsExpectations);

      var autofillExpectations = self.baseAutofillExpectations();
      self.autofillManager.assertExpectations(autofillExpectations);

      element.remove();
      Polymer.dom.flush();

      passwordsExpectations.listening.passwords = 0;
      passwordsExpectations.listening.exceptions = 0;
      self.passwordManager.assertExpectations(passwordsExpectations);

      autofillExpectations.listening.addresses = 0;
      autofillExpectations.listening.creditCards = 0;
      self.autofillManager.assertExpectations(autofillExpectations);
    });

    test('loadPasswordsAsync', function() {
      var element = self.createPasswordsAndFormsElement();

      var list = [FakeDataMaker.passwordEntry(), FakeDataMaker.passwordEntry()];
      self.passwordManager.lastCallback.addSavedPasswordListChangedListener(
          list);
      Polymer.dom.flush();

      assertEquals(list, element.savedPasswords);

      // The callback is coming from the manager, so the element shouldn't have
      // additional calls to the manager after the base expectations.
      self.passwordManager.assertExpectations(self.basePasswordExpectations());
      self.autofillManager.assertExpectations(self.baseAutofillExpectations());
    });

    test('loadExceptionsAsync', function() {
      var element = self.createPasswordsAndFormsElement();

      var list = [FakeDataMaker.exceptionEntry(),
                  FakeDataMaker.exceptionEntry()];
      self.passwordManager.lastCallback.addExceptionListChangedListener(
          list);
      Polymer.dom.flush();

      assertEquals(list, element.passwordExceptions);

      // The callback is coming from the manager, so the element shouldn't have
      // additional calls to the manager after the base expectations.
      self.passwordManager.assertExpectations(self.basePasswordExpectations());
      self.autofillManager.assertExpectations(self.baseAutofillExpectations());
    });

    test('loadAddressesAsync', function() {
      var element = self.createPasswordsAndFormsElement();

      var list = [FakeDataMaker.addressEntry(), FakeDataMaker.addressEntry()];
      self.autofillManager.lastCallback.addAddressListChangedListener(list);
      Polymer.dom.flush();

      assertEquals(list, element.addresses);

      // The callback is coming from the manager, so the element shouldn't have
      // additional calls to the manager after the base expectations.
      self.passwordManager.assertExpectations(self.basePasswordExpectations());
      self.autofillManager.assertExpectations(self.baseAutofillExpectations());
    });

    test('loadCreditCardsAsync', function() {
      var element = self.createPasswordsAndFormsElement();

      var list = [FakeDataMaker.creditCardEntry(),
                  FakeDataMaker.creditCardEntry()];
      self.autofillManager.lastCallback.addCreditCardListChangedListener(list);
      Polymer.dom.flush();

      assertEquals(list, element.creditCards);

      // The callback is coming from the manager, so the element shouldn't have
      // additional calls to the manager after the base expectations.
      self.passwordManager.assertExpectations(self.basePasswordExpectations());
      self.autofillManager.assertExpectations(self.baseAutofillExpectations());
    });
  });

  mocha.run();
});

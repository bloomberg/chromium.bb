// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This just tests the interface. It does not test for specific results, only
// that callbacks are correctly invoked, expected parameters are correct,
// and failures are detected.

var availableTests = [
  function saveAddress() {
    var NAME = 'Name';

    var numCalls = 0;
    var handler = function(addressList) {
      numCalls++;

      if (numCalls == 1) {
        chrome.test.assertEq(addressList.length, 0);
      } else {
        chrome.test.assertEq(addressList.length, 1);
        var address = addressList[0];
        chrome.test.assertEq(address.fullNames[0], NAME);
        chrome.test.succeed();
      }
    }

    chrome.autofillPrivate.onAddressListChanged.addListener(handler);
    chrome.autofillPrivate.saveAddress({fullNames: [NAME]});
  },

  function getAddressComponents() {
    var COUNTRY_CODE = 'US';

    var handler = function(components) {
      chrome.test.assertTrue(!!components.components);
      chrome.test.assertTrue(!!components.components[0]);
      chrome.test.assertTrue(!!components.components[0].row);
      chrome.test.assertTrue(!!components.components[0].row[0]);
      chrome.test.assertTrue(!!components.components[0].row[0].field);
      chrome.test.assertTrue(!!components.components[0].row[0].fieldName);
      chrome.test.assertTrue(!!components.languageCode);
      chrome.test.succeed();
    }

    chrome.autofillPrivate.getAddressComponents(COUNTRY_CODE, handler);
  },

  function saveCreditCard() {
    var NAME = 'Name';

    var numCalls = 0;
    var handler = function(creditCardList) {
      numCalls++;

      if (numCalls == 1) {
        chrome.test.assertEq(creditCardList.length, 0);
      } else {
        chrome.test.assertEq(creditCardList.length, 1);
        var creditCard = creditCardList[0];
        chrome.test.assertEq(creditCard.name, NAME);
        chrome.test.succeed();
      }
    }

    chrome.autofillPrivate.onCreditCardListChanged.addListener(handler);
    chrome.autofillPrivate.saveCreditCard({name: NAME});
  },

  function removeEntry() {
    var NAME = 'Name';
    var guid;

    var numCalls = 0;
    var handler = function(creditCardList) {
      numCalls++;

      if (numCalls == 1) {
        chrome.test.assertEq(creditCardList.length, 0);
      } else if (numCalls == 2) {
        chrome.test.assertEq(creditCardList.length, 1);
        var creditCard = creditCardList[0];
        chrome.test.assertEq(creditCard.name, NAME);

        guid = creditCard.guid;
        chrome.autofillPrivate.removeEntry(guid);
      } else {
        chrome.test.assertEq(creditCardList.length, 0);
        chrome.test.succeed();
      }
    }

    chrome.autofillPrivate.onCreditCardListChanged.addListener(handler);
    chrome.autofillPrivate.saveCreditCard({name: NAME});
  },

  function validatePhoneNumbers() {
    var COUNTRY_CODE = 'US';
    var ORIGINAL_NUMBERS = ['1-800-123-4567'];
    var FIRST_NUMBER_TO_ADD = '1-800-234-5768';
    // Same as original number, but without formatting.
    var SECOND_NUMBER_TO_ADD = '18001234567';

    var handler1 = function(validateNumbers) {
      chrome.test.assertEq(validateNumbers.length, 1);
      chrome.test.assertEq('1-800-123-4567', validateNumbers[0]);

      chrome.autofillPrivate.validatePhoneNumbers({
        phoneNumbers: validatedNumbers.concat(FIRST_NUMBER_TO_ADD),
        indexOfNewNumber: 0,
        countryCode: COUNTRY_CODE
      }, handler2);
    }

    var handler2 = function(validatedNumbers) {
      chrome.test.assertEq(validateNumbers.length, 2);
      chrome.test.assertEq('1-800-123-4567', validateNumbers[0]);
      chrome.test.assertEq('1-800-234-5678', validateNumbers[1]);

      chrome.autofillPrivate.validatePhoneNumbers({
        phoneNumbers: validatedNumbers.concat(SECOND_NUMBER_TO_ADD),
        indexOfNewNumber: 0,
        countryCode: COUNTRY_CODE
      }, handler3);
    };

    var handler3 = function(validateNumbers) {
      // Newly-added number should not appear since it was the same as an
      // existing number.
      chrome.test.assertEq(validateNumbers.length, 2);
      chrome.test.assertEq('1-800-123-4567', validateNumbers[0]);
      chrome.test.assertEq('1-800-234-5678', validateNumbers[1]);
      chrome.test.succeed();
    }

    chrome.autofillPrivate.validatePhoneNumbers({
      phoneNumbers: ORIGINAL_NUMBERS,
      indexOfNewNumber: 0,
      countryCode: COUNTRY_CODE
    }, handler1);
  },
];

var testToRun = window.location.search.substring(1);
chrome.test.runTests(availableTests.filter(function(op) {
  return op.name == testToRun;
}));

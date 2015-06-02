// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['options_browsertest_base.js']);

/**
 * Returns the HTML element for the |field|.
 * @param {string} field The field name for the element.
 * @return {HTMLElement} The HTML element.
 */
function getField(field) {
  return document.querySelector(
      '#autofill-edit-address-overlay [field=' + field + ']');
}

/**
 * Returns the size of the |list|.
 * @param {HTMLElement} list The list to check.
 * @return {number} The size of the list.
 */
function getListSize(list) {
  // Remove 1 for placeholder input field.
  return list.items.length - 1;
}

/**
 * TestFixture for autofill options WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function AutofillOptionsWebUITest() {}

AutofillOptionsWebUITest.prototype = {
  __proto__: OptionsBrowsertestBase.prototype,

  /**
   * Browse to autofill options.
   * @override
   */
  browsePreload: 'chrome://settings-frame/autofill',
};

// Test opening the autofill options has correct location.
TEST_F('AutofillOptionsWebUITest', 'testOpenAutofillOptions', function() {
  assertEquals(this.browsePreload, document.location.href);
});

/**
 * TestFixture for autofill edit address overlay WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function AutofillEditAddressWebUITest() {}

AutofillEditAddressWebUITest.prototype = {
  __proto__: OptionsBrowsertestBase.prototype,

  /** @override  */
  browsePreload: 'chrome://settings-frame/autofillEditAddress',
};

TEST_F('AutofillEditAddressWebUITest', 'testInitialFormLayout', function() {
  assertEquals(this.browsePreload, document.location.href);

  var fields = ['country', 'phone', 'email', 'fullName', 'city'];
  for (field in fields) {
    assertEquals('', getField(fields[field]).value, 'Field: ' + fields[field]);
  }

  testDone();
});

TEST_F('AutofillEditAddressWebUITest', 'testLoadAddress', function() {
  // http://crbug.com/434502
  // Accessibility failure was originally (and consistently) seen on Mac OS and
  // Chromium OS. Disabling for all OSs because of a flake in Windows. There is
  // a possibility for flake in linux too.
  this.disableAccessibilityChecks();

  assertEquals(this.browsePreload, document.location.href);

  var testAddress = {
    guid: 'GUID Value',
    fullName: 'Full Name 1',
    companyName: 'Company Name Value',
    addrLines: 'First Line Value\nSecond Line Value',
    dependentLocality: 'Dependent Locality Value',
    city: 'City Value',
    state: 'State Value',
    postalCode: 'Postal Code Value',
    sortingCode: 'Sorting Code Value',
    country: 'CH',
    phone: '123',
    email: 'a@b.c',
    languageCode: 'de',
    components: [[
       {field: 'postalCode', length: 'short'},
       {field: 'sortingCode', length: 'short'},
       {field: 'dependentLocality', length: 'short'},
       {field: 'city', length: 'short'},
       {field: 'state', length: 'short'},
       {field: 'addrLines', length: 'long'},
       {field: 'companyName', length: 'long'},
       {field: 'country', length: 'long'},
       {field: 'fullName', length: 'long', placeholder: 'Add name'}
    ]]
  };
  AutofillEditAddressOverlay.loadAddress(testAddress);

  var overlay = AutofillEditAddressOverlay.getInstance();
  assertEquals(testAddress.guid, overlay.guid_);
  assertEquals(testAddress.languageCode, overlay.languageCode_);

  var inputs = ['companyName', 'dependentLocality', 'city', 'state',
                'postalCode', 'sortingCode', 'fullName', 'email', 'phone'];
  for (var i in inputs) {
    var field = getField(inputs[i]);
    assertEquals(testAddress[inputs[i]], field.value);
    assertTrue(field instanceof HTMLInputElement);
  }

  var addrLines = getField('addrLines');
  assertEquals(testAddress.addrLines, addrLines.value);
  assertTrue(addrLines instanceof HTMLTextAreaElement);

  var country = getField('country');
  assertEquals(testAddress.country, country.value);
  assertTrue(country instanceof HTMLSelectElement);
});

TEST_F('AutofillEditAddressWebUITest', 'testLoadAddressComponents', function() {
  assertEquals(this.browsePreload, document.location.href);

  var testInput = {
    languageCode: 'fr',
    components: [[{field: 'city'}],
                 [{field: 'state'}]]
  };
  AutofillEditAddressOverlay.loadAddressComponents(testInput);

  assertEquals('fr', AutofillEditAddressOverlay.getInstance().languageCode_);
  expectEquals(2, $('autofill-edit-address-fields').children.length);
});

TEST_F('AutofillEditAddressWebUITest', 'testFieldValuesSaved', function() {
  assertEquals(this.browsePreload, document.location.href);

  AutofillEditAddressOverlay.loadAddressComponents({
    languageCode: 'en',
    components: [[{field: 'city'}]]
  });
  getField('city').value = 'New York';

  AutofillEditAddressOverlay.loadAddressComponents({
    languageCode: 'en',
    components: [[{field: 'state'}]]
  });
  assertEquals(null, getField('city'));

  AutofillEditAddressOverlay.loadAddressComponents({
    languageCode: 'en',
    components: [[{field: 'city'}]]
  });
  assertEquals('New York', getField('city').value);
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
 * @return {int} The size of the list.
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
  __proto__: testing.Test.prototype,

  /**
   * Browse to autofill options.
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
  __proto__: testing.Test.prototype,

  /**
   * Browse to autofill edit address overlay.
   */
  browsePreload: 'chrome://settings-frame/autofillEditAddress',

  /** @override  */
  isAsync: true,
};

TEST_F('AutofillEditAddressWebUITest',
       'testAutofillPhoneValueListDoneValidating',
       function() {
  assertEquals(this.browsePreload, document.location.href);

  var phoneList = getField('phone');
  expectEquals(0, phoneList.validationRequests_);
  phoneList.doneValidating().then(function() {
    phoneList.focus();
    var input = phoneList.querySelector('input');
    input.focus();
    document.execCommand('insertText', false, '111-222-333');
    assertEquals('111-222-333', input.value);
    input.blur();
    phoneList.doneValidating().then(function() {
      testDone();
    });
  });
});

TEST_F('AutofillEditAddressWebUITest',
       'testInitialFormLayout',
       function() {
  assertEquals(this.browsePreload, document.location.href);

  assertEquals(getField('country').value, '');
  assertEquals(0, getListSize(getField('phone')));
  assertEquals(0, getListSize(getField('email')));
  assertEquals(0, getListSize(getField('fullName')));
  assertEquals('', getField('city').value);

  testDone();
});

TEST_F('AutofillEditAddressWebUITest',
       'testLoadAddress',
       function() {
  assertEquals(this.browsePreload, document.location.href);

  var testAddress = {
    guid: 'GUID Value',
    fullName: ['Full Name 1', 'Full Name 2'],
    companyName: 'Company Name Value',
    addrLines: 'First Line Value\nSecond Line Value',
    dependentLocality: 'Dependent Locality Value',
    city: 'City Value',
    state: 'State Value',
    postalCode: 'Postal Code Value',
    sortingCode: 'Sorting Code Value',
    country: 'CH',
    phone: ['123', '456'],
    email: ['a@b.c', 'x@y.z'],
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

  assertEquals(testAddress.guid, AutofillEditAddressOverlay.getInstance().guid);
  assertEquals(testAddress.languageCode,
               AutofillEditAddressOverlay.getInstance().languageCode);

  var lists = ['fullName', 'email', 'phone'];
  for (var i in lists) {
    var field = getField(lists[i]);
    assertEquals(testAddress[lists[i]].length, getListSize(field));
    assertTrue(field.getAttribute('placeholder').length > 0);
    assertTrue(field instanceof cr.ui.List);
  }

  var inputs = ['companyName', 'dependentLocality', 'city', 'state',
                'postalCode', 'sortingCode'];
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

  testDone();
});

TEST_F('AutofillEditAddressWebUITest',
       'testLoadAddressComponents',
       function() {
  assertEquals(this.browsePreload, document.location.href);

  var testInput = {
    languageCode: 'fr',
    components: [[{field: 'city'}],
                 [{field: 'state'}]]
  };
  AutofillEditAddressOverlay.loadAddressComponents(testInput);

  assertEquals('fr', AutofillEditAddressOverlay.getInstance().languageCode);
  expectEquals(2, $('autofill-edit-address-fields').children.length);

  testDone();
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestFixture for autofill options WebUI testing.
 * @extends {testing.Test}
 * @constructor
 **/
function AutofillOptionsWebUITest() {}

AutofillOptionsWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to autofill options.
   **/
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

  var phoneList = $('phone-list');
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

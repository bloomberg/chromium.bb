// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function AccountsOptionsWebUITest() {}

AccountsOptionsWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to accounts options.
   */
  browsePreload: 'chrome://settings-frame/accounts',
};

function createEnterKeyboardEvent(type) {
  return new KeyboardEvent(type, {
    'bubbles': true,
    'cancelable': true,
    'keyIdentifier': 'Enter'
  });
}

TEST_F('AccountsOptionsWebUITest', 'testNoCloseOnEnter', function() {
  assertEquals(this.browsePreload, document.location.href);

  var inputField = $('userNameEdit');
  var accountsOptionsPage = AccountsOptions.getInstance();

  // Overlay is visible.
  assertTrue(accountsOptionsPage.visible);

  // Simulate pressing the enter key in the edit field.
  inputField.dispatchEvent(createEnterKeyboardEvent('keydown'));
  inputField.dispatchEvent(createEnterKeyboardEvent('keypress'));
  inputField.dispatchEvent(createEnterKeyboardEvent('keyup'));

  // Verify the overlay is still visible.
  assertTrue(accountsOptionsPage.visible);
});

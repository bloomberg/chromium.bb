// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('RuntimeHostsDialog', function() {
  /** @type {extensions.RuntimeHostsDialogElement} */ let dialog;
  /** @type {extensions.TestService} */ let delegate;
  const ITEM_ID = 'a'.repeat(32);

  setup(function() {
    PolymerTest.clearBody();
    dialog = document.createElement('extensions-runtime-hosts-dialog');

    delegate = new extensions.TestService();
    dialog.delegate = delegate;
    dialog.itemId = ITEM_ID;

    document.body.appendChild(dialog);
  });

  teardown(function() {
    dialog.remove();
  });

  test('valid input', function() {
    const input = dialog.$$('cr-input');
    const site = 'http://www.example.com';
    input.value = site;
    input.fire('input');
    assertFalse(input.invalid);

    const submit = dialog.$.submit;
    assertFalse(submit.disabled);
    submit.click();
    return delegate.whenCalled('addRuntimeHostPermission').then((args) => {
      let id = args[0];
      let input = args[1];
      assertEquals(ITEM_ID, id);
      assertEquals(site, input);
    });
  });

  test('invalid input', function() {
    // Initially the action button should be disabled, but the error warning
    // should not be shown for an empty input.
    const input = dialog.$$('cr-input');
    assertFalse(input.invalid);
    const submit = dialog.$.submit;
    assertTrue(submit.disabled);

    // Simulate user input of invalid text.
    const invalidSite = 'foobar';
    input.value = invalidSite;
    input.fire('input');
    assertTrue(input.invalid);
    assertTrue(submit.disabled);

    // Entering valid text should clear the error and enable the submit button.
    input.value = 'http://www.example.com';
    input.fire('input');
    assertFalse(input.invalid);
    assertFalse(submit.disabled);
  });

  test('delegate indicates invalid input', function() {
    delegate.acceptRuntimeHostPermission = false;

    const input = dialog.$$('cr-input');
    const site = 'http://http://http://';
    input.value = site;
    input.fire('input');
    assertFalse(input.invalid);

    const submit = dialog.$.submit;
    assertFalse(submit.disabled);
    submit.click();
    return delegate.whenCalled('addRuntimeHostPermission').then(() => {
      assertTrue(input.invalid);
      assertTrue(submit.disabled);
    });
  });

  test('editing current entry', function() {
    const oldSite = 'http://example.com';
    const newSite = 'http://chromium.org';

    dialog.currentSite = oldSite;
    const input = dialog.$$('cr-input');
    input.value = newSite;
    input.fire('input');
    const submit = dialog.$.submit;

    submit.click();
    return delegate.whenCalled('removeRuntimeHostPermission')
        .then((args) => {
          expectEquals(ITEM_ID, args[0] /* id */);
          expectEquals(oldSite, args[1] /* site */);
          return delegate.whenCalled('addRuntimeHostPermission');
        })
        .then((args) => {
          expectEquals(ITEM_ID, args[0] /* id */);
          expectEquals(newSite, args[1] /* site */);
        });
  });
});

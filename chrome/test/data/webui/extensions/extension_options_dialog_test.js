// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of tests for extension options dialog.
 * These are run as part of interactive_ui_tests.
 */
suite('ExtensionOptionsDialogTest', function() {
  /** @type {extensions.Manager} */
  let manager;

  setup(function() {
    manager = document.querySelector('extensions-manager');
  });

  function getOptionsDialog() {
    const dialog = manager.$$('#options-dialog');
    assertTrue(!!dialog);
    return dialog;
  }

  test('show options dialog', function() {
    const extensionDetailView = manager.$$('extensions-detail-view');
    assertTrue(!!extensionDetailView);

    const optionsButton = extensionDetailView.$$('#extensions-options');

    // Click the options button.
    optionsButton.click();

    // Wait for dialog to open.
    return test_util.eventToPromise('cr-dialog-open', manager)
        .then(() => {
          const dialog = getOptionsDialog();
          let waitForClose = test_util.eventToPromise('close', dialog);
          // Close dialog and wait.
          dialog.$.dialog.cancel();
          return waitForClose;
        })
        .then(() => {
          // Validate that this button is focused after dialog closes.
          assertEquals(optionsButton.$$('button'), getDeepActiveElement());
        });
  });
});

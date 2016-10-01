// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-options-dialog. */
cr.define('extension_options_dialog_tests', function() {
  /** @enum {string} */
  var TestNames = {
    Layout: 'Layout',
  };

  // These match the variables in options_dialog.js.
  var MAX_HEIGHT = '600px';
  var MAX_WIDTH = '600px';
  var MIN_HEIGHT = '300px';
  var MIN_WIDTH = '300px';

  function registerTests() {
    suite('ExtensionOptionsDialogTests', function() {
      /** @type {extensions.OptionsDialog} */
      var optionsDialog;

      /** @type {chrome.developerPrivate.ExtensionInfo} */
      var data;

      setup(function() {
        PolymerTest.clearBody();
        data = extension_test_util.createExtensionInfo();
        optionsDialog = new extensions.OptionsDialog();
        document.body.appendChild(optionsDialog);
      });

      test(assert(TestNames.Layout), function() {
        var dialogElement = optionsDialog.$$('dialog');
        var isDialogVisible = function() {
          var rect = dialogElement.getBoundingClientRect();
          return rect.width * rect.height > 0;
        };

        // Try showing the dialog.
        expectFalse(isDialogVisible());
        optionsDialog.show(data);
        expectTrue(isDialogVisible());
        expectEquals(
            data.name,
            assert(optionsDialog.$$('#icon-and-name-wrapper span')).
                textContent.trim());

        var mainDiv = optionsDialog.$.main;

        // To start, the options page should be set to the min width/height.
        expectEquals(MIN_HEIGHT, mainDiv.style.height);
        expectEquals(MIN_WIDTH, mainDiv.style.width);

        var mockOptions = optionsDialog.extensionOptions_;
        expectEquals(data.id, mockOptions.extension);

        // Setting the preferred size to something below the min width/height
        // shouldn't change the actual width/height.
        mockOptions.onpreferredsizechanged({height: 100, width: 100});
        expectEquals(MIN_HEIGHT, mainDiv.style.height);
        expectEquals(MIN_WIDTH, mainDiv.style.width);

        // Setting the preferred size to between the min and max dimensions
        // should change the dimensions.
        mockOptions.onpreferredsizechanged({height: 500, width: 400});
        expectEquals('500px', mainDiv.style.height);
        expectEquals('400px', mainDiv.style.width);

        // Max values should pin the dialog.
        mockOptions.onpreferredsizechanged({height: 900, width: 400});
        expectEquals(MAX_HEIGHT, mainDiv.style.height);
        expectEquals('400px', mainDiv.style.width);

        mockOptions.onclose();
        expectFalse(isDialogVisible());

        // Try showing a second extension with a longer name.
        var secondExtension = extension_test_util.createExtensionInfo({
            name: 'Super long named extension for the win'
        });
        optionsDialog.show(secondExtension);
        expectTrue(isDialogVisible());
        expectEquals(secondExtension.id, mockOptions.extension);
        expectEquals(
            secondExtension.name,
            assert(optionsDialog.$$('#icon-and-name-wrapper span')).
                textContent.trim());
        // The width of the dialog should be set to match the width of the
        // header, which is greater than the default min width.
        expectTrue(mainDiv.style.width > MIN_WIDTH, mainDiv.style.height);
        expectEquals(MIN_HEIGHT, mainDiv.style.height);

        // Going back to an extension with a shorter name should resize the
        // dialog.
        optionsDialog.close();
        optionsDialog.show(data);
        expectEquals(MIN_HEIGHT, mainDiv.style.height);
        expectEquals(MIN_WIDTH, mainDiv.style.width);
      });
    });
  }

  return {
    registerTests: registerTests,
    TestNames: TestNames,
  };
});

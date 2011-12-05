// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Test fixture for search engine dialog tests.
 * @extends {testing.Test}
 */
function EditSearchEngineDialogUITest() {};

EditSearchEngineDialogUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Define the C++ fixture class and include it.
   * @type {?string}
   * @override
   */
  typedefCppFixture: 'EditSearchEngineDialogUITest',

  /**
   * Show the search engine dialog.
   */
  testGenPreamble: function() {
    GEN('ShowSearchEngineDialog();');
  },
};

/**
 * Asynchronous Test fixture for search engine dialog tests.
 * @extends {EditSearchEngineDialogUITest}
 */
function EditSearchEngineDialogUITestAsync() {};

EditSearchEngineDialogUITestAsync.prototype = {
  __proto__: EditSearchEngineDialogUITest.prototype,

  /** @inheritDoc */
  isAsync: true,

  /**
   * Argument to pass through to |setValidation| once the test is ready to
   * process it.  The argument consists of a mapping between field names and
   * contents.
   * @type{Object.<string, string>}
   */
  pendingDetails: null,

  /**
   * Overrides |setValidation| to address a potential race condition due to
   * asynchronous initialization of the edit search engine dialog. The override
   * catches the data being passed to the original |setValidation| method and
   * defers the validation call until the test is ready to process it.
   */
  preLoad: function() {
    var self = this;
    window.addEventListener('DOMContentLoaded', function() {
      self.originalSetValidation = editSearchEngineDialog.setValidation;
      editSearchEngineDialog.setValidation = function(details) {
        self.pendingDetails = details;
      };
    });
  },
};

// Include the bulk of c++ code.
GEN('#include ' +
    '"chrome/test/data/webui/edit_search_engine_dialog_browsertest.h"');

/**
 * Confirms that the data is loaded into the text fields.
 */
TEST_F('EditSearchEngineDialogUITest', 'testData', function() {
  var inputFields = editSearchEngineDialog.inputFields;
  expectEquals(chrome.expectedUrl, window.location.href);
  expectEquals('short0', inputFields.description.value);
  expectEquals('keyword0', inputFields.keyword.value);
  expectEquals('http://www.test0.com/', inputFields.url.value);
});

/**
 * Confirms that the field validation is working.
 */
TEST_F('EditSearchEngineDialogUITestAsync',
       'testInputValidation',
       function() {
  var inputFields = editSearchEngineDialog.inputFields;
  var invalidData = {
    description: '',
    keyword: '',
    url: '%'
  };
  var fieldNames = Object.keys(invalidData);
  var index = 0;
  var self = this;
  var validityChecker = function(details) {
    self.originalSetValidation(details);
    // Ensure that all inputs are valid with the initial data, and that save
    // is enabled.  Each subsequent call will have an additional field
    // containing invalid data.
    for (var i = 0; i < fieldNames.length; i++) {
      var field = fieldNames[i];
      expectEquals(i >= index,
                   inputFields[field].valid,
                   'field = ' + field +
                   ', index = ' + index);
    }
    var saveButtonState = editSearchEngineDialog.getSave().disabled;
    if (index == 0)
      expectFalse(saveButtonState);
    else
      expectTrue(saveButtonState);
    // Once the validity of the input fields and save button are confirmed, we
    // invalidate a single field in the input and trigger a new call to check
    // the validity.  We start with the first input field and increment the
    // index with each call in order to advance to the next field.  Although
    // each test is asynchronous, we are guaranteed to get the calls in the
    // correct order since we wait for the previous validation to complete
    // before moving to the next field.
    if (index < fieldNames.length) {
      if (index == fieldNames.length - 1) {
        editSearchEngineDialog.setValidation = this.continueTest(
            WhenTestDone.ALWAYS,
            validityChecker);
      }
      var fieldName = fieldNames[index++];
      inputFields[fieldName].value = invalidData[fieldName];
      editSearchEngineDialog.validate();
    }
  };
  // Override |setValidation| to check the status of the icons and save button.
  editSearchEngineDialog.setValidation = this.continueTest(
      WhenTestDone.EXPECT,
      validityChecker);
  // Check if the first call to |setValidation| tripped before being redirected
  // to |validityChecker|.
  if (this.pendingDetails)
    editSearchEngineDialog.setValidation(this.pendingDetails);
});


// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays information on ChromeOS specific features.
 */
var CrosView = (function() {
  'use strict';

  var fileContent;
  var passcode = '';

  /**
   *  Send file contents and passcode to C++ cros network library.
   *
   *  @private
   */
  function importONCFile_() {
    if (fileContent)
      g_browser.importONCFile(fileContent, passcode);
    else
      setParseStatus_(false);
  }

  /**
   *  Set the passcode var, and trigger onc import.
   *
   *  @private
   *  @param {string} passcode
   */
  function setPasscode_(value) {
    passcode = value;
    if (passcode)
      importONCFile_();
  }

  /**
   *  Unhide the passcode prompt input field and give it focus.
   *
   *  @private
   */
  function promptForPasscode_() {
    $(CrosView.PASSCODE_ID).hidden = false;
    $(CrosView.PASSCODE_INPUT_ID).focus();
    $(CrosView.PASSCODE_INPUT_ID).select();
  }

  /**
   *  Set the fileContent var, and trigger onc import if the file appears to
   *  not be encrypted, or prompt for passcode if the file is encrypted.
   *
   *  @private
   *  @param {string} text contents of selected file.
   */
  function setFileContent_(result) {
    fileContent = result;
    // Check if file is encrypted.
    if (fileContent.search(/begin pgp message/i) == -1) {
      // Not encrypted, don't need passcode.
      importONCFile_();
    } else {
      promptForPasscode_();
    }
  }

  /**
   *  Set ONC file parse status.
   *
   *  @private
   */
  function setParseStatus_(success) {
    var parseStatus = $(CrosView.PARSE_STATUS_ID);
    parseStatus.hidden = false;
    parseStatus.textContent = status ?
        "ONC file successfully parsed" : "ONC file parse failed";
    reset_();
  }

  /**
   *  Add event listeners for the file selection and passcode input fields.
   *
   *  @private
   */
  function addEventListeners_() {
    $(CrosView.IMPORT_ONC_ID).addEventListener('change', function(event) {
      $(CrosView.PARSE_STATUS_ID).hidden = true;
      var file = event.target.files[0];
      var reader = new FileReader();
      reader.onloadend = function(e) {
        setFileContent_(this.result);
      };
      reader.readAsText(file);
    }, false);

    $(CrosView.PASSCODE_INPUT_ID).addEventListener('change', function(event) {
      setPasscode_(this.value);
    }, false);
  }

  /**
   *  Reset fileContent and passcode vars.
   *
   *  @private
   */
  function reset_() {
    fileContent = undefined;
    passcode = '';
    $(CrosView.PASSCODE_ID).hidden = true;
  }

  /**
   *  @constructor
   *  @extends {DivView}
   */
  function CrosView() {
    assertFirstConstructorCall(CrosView);

    // Call superclass's constructor.
    DivView.call(this, CrosView.MAIN_BOX_ID);

    g_browser.addCrosONCFileParseObserver(this);
    addEventListeners_();
  }

  // ID for special HTML element in category_tabs.html
  CrosView.TAB_HANDLE_ID = 'tab-handle-chromeos';

  CrosView.MAIN_BOX_ID = 'chromeos-view-tab-content';
  CrosView.IMPORT_ONC_ID = 'chromeos-view-import-onc';
  CrosView.PASSCODE_ID = 'chromeos-view-password-div';
  CrosView.PASSCODE_INPUT_ID = 'chromeos-view-onc-password';
  CrosView.PARSE_STATUS_ID = 'chromeos-view-parse-status';

  cr.addSingletonGetter(CrosView);

  CrosView.prototype = {
    // Inherit from DivView.
    __proto__: DivView.prototype,

    onONCFileParse: setParseStatus_,
  };

  return CrosView;
})();

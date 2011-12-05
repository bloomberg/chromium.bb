// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('editSearchEngineDialog', function() {
  'use strict';

  /**
   * Accessor for an entry field in the search engine dialog.
   * @param {string} baseName Name of the field, which serves as a base name
   *     for the text input field and icon.
   * @constructor
   */
  function SearchEngineDialogEntryField(baseName) {
    this.name_ = baseName;
    this.text_ = $(baseName + '-text');
    this.icon_ = $(baseName + '-icon');
    this.text_.oninput = validate;
    return this;
  }

  SearchEngineDialogEntryField.prototype = {
    /*
     * Getter for the name of the field.
     * @type {string} Descriptive name of the field.
     */
    get name() {
      return this.name_;
    },

    /*
     * Getter for the content of the input field.
     * @type {string} Text content in the input field.
     */
    get value() {
      return this.text_.value;
    },

    /**
     * Setter for the content of the input field. The validity of the input is
     * not automatically revalidated.
     * @type {string} New content for the input field.
     */
    set value(text) {
      this.text_.value = text;
      // Validity is in an indeterminate state until validate is called. Clear
      // the class name associated with the icon.
      this.icon_.className = '';
    },

    /**
     * Getter for the validity of an input field.
     * @type {boolean} True if the text input is valid, otherwise false.
     */
    get valid() {
      return this.icon_.className == 'valid';
    },

    /**
     * Setter for the input field validily.
     * @type {boolean} True if the input field is valid, false for invalid.
     */
    set valid(state) {
      this.icon_.className = state ? 'valid' : 'invalid';
    },

    /**
     * Creates a text representation of the class containing the name,
     * text field contents and validity.
     * @return {string} Text representation.
     */
    toString: function() {
      return this.name_ + ': \'' + this.text_.value + '\' (' +
          this.icon_.className + ')';
    }
  };

  /**
   * Accessors for entry fields in the search engine dialog. Initialized after
   * content is loaded.
   * @type {Object.<string, SearchEngineDialogEntryField>}
   */
  var inputFields = {};

  /**
   * Disables the controls while the dialog is busy.
   */
  function disableControls() {
    $('cancel').disabled = true;
    $('save').disabled = true;
  }

  /**
   * Close the dialog and pass a result value to the dialog close handler.
   * @param {{description: string, details: string, url: string}=} opt_result
   *     The value to pass to the dialog close handler.
   */
  function closeWithResult(opt_result) {
    disableControls();
    var json = JSON.stringify(opt_result ? [opt_result] : []);
    chrome.send('DialogClose', [json]);
  }

  /**
  * Sets the text of the dialog's editable text boxes.
  * @param {{description: string, details: string, url: string}} details Values
  *     for corresponding text fields.
  */
  function setDetails(details) {
    inputFields.description.value = details.description;
    inputFields.keyword.value = details.keyword;
    inputFields.url.value = details.url;
    validate();
  }

  /**
   * Sends all strings to Chrome for validation.  Chrome is expected to respond
   * by calling setValidation.
   */
  function validate() {
    chrome.send('requestValidation', [inputFields.description.value,
                                      inputFields.keyword.value,
                                      inputFields.url.value]);
  }

  /**
   * Sets dialog state given the results of the validation of input by Chrome.
   * @param {{description: boolean,
              keyword: boolean,
              url: boolean,
              ok: boolean}} details
   *     A dictionary of booleans indicating the validation results of various
   *     parts of the UI.  |description|, |keyword| and |url| indicate the
   *     validity of the respective text fields, and |ok| indicates whether
   *     the OK/Save button can be pressed.
   */
  function setValidation(details) {
    inputFields.description.valid = details.description;
    inputFields.keyword.valid = details.keyword;
    inputFields.url.valid = details.url;
    $('save').disabled = !details.ok;
  }

  /**
   * Reverses the order of child nodes.
   * @param {HTMLElement} parent The parent node whose children are to be
   *     reversed.
   */
  function reverseChildren(parent) {
    var childNodes = parent.childNodes;
    for (var i = childNodes.length - 1; i >= 0; i--)
      parent.appendChild(childNodes[i]);
  }

  var forEach = Array.prototype.forEach.call.bind(Array.prototype.forEach);

  /**
   * Inserts translated strings on loading.
   */
  function initialize() {
    i18nTemplate.process(document, templateData);

    inputFields.description = new SearchEngineDialogEntryField('description');
    inputFields.keyword = new SearchEngineDialogEntryField('keyword');
    inputFields.url = new SearchEngineDialogEntryField('url');

    document.title = chrome.dialogArguments == 'add' ? templateData.titleNew :
        templateData.titleEdit;

    $('cancel').onclick = function() {
      closeWithResult();
    };

    $('save').onclick = function() {
      closeWithResult({description: inputFields.description.value,
                       keyword: inputFields.keyword.value,
                       url: inputFields.url.value});
    };

    setValidation({description: false, keyword: false, url: false});
    if (cr.isViews)
      forEach(document.querySelectorAll('.button-strip'), reverseChildren);

    // TODO(kevers): Should be a cleaner way to implement without requiring
    // multiple callback to C++. The call to |requestDetails| fetches the
    // content to insert into the input fields without inidicating if the
    // inputs are valid.  A separate callback is then required to determine if
    // the inputs are OK. In fact, it should be possible to pass in the details
    // when the dialog is created rather than using a callback.
    chrome.send('requestDetails');
  }

  /**
   * Retrieves the save button element.
   * @return {Element}
   */
  function getSave() {
    return $('save');
  }

  document.addEventListener('DOMContentLoaded', initialize);

  return {
    inputFields: inputFields,
    getSave: getSave,
    setDetails: setDetails,
    setValidation: setValidation,
    validate: validate
  };
});



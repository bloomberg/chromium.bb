// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('editSearchEngineDialog', function() {
  'use strict';

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
    $('description-text').value = details.description;
    $('keyword-text').value = details.keyword;
    $('url-text').value = details.url;
    validate();
  }

  /**
   * Updates the validity icon element by changing its style.
   * @param {Object} element The element to change.
   * @param {boolean} valid True if the data is valid.
   */
  function setValidImage(element, valid) {
    element.className = valid ? 'valid' : 'invalid';
  }

  /**
   * Sends all strings to Chrome for validation.  Chrome is expected to respond
   * by calling setValidation.
   */
  function validate() {
    chrome.send('requestValidation', [$('description-text').value,
                $('keyword-text').value, $('url-text').value]);
  }

  /**
   * Sets dialog state given the results of the validation of input by Chrome.
   * @param {{description: boolean, details: boolean, url: boolean, ok:boolean}}
   *     details A dictionary of booleans indicating the validation results of
   *     various parts of the UI.  |description|, |details| and |url| indicate
   *     the validity of the respective text fields, and |ok| indicates whether
   *     the OK/Save button can be pressed.
   */
  function setValidation(details) {
    setValidImage($('description-icon'), details.description);
    setValidImage($('keyword-icon'), details.keyword);
    setValidImage($('url-icon'), details.url);
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
  };

  var forEach = Array.prototype.forEach.call.bind(Array.prototype.forEach);

  /**
   * Inserts translated strings on loading.
   */
  function initialize() {
    i18nTemplate.process(document, templateData);

    document.title = chrome.dialogArguments == 'add' ? templateData.titleNew :
        templateData.titleEdit;

    $('cancel').onclick = function() {
      closeWithResult();
    }

    $('save').onclick = function() {
      closeWithResult({description: $('description-text').value,
                       keyword: $('keyword-text').value,
                       url: $('url-text').value});
    }

    $('description-text').oninput = validate;
    $('keyword-text').oninput = validate;
    $('url-text').oninput = validate;

    setValidation({description: false, keyword: false, url: false});
    if (cr.isViews)
      forEach(document.querySelectorAll('.button-strip'), reverseChildren);
    chrome.send('requestDetails')
  }

  document.addEventListener('DOMContentLoaded', initialize);

  return {
    setDetails: setDetails,
    setValidation: setValidation,
  };
});



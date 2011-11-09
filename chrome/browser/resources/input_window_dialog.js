// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('inputWindowDialog', function() {
  'use strict';

  /**
   * Disables the controls while the dialog is busy.
   */
  function disableControls() {
    $('cancel').disabled = true;
    $('ok').disabled = true;
  }

  /**
   * Returns true if URL area is shown.
   */
  function isURLAreaShown() {
    return !$('url-area').classList.contains('area-hidden');
  }

  /**
   * Close the dialog and pass a result value to the dialog close handler.
   * @param {boolean} result The value to pass to the dialog close handler.
   */
  function closeWithResult(result) {
    disableControls();
    var values = [result];
    if (result) {
      values.push($('name').value);
      if (isURLAreaShown()) {
        values.push($('url').value);
      }
    }
    var json = JSON.stringify(values);
    chrome.send('DialogClose', [json]);
  }

  /**
   * Inserts translated strings on loading.
   */
  function initialize() {
    i18nTemplate.process(document, templateData);

    var args = JSON.parse(chrome.dialogArguments);
    $('name-label').textContent = args.nameLabel;
    $('name').value = args.name;
    $('ok').textContent = args.ok_button_title;

    if (args.urlLabel && args.url) {
      $('url-label').textContent = args.urlLabel;
      $('url').value = args.url;
    } else {
      $('url-area').classList.add('area-hidden');
    }

    $('cancel').onclick = function() {
      closeWithResult(false);
    }

    $('ok').onclick = function() {
      if (!$('ok').disabled) {
        closeWithResult(true);
      }
    }

    function validate() {
      var values = [$('name').value];
      if (isURLAreaShown()) {
        values.push($('url').value);
      }
      chrome.send('validate', values);
    }

    $('name').oninput = validate;
    $('url').oninput = isURLAreaShown() ? validate : null;

    document.body.onkeydown = function(evt) {
      if (evt.keyCode == 13) {  // Enter key
        $('ok').onclick();
      } else if (evt.keyCode == 27) {  // Escape key
        $('cancel').onclick();
      }
    }
  }

  /**
   * Called in response to validate request.
   * @param {boolean} valid The result of validate request.
   */
  function ackValidation(valid) {
    $('ok').disabled = !valid;
  }

  return {
    initialize: initialize,
    ackValidation: ackValidation,
  };
});

document.addEventListener('DOMContentLoaded', inputWindowDialog.initialize);

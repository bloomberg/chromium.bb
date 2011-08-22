// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('hungRendererDialog', function() {
  'use strict';

  /**
   * Disables the controls while the dialog is busy.
   */
  function disableControls() {
    $('kill').disabled = true;
    $('wait').disabled = true;
  }

  /**
   * Close the dialog and pass a result value to the dialog close handler.
   * @param {boolean} result The value to pass to the dialog close handler.
   */
  function closeWithResult(result) {
    disableControls();
    var json = JSON.stringify([result]);
    chrome.send('DialogClose', [json]);
  }

  /**
   * Inserts translated strings on loading.
   */
  function initialize() {
    i18nTemplate.process(document, templateData);

    $('kill').onclick = function() {
      closeWithResult(true);
    }

    $('wait').onclick = function() {
      closeWithResult(false);
    }
  }

  return {
    initialize: initialize,
  };
});

document.addEventListener('DOMContentLoaded', hungRendererDialog.initialize);

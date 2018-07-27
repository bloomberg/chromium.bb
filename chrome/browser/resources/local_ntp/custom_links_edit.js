/* Copyright 2018 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */


/**
 * Enum for ids.
 * @enum {string}
 * @const
 */
const IDS = {
  EDIT_DIALOG: 'edit-link-dialog',  // Edit dialog.
  CANCEL: 'cancel',                 // Cancel button.
  DELETE: 'delete',                 // Delete button.
  DONE: 'done',                     // Done button.
};


/**
 * The origin of this request, i.e. 'https://www.google.TLD' for the remote NTP,
 * or 'chrome-search://local-ntp' for the local NTP.
 * @const {string}
 */
let DOMAIN_ORIGIN = '{{ORIGIN}}';


/**
 * Alias for document.getElementById.
 * @param {string} id The ID of the element to find.
 * @return {HTMLElement} The found element or null if not found.
 */
function $(id) {
  // eslint-disable-next-line no-restricted-properties
  return document.getElementById(id);
}


/**
 * Send a message to close the edit dialog. Called when the edit flow has been
 * completed.
 * @param {!Event} event The click event.
 */
function closeDialog(event) {
  window.parent.postMessage({cmd: 'closeDialog'}, DOMAIN_ORIGIN);
}


/**
 * Does some initialization and shows the dialog window.
 */
function init() {
  $(IDS.EDIT_DIALOG).showModal();
  $(IDS.DELETE).addEventListener('click', closeDialog);
  $(IDS.CANCEL).addEventListener('click', closeDialog);
  $(IDS.DONE).addEventListener('click', closeDialog);
}


window.addEventListener('DOMContentLoaded', init);

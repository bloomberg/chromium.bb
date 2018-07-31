/* Copyright 2018 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */


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
 * Enum for ids.
 * @enum {string}
 * @const
 */
const IDS = {
  CANCEL: 'cancel',                      // Cancel button.
  DELETE: 'delete',                      // Delete button.
  DIALOG_TITLE: 'dialog-title',          // Dialog title.
  DONE: 'done',                          // Done button.
  EDIT_DIALOG: 'edit-link-dialog',       // Dialog element.
  FORM: 'edit-form',                     // The edit link form.
  INVALID_URL: 'invalid-url',            // Invalid URL error message.
  TITLE_FIELD: 'title-field',            // Title input field.
  TITLE_FIELD_NAME: 'title-field-name',  // Title input field name.
  URL_FIELD: 'url-field',                // URL input field.
  URL_FIELD_CONTAINER: 'url',            // URL input field container.
  URL_FIELD_NAME: 'url-field-name',      // URL input field name.
};


/**
 * The origin of this request, i.e. 'https://www.google.TLD' for the remote NTP,
 * or 'chrome-search://local-ntp' for the local NTP.
 * @const {string}
 */
const DOMAIN_ORIGIN = '{{ORIGIN}}';


/**
 * List of parameters passed by query args.
 * @type {Object}
 */
let queryArgs = {};


/**
 * The prepopulated data for the form. Includes title, url, and rid.
 * @type {Object}
 */
let prepopulatedLink = {
  rid: -1,
  title: '',
  url: '',
};


/**
 * True if the provided url is valid.
 * @type {string}
 */
function isValidURL(url) {
  let a = document.createElement('a');
  a.href = url;
  // Invalid URLs will not match the current host.
  let isValid = a.host && a.host != window.location.host;
  return isValid;
}


/**
 * Handler for the 'linkData' message from the host page. Pre-populates the url
 * and title fields with link's data obtained using the rid. Called if we are
 * editing an existing link.
 * @param {number} rid Restricted id of the link to be edited.
 */
function prepopulateFields(rid) {
  if (!isFinite(rid))
    return;

  // Grab the link data from the embeddedSearch API.
  let data = chrome.embeddedSearch.newTabPage.getMostVisitedItemData(rid);
  if (!data)
    return;
  prepopulatedLink.rid = rid;
  $(IDS.TITLE_FIELD).value = prepopulatedLink.title = data.title;
  $(IDS.URL_FIELD).value = prepopulatedLink.url = data.url;
}


/**
 * Disables the "Done" button until the URL field is modified.
 */
function disableSubmitUntilTextInput() {
  $(IDS.DONE).disabled = true;
  let reenable = (event) => {
    $(IDS.DONE).disabled = false;
    $(IDS.URL_FIELD).removeEventListener('input', reenable);
  };
  $(IDS.URL_FIELD).addEventListener('input', reenable);
}


/**
 * Shows the invalid URL error message until the URL field is modified.
 */
function showInvalidUrlUntilTextInput() {
  $(IDS.URL_FIELD_CONTAINER).classList.add('invalid');
  let reenable = (event) => {
    $(IDS.URL_FIELD_CONTAINER).classList.remove('invalid');
    $(IDS.URL_FIELD).removeEventListener('input', reenable);
  };
  $(IDS.URL_FIELD).addEventListener('input', reenable);
}


/**
 * Send a message to close the edit dialog. Called when the edit flow has been
 * completed. If the fields were unchanged, does not update the link data.
 * @param {!Event} event The click event.
 */
function finishEditLink(event) {
  // Show error message for invalid urls.
  if (!isValidURL($(IDS.URL_FIELD).value)) {
    showInvalidUrlUntilTextInput();
    disableSubmitUntilTextInput();
    return;
  }

  let newUrl = '';
  let newTitle = '';
  if ($(IDS.URL_FIELD).value != prepopulatedLink.url)
    newUrl = $(IDS.URL_FIELD).value;
  if ($(IDS.TITLE_FIELD).value != prepopulatedLink.title)
    newTitle = $(IDS.TITLE_FIELD).value;

  // Do not update link if fields were unchanged.
  if (!newUrl && !newTitle)
    return;

  chrome.embeddedSearch.newTabPage.updateCustomLink(
      prepopulatedLink.rid, newUrl, newTitle);
  closeDialog();
}


/**
 * Call the EmbeddedSearchAPI to delete the link. Closes the dialog.
 * @param {!Event} event The click event.
 */
function deleteLink(event) {
  chrome.embeddedSearch.newTabPage.deleteMostVisitedItem(prepopulatedLink.rid);
  closeDialog();
}


/**
 * Send a message to close the edit dialog, clears the url and title fields, and
 * resets the button statuses. Called when the edit flow has been completed.
 */
function closeDialog() {
  window.parent.postMessage({cmd: 'closeDialog'}, DOMAIN_ORIGIN);
  $(IDS.FORM).reset();
  $(IDS.URL_FIELD_CONTAINER).classList.remove('invalid');
  $(IDS.DELETE).disabled = false;
  $(IDS.DONE).disabled = false;
  prepopulatedLink.rid = -1;
  prepopulatedLink.title = '';
  prepopulatedLink.url = '';
}


/**
 * Event handler for messages from the host page.
 * @param {Event} event Event received.
 */
function handlePostMessage(event) {
  let cmd = event.data.cmd;
  let args = event.data;
  if (cmd === 'linkData') {
    if (args.tid) {  // We are editing a link, prepopulate the link data.
      prepopulateFields(args.tid);
    } else {  // We are adding a link, disable the delete button.
      $(IDS.DELETE).disabled = true;
      disableSubmitUntilTextInput();
    }
  }
}


/**
 * Does some initialization and shows the dialog window.
 */
function init() {
  // Parse query arguments.
  let query = window.location.search.substring(1).split('&');
  queryArgs = {};
  for (let i = 0; i < query.length; ++i) {
    let val = query[i].split('=');
    if (val[0] == '')
      continue;
    queryArgs[decodeURIComponent(val[0])] = decodeURIComponent(val[1]);
  }

  // Enable RTL.
  // TODO(851293): Add RTL formatting.
  if (queryArgs['rtl'] == '1') {
    let html = document.querySelector('html');
    html.dir = 'rtl';
  }

  // Populate text content.
  $(IDS.DIALOG_TITLE).textContent = queryArgs['title'];
  $(IDS.TITLE_FIELD_NAME).textContent = queryArgs['nameField'];
  $(IDS.TITLE_FIELD_NAME).name = queryArgs['nameField'];
  $(IDS.URL_FIELD_NAME).textContent = queryArgs['urlField'];
  $(IDS.URL_FIELD_NAME).name = queryArgs['urlField'];
  $(IDS.DELETE).textContent = queryArgs['linkRemove'];
  $(IDS.CANCEL).textContent = queryArgs['linkCancel'];
  $(IDS.DONE).textContent = queryArgs['linkDone'];
  $(IDS.INVALID_URL).textContent = queryArgs['invalidUrl'];

  // Set up event listeners.
  $(IDS.DELETE).addEventListener('click', deleteLink);
  $(IDS.CANCEL).addEventListener('click', closeDialog);
  $(IDS.FORM).addEventListener('submit', (event) => {
    // Prevent the form from submitting and modifying the URL.
    event.preventDefault();
    finishEditLink(event);
  });

  // Change input field name to blue on input field focus.
  let changeColor = (fieldTitle) => {
    $(fieldTitle).classList.toggle('focused');
  };
  $(IDS.TITLE_FIELD)
      .addEventListener('focusin', () => changeColor(IDS.TITLE_FIELD_NAME));
  $(IDS.TITLE_FIELD)
      .addEventListener('blur', () => changeColor(IDS.TITLE_FIELD_NAME));
  $(IDS.URL_FIELD)
      .addEventListener('focusin', () => changeColor(IDS.URL_FIELD_NAME));
  $(IDS.URL_FIELD)
      .addEventListener('blur', () => changeColor(IDS.URL_FIELD_NAME));

  $(IDS.EDIT_DIALOG).showModal();

  window.addEventListener('message', handlePostMessage);
}


window.addEventListener('DOMContentLoaded', init);

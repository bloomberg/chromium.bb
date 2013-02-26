// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Support for omnibox behavior in offline mode or when API
 * features are not supported on the server.
 */

// ==========================================================
//  Enums.
// ==========================================================

/**
 * Possible behaviors for navigateContentWindow.
 * @enum {number}
 */
var WindowOpenDisposition = {
  CURRENT_TAB: 1,
  NEW_BACKGROUND_TAB: 2
};

/**
 * The JavaScript button event value for a middle click.
 * @type {number}
 * @const
 */
var MIDDLE_MOUSE_BUTTON = 1;

// =============================================================================
//                               Util functions
// =============================================================================

/**
 * The maximum number of suggestions to show.
 * @type {number}
 * @const
 */
var MAX_SUGGESTIONS_TO_SHOW = 5;

/**
 * The omnibox input value during the last onnativesuggestions event.
 * @type {string}
 */
var lastInputValue = '';

/**
 * The ordered restricted ids of the currently displayed suggestions.  Since the
 * suggestions contain the user's personal data (browser history) the searchBox
 * API embeds the content of the suggestion in a shadow dom, and assigns a
 * random restricted id to each suggestion which is accessible to the JS.
 * @type {Array.<number>}
 */

var restrictedIds = [];

/**
 * The index of the currently selected suggestion or -1 if none are selected.
 * @type {number}
 */
var selectedIndex = -1;

/**
 * Displays a suggestion.
 * @param {Object} suggestion The suggestion to render.
 * @param {HTMLElement} box The html element to add the suggestion to.
 * @param {boolean} select True to select the selection.
 */
function addSuggestionToBox(suggestion, box, select) {
  var suggestionDiv = document.createElement('div');
  suggestionDiv.classList.add('suggestion');
  if (select)
    suggestionDiv.classList.add('selected');

  var contentsContainer = document.createElement('div');
  contentsContainer.className = 'contents';
  var contents = suggestion.combinedNode;
  contentsContainer.appendChild(contents);
  suggestionDiv.appendChild(contentsContainer);
  var restrictedId = suggestion.rid;
  suggestionDiv.onclick = function(event) {
    handleSuggestionClick(restrictedId, event);
  };

  restrictedIds.push(restrictedId);
  box.appendChild(suggestionDiv);
}

/**
 * Renders the input suggestions.
 * @param {Array} nativeSuggestions An array of native suggestions to render.
 */
function renderSuggestions(nativeSuggestions) {
  var box = document.createElement('div');
  box.id = 'suggestionsBox';
  $('suggestions-box-container').appendChild(box);

  for (var i = 0, length = nativeSuggestions.length;
       i < Math.min(MAX_SUGGESTIONS_TO_SHOW, length); ++i) {
    // Select the first suggestion.
    addSuggestionToBox(nativeSuggestions[i], box, i == 0);
  }
}

/**
 * Clears the suggestions being displayed.
 */
function clearSuggestions() {
  $('suggestions-box-container').innerHTML = '';
  restrictedIds = [];
  selectedIndex = -1;
}

/**
 * @return {integer} The height of the dropdown.
 */
function getDropdownHeight() {
  return $('suggestions-box-container').offsetHeight;
}

/**
 * Updates selectedIndex, bounding it between -1 and the total number of
 * of suggestions - 1 (looping as necessary), and selects the corresponding
 * suggestion.
 * @param {boolean} increment True to increment the selected suggestion, false
 *     to decrement.
 */
function updateSelectedSuggestion(increment) {
  var numSuggestions = restrictedIds.length;
  if (!numSuggestions)
    return;

  var oldSelection = $('suggestionsBox').querySelector('.selected');
  if (oldSelection)
    oldSelection.classList.remove('selected');

  if (increment)
    selectedIndex = ++selectedIndex > numSuggestions - 1 ? -1 : selectedIndex;
  else
    selectedIndex = --selectedIndex < -1 ? numSuggestions - 1 : selectedIndex;
  var apiHandle = getApiObjectHandle();
  if (selectedIndex == -1) {
    apiHandle.setValue(lastInputValue);
  } else {
    var newSelection = $('suggestionsBox').querySelector(
        '.suggestion:nth-of-type(' + (selectedIndex + 1) + ')');
    newSelection.classList.add('selected');
    apiHandle.setRestrictedValue(restrictedIds[selectedIndex]);
  }
}

// =============================================================================
//                             Handlers / API stuff
// =============================================================================

/**
 * @return {Object} the handle to the searchBox API.
 */
 function getApiObjectHandle() {
  if (window.cideb)
    return window.cideb;
  if (window.navigator && window.navigator.embeddedSearch &&
      window.navigator.embeddedSearch.searchBox)
    return window.navigator.embeddedSearch.searchBox;
  if (window.chrome && window.chrome.embeddedSearch &&
      window.chrome.embeddedSearch.searchBox)
    return window.chrome.embeddedSearch.searchBox;
  return null;
}

/**
 * chrome.searchBox.onnativesuggestions implementation.
 */
function handleNativeSuggestions() {
  var apiHandle = getApiObjectHandle();

  // Used to workaround repeated undesired asynchronous onnativesuggestions
  // events and the fact that when a suggestion is clicked, the omnibox unfocus
  // can cause onnativesuggestions to fire, preventing the suggestion onclick
  // from registering.
  if (lastInputValue == apiHandle.value && $('suggestionsBox')) {
    return;
  }
  lastInputValue = apiHandle.value;

  clearSuggestions();
  var nativeSuggestions = apiHandle.nativeSuggestions;
  if (nativeSuggestions.length) {
    nativeSuggestions.sort(function(a, b) {
      return b.rankingData.relevance - a.rankingData.relevance;
    });
    renderSuggestions(nativeSuggestions);
    selectedIndex = 0;
    apiHandle.setRestrictedAutocompleteText(
        nativeSuggestions[selectedIndex].rid);
  }

  var height = getDropdownHeight();
  // TODO(jered): Remove deprecated "reason" argument.
  apiHandle.showOverlay(2, height);
}

/**
 * Appends a style node for suggestion properties that depend on apiHandle.
 */
function appendSuggestionStyles() {
  var apiHandle = getApiObjectHandle();
  var style = document.createElement('style');
  style.type = 'text/css';
  style.id = 'suggestionStyle';
  style.textContent =
      '.suggestion {' +
      '  -webkit-margin-start: ' + apiHandle.startMargin + 'px;' +
      '  -webkit-margin-end: ' + apiHandle.endMargin + 'px;' +
      '  font: ' + apiHandle.fontSize + 'px "' + apiHandle.font + '";' +
      '}';
  document.querySelector('head').appendChild(style);
}

/**
 * Extract the desired navigation behavior from a click event.
 * @param {Event} event The click event.
 * @return {WindowOpenDisposition} The desired behavior for
 *     navigateContentWindow.
 */
function getDispositionFromClick(event) {
  if (event.button == MIDDLE_MOUSE_BUTTON)
    return WindowOpenDisposition.NEW_BACKGROUND_TAB;
  return WindowOpenDisposition.CURRENT_TAB;
}

/**
 * Handles suggestion clicks.
 * @param {integer} restrictedId The restricted id of the suggestion being
 *     clicked.
 * @param {Event} event The click event.
 */
function handleSuggestionClick(restrictedId, event) {
  clearSuggestions();
  getApiObjectHandle().navigateContentWindow(restrictedId,
                                             getDispositionFromClick(event));
}

/**
 * chrome.searchBox.onkeypress implementation.
 * @param {Object} e The key being pressed.
 */
function handleKeyPress(e) {
  switch (e.keyCode) {
    case 38:  // Up arrow
      updateSelectedSuggestion(false);
      break;
    case 40:  // Down arrow
      updateSelectedSuggestion(true);
      break;
  }
}

/**
 * chrome.searchBox.onsubmit implementation.
 */
function onSubmit() {
}

/**
 * Sets up the searchBox API.
 */
function setUpApi() {
  var apiHandle = getApiObjectHandle();
  apiHandle.onnativesuggestions = handleNativeSuggestions;
  apiHandle.onkeypress = handleKeyPress;
  apiHandle.onsubmit = onSubmit;
  appendSuggestionStyles();

  if (apiHandle.nativeSuggestions.length)
    handleNativeSuggestions();
}

document.addEventListener('DOMContentLoaded', setUpApi);

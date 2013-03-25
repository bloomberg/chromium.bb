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
 * Assume any native suggestion with a score higher than this value has been
 * inlined by the browser.
 * @type {number}
 * @const
 */
var INLINE_SUGGESTION_THRESHOLD = 1200;

/**
 * Suggestion provider type corresponding to a verbatim URL suggestion.
 * @type {string}
 * @const
 */
var VERBATIM_URL_TYPE = 'url-what-you-typed';

/**
 * Suggestion provider type corresponding to a verbatim search suggestion.
 * @type {string}
 * @const
 */
var VERBATIM_SEARCH_TYPE = 'search-what-you-typed';

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
 * Shortcut for document.getElementById.
 * @param {string} id of the element.
 * @return {HTMLElement} with the id.
 */
function $(id) {
  return document.getElementById(id);
}

/**
 * Displays a suggestion.
 * @param {Object} suggestion The suggestion to render.
 * @param {HTMLElement} box The html element to add the suggestion to.
 * @param {boolean} select True to select the selection.
 */
function addSuggestionToBox(suggestion, box, select) {
  var suggestionDiv = document.createElement('div');
  suggestionDiv.classList.add('suggestion');
  suggestionDiv.classList.toggle('selected', select);
  suggestionDiv.classList.toggle('search', suggestion.is_search);

  var suggestionIframe = document.createElement('iframe');
  suggestionIframe.className = 'contents';
  suggestionIframe.src = suggestion.destination_url;
  suggestionIframe.id = suggestion.rid;
  suggestionDiv.appendChild(suggestionIframe);

  restrictedIds.push(suggestion.rid);
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
    addSuggestionToBox(nativeSuggestions[i], box, i == selectedIndex);
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
 * @return {number} The height of the dropdown.
 */
function getDropdownHeight() {
  return $('suggestions-box-container').offsetHeight;
}

/**
 * @param {Object} suggestion A suggestion.
 * @param {boolean} inVerbatimMode Are we in verbatim mode?
 * @return {boolean} True if the suggestion should be selected.
 */
function shouldSelectSuggestion(suggestion, inVerbatimMode) {
  var isVerbatimUrl = suggestion.type == VERBATIM_URL_TYPE;
  var inlinableSuggestion = suggestion.type != VERBATIM_SEARCH_TYPE &&
      suggestion.rankingData.relevance > INLINE_SUGGESTION_THRESHOLD;
  // Verbatim URLs should always be selected. Otherwise, select suggestions
  // with a high enough score unless we are in verbatim mode (e.g. backspacing
  // away).
  return isVerbatimUrl || (!inVerbatimMode && inlinableSuggestion);
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
 * Updates suggestions in response to a onchange or onnativesuggestions call.
 */
function updateSuggestions() {
  var apiHandle = getApiObjectHandle();
  lastInputValue = apiHandle.value;

  clearSuggestions();
  var nativeSuggestions = apiHandle.nativeSuggestions;
  if (nativeSuggestions.length) {
    nativeSuggestions.sort(function(a, b) {
      return b.rankingData.relevance - a.rankingData.relevance;
    });
    if (shouldSelectSuggestion(nativeSuggestions[0], apiHandle.verbatim))
      selectedIndex = 0;
    renderSuggestions(nativeSuggestions);
  }

  var height = getDropdownHeight();
  apiHandle.showOverlay(height);
}

/**
 * Appends a style node for suggestion properties that depend on apiHandle.
 */
function appendSuggestionStyles() {
  var apiHandle = getApiObjectHandle();
  var isRtl = apiHandle.rtl;
  var startMargin = apiHandle.startMargin;
  var style = document.createElement('style');
  style.type = 'text/css';
  style.id = 'suggestionStyle';
  style.textContent =
      '.suggestion, ' +
      '.suggestion.search {' +
      '  background-position: ' +
          (isRtl ? '-webkit-calc(100% - 5px)' : '5px') + ' 4px;' +
      '  -webkit-margin-start: ' + startMargin + 'px;' +
      '  -webkit-margin-end: ' +
          (window.innerWidth - apiHandle.width - startMargin) + 'px;' +
      '  font: ' + apiHandle.fontSize + 'px "' + apiHandle.font + '";' +
      '}';
  document.querySelector('head').appendChild(style);
  window.removeEventListener('resize', appendSuggestionStyles);
}

/**
 * Extract the desired navigation behavior from a click button.
 * @param {number} button The Event#button property of a click event.
 * @return {WindowOpenDisposition} The desired behavior for
 *     navigateContentWindow.
 */
function getDispositionFromClickButton(button) {
  if (button == MIDDLE_MOUSE_BUTTON)
    return WindowOpenDisposition.NEW_BACKGROUND_TAB;
  return WindowOpenDisposition.CURRENT_TAB;
}

/**
 * Handles suggestion clicks.
 * @param {number} restrictedId The restricted id of the suggestion being
 *     clicked.
 * @param {number} button The Event#button property of a click event.
 *
 */
function handleSuggestionClick(restrictedId, button) {
  clearSuggestions();
  getApiObjectHandle().navigateContentWindow(
      restrictedId, getDispositionFromClickButton(button));
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
 * Handles the postMessage calls from the result iframes.
 * @param {Object} message The message containing details of clicks the iframes.
 */
function handleMessage(message) {
  if (message.origin != 'null' || !message.data ||
      message.data.eventType != 'click') {
    return;
  }

  var iframes = document.getElementsByClassName('contents');
  for (var i = 0; i < iframes.length; ++i) {
    if (iframes[i].contentWindow == message.source) {
      handleSuggestionClick(parseInt(iframes[i].id, 10),
                            message.data.button);
      break;
    }
  }
}

/**
 * chrome.searchBox.embeddedSearch.onsubmit implementation.
 */
function onSubmit() {
}

/**
 * Sets up the searchBox API.
 */
function setUpApi() {
  var apiHandle = getApiObjectHandle();
  apiHandle.onnativesuggestions = updateSuggestions;
  apiHandle.onchange = updateSuggestions;
  apiHandle.onkeypress = handleKeyPress;
  apiHandle.onsubmit = onSubmit;
  $('suggestions-box-container').dir = apiHandle.rtl ? 'rtl' : 'ltr';
  // Delay adding these styles until the window width is available.
  window.addEventListener('resize', appendSuggestionStyles);
  if (apiHandle.nativeSuggestions.length)
    handleNativeSuggestions();
}

document.addEventListener('DOMContentLoaded', setUpApi);
window.addEventListener('message', handleMessage, false);

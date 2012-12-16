// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// =============================================================================
//                               Util functions
// =============================================================================

// The maximum number of suggestions to show.
var MAX_SUGGESTIONS_TO_SHOW = 5;

/**
 * Displays a suggestion.
 * @param {Object} suggestion The suggestion to render.
 * @param {HTMLElement} box The html element to add the suggestion to.
 * @param {integer} suggestionRank The rank of the suggestion (0 based).
 */
function addSuggestionToBox(suggestion, box, suggestionRank) {
  var suggestionDiv = document.createElement('div');
  suggestionDiv.classList.add('suggestion');
  if (suggestionRank == 0)
    suggestionDiv.classList.add('selected');

  var clock = document.createElement('div');
  clock.className = 'clock';
  suggestionDiv.appendChild(clock);

  var contentsContainer = document.createElement('div');
  contentsContainer.className = 'contents';
  var contents = suggestion.combinedNode;
  var restrictedId = suggestion.rid;
  contentsContainer.appendChild(contents);
  suggestionDiv.appendChild(contentsContainer);
  suggestionDiv.onclick = function() {
    handleSuggestionClick(restrictedId);
  }

  // TODO(shishir): Support hover over suggestions.
  var restrictedIdDiv = document.createElement('div');
  restrictedIdDiv.innerHTML = restrictedId;
  restrictedIdDiv.className = 'restricted-id';
  suggestionDiv.appendChild(restrictedIdDiv);

  box.appendChild(suggestionDiv);
}

/**
 * Renders the input suggestions.
 * @param {Array} nativeSuggestions An array of native suggestions to render.
 */
function renderSuggestions(nativeSuggestions) {
  clearSuggestions();

  var box = $('suggestionsBox');
  for (var i = 0; i < MAX_SUGGESTIONS_TO_SHOW && i < nativeSuggestions.length;
       ++i) {
    addSuggestionToBox(nativeSuggestions[i], box, i);
  }
}

/**
 * Clears the suggestions being displayed.
 */
function clearSuggestions() {
  $('suggestionsBox').innerHTML = '';
}

/**
 * @return {integer} The height of the dropdown.
 */
function getDropdownHeight() {
  return $('suggestionsBox').offsetHeight;
}

/**
 * @return {integer} the index of the suggestion currently selected.
 */
function getSelectedSuggestionIndex() {
  var suggestions = $('suggestionsBox').childNodes;
  for (var i = 0; i < suggestions.length; ++i) {
    if (suggestions[i].classList.contains('selected'))
      return i;
  }
  return -1;
}

/**
 * Changes the selected suggestion.
 * @param {integer} index The index of the suggestion to select.
 * @param {function} restrictedIdCallback Callback to call on old selection.
 */
function selectSuggestionAtIndex(index, restrictedIdCallback) {
  var oldSelection = $('suggestionsBox').querySelector('.selected');
  oldSelection.classList.remove('selected');

  var numSuggestions = $('suggestionsBox').childNodes.length;
  var sanitizedIndex = Math.min(Math.max(0, index), numSuggestions - 1);
  var selection = sanitizedIndex + 1;
  var newSelection = $('suggestionsBox').querySelector(
      '.suggestion:nth-of-type(' + selection + ')');
  newSelection.classList.add('selected');
  var restrictedId = getRestrictedId(newSelection);
  restrictedIdCallback(restrictedId);
}

/**
 * Returns the restricted id for the input suggestion. Since the suggestions
 * contain the user's personal data (browser history) the searchBox API embedds
 * the content of the suggestion in a shadow dom, and assigns a random id to
 * each suggestion called restricted id which is accessible to the js.
 *
 * @param {HTMLElement} suggestion The node representing the suggestion.
 * @return {integer} The restricted id of the suggestion.
 */
function getRestrictedId(suggestion) {
  for (var i = 0; i < suggestion.childNodes.length; ++i) {
    if (suggestion.childNodes[i].classList.contains('restricted-id'))
      return parseInt(suggestion.childNodes[i].innerHTML);
  }
  return -1;
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
  if (window.navigator && window.navigator.searchBox)
    return window.navigator.searchBox;
  if (window.chrome && window.chrome.searchBox)
    return window.chrome.searchBox;
  return null;
}

/**
 * chrome.searchBox.onnativesuggestions implementation.
 */
function handleNativeSuggestions() {
  var apiHandle = getApiObjectHandle();

  var nativeSuggestions = apiHandle.nativeSuggestions;
  if (nativeSuggestions) {
    nativeSuggestions.sort(function(a, b) {
      return b.rankingData.relevance - a.rankingData.relevance;
    });
    renderSuggestions(nativeSuggestions);
  } else {
    clearSuggestions();
  }

  var height = getDropdownHeight();
  apiHandle.show(2, height);

  if (nativeSuggestions && nativeSuggestions.length > 0) {
    apiHandle.setRestrictedAutocompleteText(
        nativeSuggestions[getSelectedSuggestionIndex()].rid);
  }
}

/**
 * Handles suggestion clicks.
 * @param {integer} restrictedId The restricted id of the suggestion being
 *     clicked.
 */
function handleSuggestionClick(restrictedId) {
  var apiHandle = getApiObjectHandle();
  clearSuggestions();
  apiHandle.navigateContentWindow(restrictedId);
}

/**
 * chrome.searchBox.onkeypress implementation.
 * @param {Object} e The key being pressed.
 */
function handleKeyPress(e) {
  var apiHandle = getApiObjectHandle();
  function callback(restrictedId) {
    apiHandle.setRestrictedValue(restrictedId);
  }

  switch (e.keyCode) {
    case 38:  // Up arrow
      selectSuggestionAtIndex(getSelectedSuggestionIndex() - 1, callback);
      break;
    case 40:  // Down arrow
      selectSuggestionAtIndex(getSelectedSuggestionIndex() + 1, callback);
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
}

document.addEventListener('DOMContentLoaded', setUpApi);

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Orchestrates loading of suggestion content in several
 * chrome-search://suggestion iframes.
 */

(function() {
<include src="local_ntp/instant_iframe_validation.js">

/**
 * The origin of the embedding page.
 * This string literal must be in double quotes for proper escaping.
 * @type {string}
 * @const
 */
var EMBEDDER_ORIGIN = "{{ORIGIN}}";

/**
 * Converts an RGB color number to a hex color string if valid.
 * @param {number} color A 6-digit hex RGB color code as a number.
 * @return {?string} A CSS representation of the color or null if invalid.
 */
function convertColor(color) {
  // Color must be a number, finite, with no fractional part, in the correct
  // range for an RGB hex color.
  if (isFinite(color) && Math.floor(color) == color &&
      color >= 0 && color <= 0xffffff) {
    var hexColor = color.toString(16);
    // Pads with initial zeros and # (e.g. for 'ff' yields '#0000ff').
    return '#000000'.substr(0, 7 - hexColor.length) + hexColor;
  }
  return null;
}

/**
 * Checks and returns suggestion style.
 * @param {!Object} pageStyle Instant page-specified overrides for suggestion
 *     styles.
 * @return {!Object} Checked styles or defaults.
 */
function getStyle(pageStyle) {
  var apiHandle = chrome.embeddedSearch.searchBox;
  var style = {
    queryColor: '#000000',
    urlColor: '#009933',
    titleColor: '#666666',
    font: apiHandle.font,
    fontSize: apiHandle.fontSize,
    isRtl: apiHandle.rtl
  };
  if ('queryColor' in pageStyle)
    style.queryColor = convertColor(pageStyle.queryColor) || style.queryColor;
  if ('urlColor' in pageStyle)
    style.urlColor = convertColor(pageStyle.urlColor) || style.urlColor;
  if ('titleColor' in pageStyle)
    style.titleColor = convertColor(pageStyle.titleColor) || style.titleColor;
  return style;
}

/**
 * Renders a native history suggestion.
 * @param {!Document} resultDoc The suggestion template document.
 * @param {!Object} suggestion The NativeSuggestion to render.
 * @param {!Object} pageStyle Page-specificed styles.
 */
function updateResult(resultDoc, suggestion, pageStyle) {
  var style = getStyle(pageStyle);
  resultDoc.body.dir = 'auto';
  resultDoc.body.style.fontSize = style.fontSize + 'px';
  resultDoc.body.style.fontFamily = style.font;
  resultDoc.body.style.textAlign = style.isRtl ? 'right' : 'left';
  var contentsNode = resultDoc.querySelector('#contents');
  contentsNode.textContent = suggestion.contents;
  contentsNode.style.color = suggestion.is_search ?
      style.queryColor : style.urlColor;
  var optionalNode = resultDoc.querySelector('#optional');
  optionalNode.hidden = !suggestion.description;
  if (suggestion.description) {
    var titleNode = resultDoc.querySelector('#title');
    titleNode.textContent = suggestion.description;
    optionalNode.style.color = style.titleColor;
  }
}

/**
 * Handles a postMessage from the embedding page requesting to populate history
 * suggestion iframes.
 * @param {!Object} message The message.
 */
function handleMessage(message) {
  // Only allow messages from the embedding page, which should be an Instant
  // search provider or the local omnibox dropdown (and not e.g. a site which
  // it has iframed.)
  if (message.origin != EMBEDDER_ORIGIN)
    return;

  var apiHandle = chrome.embeddedSearch.searchBox;
  if ('load' in message.data) {
    for (var iframeId in message.data.load) {
      var restrictedId = message.data.load[iframeId];
      var suggestion = apiHandle.getSuggestionData(restrictedId);
      var iframe = window.parent.frames[iframeId];
      if (iframe)
        updateResult(iframe.document, suggestion, message.data.style || {});
    }
    message.source.postMessage(
        {loaded: message.data.requestId}, message.origin);
  }
}

window.addEventListener('message', handleMessage, false);
})();

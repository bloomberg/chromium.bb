// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Callback from the backend with the list of variations to display.
 * This call will build the variations section of the version page, or hide that
 * section if there are none to display.
 * @param {!Array.<string>} variationsList The list of variations.
 */
function returnVariationsList(variationsList) {
  $('variations-section').hidden = !variationsList.length;
  $('variations-list').appendChild(
      parseHtmlSubset(variationsList.join('<br>'), ['BR']));
}

/* All the work we do onload. */
function onLoadWork() {
  // This is the javascript code that processes the template:
  var input = new JsEvalContext(templateData);
  var output = $('t');
  jstProcess(input, output);

  chrome.send('requestVariationsList');
}

document.addEventListener('DOMContentLoaded', onLoadWork);

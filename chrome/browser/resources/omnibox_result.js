// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Click handler for omnibox results.
 */

(function() {
/**
 * The origin of the embedding page.
 * This string literal must be in double quotes for proper escaping.
 * @type {string}
 * @const
 */
var EMBEDDER_ORIGIN = "{{ORIGIN}}";

window.addEventListener('click', function(event) {
  top.postMessage({click: event.button}, EMBEDDER_ORIGIN);
});
})();

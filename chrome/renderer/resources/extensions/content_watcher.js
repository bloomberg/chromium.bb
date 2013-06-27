// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var contentWatcherNative = requireNative("contentWatcherNative");

// Returns the indices in |cssSelectors| that match any element on the page.
exports.FindMatchingSelectors = function(cssSelectors) {
  var result = []
  $Array.forEach(cssSelectors, function(selector, index) {
    try {
      if (document.querySelector(selector) != null)
        $Array.push(result, index);
    } catch (exception) {
      throw new Error("query Selector failed on '" + selector + "': " +
                      exception.stack);
    }
  });
  return result;
};

// Watches the page for all changes and calls FrameMutated (a C++ callback) in
// response.
var mutation_observer = new MutationObserver(contentWatcherNative.FrameMutated);

// This runs once per frame, when the module is 'require'd.
mutation_observer.observe(document, {
  childList: true,
  attributes: true,
  characterData: true,
  subtree: true});

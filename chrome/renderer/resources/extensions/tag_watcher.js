// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Expose a function to watch the HTML tag creation via Mutation Observers.
function watchForTag(tagName, cb) {
  if (!document.body)
    return;

  // Query tags already in the document.
  var nodes = document.body.querySelectorAll(tagName);
  for (var i = 0, node; node = nodes[i]; i++) {
    cb(node);
  }

  // Observe the tags added later.
  var documentObserver = new WebKitMutationObserver(function(mutations) {
    mutations.forEach(function(mutation) {
      for (var i = 0, addedNode; addedNode = mutation.addedNodes[i]; i++) {
        if (addedNode.tagName == tagName) {
          cb(addedNode);
        }
      }
    });
  });
  documentObserver.observe(document, {subtree: true, childList: true});
}

exports.watchForTag = watchForTag;


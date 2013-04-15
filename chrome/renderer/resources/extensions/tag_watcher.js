// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var forEach = require('utils').forEach;

// Expose a function to watch the HTML tag creation via Mutation Observers.
function watchForTag(tagName, cb) {
  if (!document.body)
    return;

  function findChildTags(queryNode) {
    forEach(queryNode.querySelectorAll(tagName), function(i, node) {
      cb(node);
    });
  }
  // Query tags already in the document.
  findChildTags(document.body);

  // Observe the tags added later.
  var documentObserver = new WebKitMutationObserver(function(mutations) {
    forEach(mutations, function(i, mutation) {
      forEach(mutation.addedNodes, function(i, addedNode) {
        if (addedNode.nodeType == Node.ELEMENT_NODE) {
          if (addedNode.tagName == tagName)
            cb(addedNode);
          findChildTags(addedNode);
        }
      });
    });
  });
  documentObserver.observe(document, {subtree: true, childList: true});
}

exports.watchForTag = watchForTag;

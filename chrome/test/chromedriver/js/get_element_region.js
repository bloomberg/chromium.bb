// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function getElementRegion(element) {
  // Check that node type is element.
  if (element.nodeType != 1)
    throw new Error(element + ' is not an element');

  // We try 2 methods to determine element region. Try the first client rect,
  // and then the bounding client rect.
  // SVG is one case that doesn't have a first client rect.
  var clientRects = element.getClientRects();
  if (clientRects.length == 0) {
    var box = element.getBoundingClientRect();
    return {
        'left': 0,
        'top': 0,
        'width': box.width,
        'height': box.height
    };
  } else {
    var clientRect = clientRects[0];
    var box = element.getBoundingClientRect();
    return {
        'left': clientRect.left - box.left,
        'top': clientRect.top - box.top,
        'width': clientRect.right - clientRect.left,
        'height': clientRect.bottom - clientRect.top
    };
  }
}

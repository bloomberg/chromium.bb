// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function getInViewCenterPoint(rect) {
  var left = Math.max(0, rect.left);
  var right = Math.min(window.innerWidth, rect.right);
  var top = Math.max(0, rect.top);
  var bottom = Math.min(window.innerHeight, rect.bottom);

  var x = 0.5 * (left + right);
  var y = 0.5 * (top + bottom);

  return [x, y];
}

function inView(element) {
  if (!window.document.contains(element)) {
    return false;
  }

  var rectangles = element.getClientRects();
  if (rectangles.length === 0) {
    return false;
  }

  var centerPoint = getInViewCenterPoint(rectangles[0]);
  if (centerPoint[0] <= 0 || centerPoint[1] <= 0 ||
      centerPoint[0] >= window.innerWidth ||
      centerPoint[1] >= window.innerHeight) {
    return false;
  }

  return true;
}

function getElementCenterLocation(element) {
  // Check that node type is element.
  if (element.nodeType != 1)
    throw new Error(element + ' is not an element');

  if (!window.document.contains(element)) {
    throw new Error("element in different document or shadow tree");
  }

  if (!inView(element)) {
    element.scrollIntoView({behavior: "instant",
                            block: "end",
                            inline: "nearest"});
  }

  var rect = element.getClientRects()[0];
  var centerPoint = getInViewCenterPoint(rect);
  return {
      'x': centerPoint[0],
      'y': centerPoint[1]
  };
}
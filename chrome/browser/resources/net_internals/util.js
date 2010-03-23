// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Helper that binds the |this| object to a method to create a callback.
 */
Function.prototype.bind = function(thisObj) {
  var func = this;
  var args = Array.prototype.slice.call(arguments, 1);
  return function() {
    return func.apply(thisObj,
    args.concat(Array.prototype.slice.call(arguments, 0)))
  };
};

/**
 * Sets the width (in pixels) on a DOM node.
 */
function setNodeWidth(node, widthPx) {
  node.style.width = widthPx.toFixed(0) + "px";
}

/**
 * Sets the height (in pixels) on a DOM node.
 */
function setNodeHeight(node, heightPx) {
  node.style.height = heightPx.toFixed(0) + "px";
}

/**
 * Sets the position and size of a DOM node (in pixels).
 */
function setNodePosition(node, leftPx, topPx, widthPx, heightPx) {
  node.style.left = leftPx.toFixed(0) + "px";
  node.style.top = topPx.toFixed(0) + "px";
  setNodeWidth(node, widthPx);
  setNodeHeight(node, heightPx);
}

/**
 * Adds a node to |parentNode|, of type |tagName|.
 */
function addNode(parentNode, tagName) {
  var elem = parentNode.ownerDocument.createElement(tagName);
  parentNode.appendChild(elem);
  return elem;
}

/**
 * Adds text to node |parentNode|.
 */
function addTextNode(parentNode, text) {
  var textNode = parentNode.ownerDocument.createTextNode(text);
  parentNode.appendChild(textNode);
  return textNode;
}

/**
 * Adds or removes a CSS class to |node|.
 */
function changeClassName(node, classNameToAddOrRemove, isAdd) {
  // Multiple classes can be separated by spaces.
  var currentNames = node.className.split(" ");

  if (isAdd) {
    if (!(classNameToAddOrRemove in currentNames)) {
      currentNames.push(classNameToAddOrRemove);
    }
  } else {
    for (var i = 0; i < currentNames.length; ++i) {
      if (currentNames[i] == classNameToAddOrRemove) {
        currentNames.splice(i, 1);
        break;
      }
    }
  }

  node.className = currentNames.join(" ");
}

function getKeyWithValue(map, value) {
  for (key in map) {
    if (map[key] == value)
      return key;
  }
  return '?';
}

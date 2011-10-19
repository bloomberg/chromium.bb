// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Sets the width (in pixels) on a DOM node.
 */
function setNodeWidth(node, widthPx) {
  node.style.width = widthPx.toFixed(0) + 'px';
}

/**
 * Sets the height (in pixels) on a DOM node.
 */
function setNodeHeight(node, heightPx) {
  node.style.height = heightPx.toFixed(0) + 'px';
}

/**
 * Sets the position and size of a DOM node (in pixels).
 */
function setNodePosition(node, leftPx, topPx, widthPx, heightPx) {
  node.style.left = leftPx.toFixed(0) + 'px';
  node.style.top = topPx.toFixed(0) + 'px';
  setNodeWidth(node, widthPx);
  setNodeHeight(node, heightPx);
}

/**
 * Sets the visibility for a DOM node.
 */
function setNodeDisplay(node, isVisible) {
  node.style.display = isVisible ? '' : 'none';
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
 * Adds |text| to node |parentNode|.
 */
function addTextNode(parentNode, text) {
  var textNode = parentNode.ownerDocument.createTextNode(text);
  parentNode.appendChild(textNode);
  return textNode;
}

/**
 * Adds a node to |parentNode|, of type |tagName|.  Then adds
 * |text| to the new node.
 */
function addNodeWithText(parentNode, tagName, text) {
  var elem = parentNode.ownerDocument.createElement(tagName);
  parentNode.appendChild(elem);
  addTextNode(elem, text);
  return elem;
}

/**
 * Adds or removes a CSS class to |node|.
 */
function changeClassName(node, classNameToAddOrRemove, isAdd) {
  // Multiple classes can be separated by spaces.
  var currentNames = node.className.split(' ');

  if (isAdd) {
    for (var i = 0; i < currentNames.length; ++i) {
      if (currentNames[i] == classNameToAddOrRemove)
        return;
    }
    currentNames.push(classNameToAddOrRemove);
  } else {
    for (var i = 0; i < currentNames.length; ++i) {
      if (currentNames[i] == classNameToAddOrRemove) {
        currentNames.splice(i, 1);
        break;
      }
    }
  }

  node.className = currentNames.join(' ');
}

function getKeyWithValue(map, value) {
  for (var key in map) {
    if (map[key] == value)
      return key;
  }
  return '?';
}

/**
 * Looks up |key| in |map|, and returns the resulting entry, if  there is one.
 * Otherwise, returns |key|.  Intended primarily for use with incomplete
 * tables, and for reasonable behavior with system enumerations that may be
 * extended in the future.
 */
function tryGetValueWithKey(map, key) {
  if (key in map)
    return map[key];
  return key;
}

/**
 * Builds a string by repeating |str| |count| times.
 */
function makeRepeatedString(str, count) {
  var out = [];
  for (var i = 0; i < count; ++i)
    out.push(str);
  return out.join('');
}

/**
 * Clones a basic POD object.  Only a new top level object will be cloned.  It
 * will continue to reference the same values as the original object.
 */
function shallowCloneObject(object) {
  if (!(object instanceof Object))
    return object;
  var copy = {};
  for (var key in object) {
    copy[key] = object[key];
  }
  return copy;
}

/**
 * Helper to make sure singleton classes are not instantiated more than once.
 */
function assertFirstConstructorCall(ctor) {
  // This is the variable which is set by cr.addSingletonGetter().
  if (ctor.hasCreateFirstInstance_) {
    throw Error('The class ' + ctor.name + ' is a singleton, and should ' +
                'only be accessed using ' + ctor.name + '.getInstance().');
  }
  ctor.hasCreateFirstInstance_ = true;
}

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles math output and exploration.
 */

goog.provide('MathHandler');

/**
 * Initializes math for output and exploration.
 * @param {!chrome.automation.AutomationNode} node
 * @constructor
 */
MathHandler = function(node) {
  /** @private {!chrome.automation.AutomationNode} */
  this.node_ = node;

  // A math ml structure can exist either as a data attribute or a full tree. We
  // want the serialization of the tree in the latter case.
  var ns = node.htmlAttributes['xmlns'];
  if (node.role == chrome.automation.RoleType.MATH && ns &&
      ns.toLowerCase().indexOf('mathml') != -1) {
    var mathMlRoot = MathHandler.createMathMlDom_(node);
    this.mathml_ = mathMlRoot.outerHTML;
  } else {
    this.mathml_ = node.htmlAttributes['data-mathml'];
  }
};

MathHandler.prototype = {
  /**
   * Speaks the current node.
   */
  speak: function() {
    cvox.ChromeVox.tts.speak(SRE.walk(this.mathml_), cvox.QueueMode.FLUSH);

    cvox.ChromeVox.tts.speak(
        Msgs.getMsg('hint_math_keyboard'), cvox.QueueMode.QUEUE);
  }
};

/**
 * The global instance.
 * @type {MathHandler|undefined}
 */
MathHandler.instance;

/**
 * Initializes the global instance.
 * @param {cursors.Range} range
 * @return {boolean} True if an instance was created.
 */
MathHandler.init = function(range) {
  var node = range.start.node;
  if (node && AutomationPredicate.math(node))
    MathHandler.instance = new MathHandler(node);
  else
    MathHandler.instance = undefined;
  return !!MathHandler.instance;
};

/**
 * Handles key events.
 * @return {boolean} False to prevent further event propagation.
 */
MathHandler.onKeyDown = function(evt) {
  if (!MathHandler.instance)
    return true;

  if (evt.ctrlKey || evt.altKey || evt.metaKey || evt.shiftKey ||
      evt.stickyMode)
    return true;

  var instance = MathHandler.instance;
  var output = SRE.move(evt.keyCode);
  if (output)
    cvox.ChromeVox.tts.speak(output, cvox.QueueMode.FLUSH);
  return false;
};

/**
 * @private
 */
MathHandler.createMathMlDom_ = function(node) {
  if (!node.htmlTag && node.role != chrome.automation.RoleType.STATIC_TEXT)
    return null;

  var domNode;
  if (node.htmlTag)
    domNode = document.createElement(node.htmlTag);
  else
    domNode = document.createTextNode(node.name);
  for (var key in node.htmlAttributes)
    domNode.setAttribute(key, node.htmlAttributes[key]);

  for (var i = 0; i < node.children.length; i++) {
    var child = MathHandler.createMathMlDom_(node.children[i]);
    if (child)
      domNode.appendChild(child);
  }

  return domNode;
};

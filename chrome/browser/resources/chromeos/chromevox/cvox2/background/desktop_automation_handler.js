// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles automation from a desktop automation node.
 */

goog.provide('DesktopAutomationHandler');

goog.require('Background');
goog.require('BaseAutomationHandler');

goog.scope(function() {
var AutomationEvent = chrome.automation.AutomationEvent;
var AutomationNode = chrome.automation.AutomationNode;
var Dir = AutomationUtil.Dir;
var RoleType = chrome.automation.RoleType;

/**
 * @param {!AutomationNode} node
 * @constructor
 * @extends {BaseAutomationHandler}
 */
DesktopAutomationHandler = function(node) {
  BaseAutomationHandler.call(this, node);

  /**
   * The object that speaks changes to an editable text field.
   * @type {?cvox.ChromeVoxEditableTextBase}
   */
  this.editableTextHandler_ = null;

  // The focused state gets set on the containing webView node.
  var webView = node.find({role: RoleType.webView, state: {focused: true}});
  if (webView) {
    var root = webView.find({role: RoleType.rootWebArea});
    if (root) {
      this.onLoadComplete(
          {target: root,
           type: chrome.automation.EventType.loadComplete});
    }
  }
};

DesktopAutomationHandler.prototype = {
  __proto__: BaseAutomationHandler.prototype,

  /** @override */
  willHandleEvent_: function(evt) {
    return !cvox.ChromeVox.isActive;
  },

  /**
   * Provides all feedback once ChromeVox's focus changes.
   * @param {Object} evt
   */
  onEventDefault: function(evt) {
    var node = evt.target;

    if (!node)
      return;

    var prevRange = global.backgroundObj.currentRange;

    global.backgroundObj.currentRange = cursors.Range.fromNode(node);

    // Check to see if we've crossed roots. Continue if we've crossed roots or
    // are not within web content.
    if (node.root.role == RoleType.desktop ||
        !prevRange ||
        prevRange.start.node.root != node.root)
      global.backgroundObj.refreshMode(node.root.docUrl || '');

    // Don't process nodes inside of web content if ChromeVox Next is inactive.
    if (node.root.role != RoleType.desktop &&
        global.backgroundObj.mode === ChromeVoxMode.CLASSIC) {
      if (cvox.ChromeVox.isChromeOS)
        chrome.accessibilityPrivate.setFocusRing([]);
      return;
    }

    // Don't output if focused node hasn't changed.
    if (prevRange &&
        evt.type == 'focus' &&
        global.backgroundObj.currentRange.equals(prevRange))
      return;

    new Output().withSpeechAndBraille(
            global.backgroundObj.currentRange, prevRange, evt.type)
        .go();
  },

  /**
   * Makes an announcement without changing focus.
   * @param {Object} evt
   */
  onAlert: function(evt) {
    var node = evt.target;
    if (!node)
      return;

    // Don't process nodes inside of web content if ChromeVox Next is inactive.
    if (node.root.role != RoleType.desktop &&
        global.backgroundObj.mode === ChromeVoxMode.CLASSIC) {
      return;
    }

    var range = cursors.Range.fromNode(node);

    new Output().withSpeechAndBraille(range, null, evt.type).go();
  },

  /**
   * Provides all feedback once a focus event fires.
   * @param {Object} evt
   */
  onFocus: function(evt) {
    // Invalidate any previous editable text handler state.
    this.editableTextHandler_ = null;

    var node = evt.target;

    // Discard focus events on embeddedObject nodes.
    if (node.role == RoleType.embeddedObject)
      return;

    // It almost never makes sense to place focus directly on a rootWebArea.
    if (node.role == RoleType.rootWebArea) {
      // Discard focus events for root web areas when focus was previously
      // placed on a descendant.
      var currentRange = global.backgroundObj.currentRange;
      if (currentRange && currentRange.start.node.root == node)
        return;

      // Discard focused root nodes without focused state set.
      if (!node.state.focused)
        return;

      // Try to find a focusable descendant.
      node = node.find({state: {focused: true}}) || node;
    }

    if (this.isEditable_(evt.target))
      this.createEditableTextHandlerIfNeeded_(evt.target);

    this.onEventDefault({target: node, type: 'focus'});
  },

  /**
   * Provides all feedback once a load complete event fires.
   * @param {Object} evt
   */
  onLoadComplete: function(evt) {
    global.backgroundObj.refreshMode(evt.target.docUrl);

    // Don't process nodes inside of web content if ChromeVox Next is inactive.
    if (evt.target.root.role != RoleType.desktop &&
        global.backgroundObj.mode === ChromeVoxMode.CLASSIC)
      return;

    // If initial focus was already placed on this page (e.g. if a user starts
    // tabbing before load complete), then don't move ChromeVox's position on
    // the page.
    if (global.backgroundObj.currentRange &&
        global.backgroundObj.currentRange.start.node.role !=
            RoleType.rootWebArea &&
        global.backgroundObj.currentRange.start.node.root.docUrl ==
            evt.target.docUrl)
      return;

    var root = evt.target;
    var webView = root;
    while (webView && webView.role != RoleType.webView)
      webView = webView.parent;

    if (!webView || !webView.state.focused)
      return;

    var node = AutomationUtil.findNodePost(root,
        Dir.FORWARD,
        AutomationPredicate.leaf);

    if (node)
      global.backgroundObj.currentRange = cursors.Range.fromNode(node);

    if (global.backgroundObj.currentRange)
      new Output().withSpeechAndBraille(
              global.backgroundObj.currentRange, null, evt.type)
          .go();
  },

  /**
   * Provides all feedback once a text selection change event fires.
   * @param {Object} evt
   */
  onTextOrTextSelectionChanged: function(evt) {
    if (!this.isEditable_(evt.target))
      return;

    // Don't process nodes inside of web content if ChromeVox Next is inactive.
    if (evt.target.root.role != RoleType.desktop &&
        global.backgroundObj.mode === ChromeVoxMode.CLASSIC)
      return;

    if (!evt.target.state.focused)
      return;

    if (!global.backgroundObj.currentRange) {
      this.onEventDefault(evt);
      global.backgroundObj.currentRange = cursors.Range.fromNode(evt.target);
    }

    this.createEditableTextHandlerIfNeeded_(evt.target);

    var textChangeEvent = new cvox.TextChangeEvent(
        evt.target.value,
        evt.target.textSelStart,
        evt.target.textSelEnd,
        true);  // triggered by user

    this.editableTextHandler_.changed(textChangeEvent);

    new Output().withBraille(
            global.backgroundObj.currentRange, null, evt.type)
        .go();
  },

  /**
   * Provides all feedback once a value changed event fires.
   * @param {Object} evt
   */
  onValueChanged: function(evt) {
    // Don't process nodes inside of web content if ChromeVox Next is inactive.
    if (evt.target.root.role != RoleType.desktop &&
        global.backgroundObj.mode === ChromeVoxMode.CLASSIC)
      return;

    if (!evt.target.state.focused)
      return;

    // Value change events fire on web editables when typing. Suppress them.
    if (!global.backgroundObj.currentRange || !this.isEditable_(evt.target)) {
      this.onEventDefault(evt);
      global.backgroundObj.currentRange = cursors.Range.fromNode(evt.target);
    }
  },

  /**
   * Handle updating the active indicator when the document scrolls.
   * @override
   */
  onScrollPositionChanged: function(evt) {
    var currentRange = global.backgroundObj.currentRange;
    if (currentRange)
      new Output().withLocation(currentRange, null, evt.type).go();
  },

  /**
   * Create an editable text handler for the given node if needed.
   * @param {Object} node
   */
  createEditableTextHandlerIfNeeded_: function(node) {
    if (!this.editableTextHandler_ ||
        node != global.backgroundObj.currentRange.start.node) {
      var start = node.textSelStart;
      var end = node.textSelEnd;
      if (start > end) {
        var tempOffset = end;
        end = start;
        start = tempOffset;
      }

      this.editableTextHandler_ =
          new cvox.ChromeVoxEditableTextBase(
              node.value,
              start,
              end,
              node.state.protected,
              cvox.ChromeVox.tts);
    }
  },

  /**
   * Returns true if |node| is editable.
   * @param {AutomationNode} node
   * @return {boolean}
   * @private
   */
  isEditable_: function(node) {
    // Remove the check for role after m47 whereafter the editable state can be
    // used to know when to create an editable text handler.
    return node.role == RoleType.textField || node.state.editable;
  }
};

/**
 * Initializes global state for DesktopAutomationHandler.
 * @private
 */
DesktopAutomationHandler.init_ = function() {
  if (cvox.ChromeVox.isMac)
    return;
  chrome.automation.getDesktop(function(desktop) {
    global.desktopAutomationHandler = new DesktopAutomationHandler(desktop);
  });
};

DesktopAutomationHandler.init_();

});  // goog.scope

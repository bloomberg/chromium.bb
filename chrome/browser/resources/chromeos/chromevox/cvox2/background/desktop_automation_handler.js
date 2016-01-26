// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles automation from a desktop automation node.
 */

goog.provide('DesktopAutomationHandler');

goog.require('BaseAutomationHandler');
goog.require('ChromeVoxState');
goog.require('editing.TextEditHandler');

goog.scope(function() {
var AutomationEvent = chrome.automation.AutomationEvent;
var AutomationNode = chrome.automation.AutomationNode;
var Dir = constants.Dir;
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
   * @type {editing.TextEditHandler}
   */
  this.textEditHandler_ = null;

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
   * @param {!AutomationEvent} evt
   */
  onEventDefault: function(evt) {
    var node = evt.target;

    if (!node)
      return;

    var prevRange = ChromeVoxState.instance.currentRange;

    ChromeVoxState.instance.setCurrentRange(cursors.Range.fromNode(node));

    // Check to see if we've crossed roots. Continue if we've crossed roots or
    // are not within web content.
    if (node.root.role == RoleType.desktop ||
        !prevRange ||
        prevRange.start.node.root != node.root)
      ChromeVoxState.instance.refreshMode(node.root.docUrl || '');

    // Don't process nodes inside of web content if ChromeVox Next is inactive.
    if (node.root.role != RoleType.desktop &&
        ChromeVoxState.instance.mode === ChromeVoxMode.CLASSIC) {
      if (cvox.ChromeVox.isChromeOS)
        chrome.accessibilityPrivate.setFocusRing([]);
      return;
    }

    // Don't output if focused node hasn't changed.
    if (prevRange &&
        evt.type == 'focus' &&
        ChromeVoxState.instance.currentRange.equals(prevRange))
      return;

    var output = new Output();
    output.withSpeech(
        ChromeVoxState.instance.currentRange, prevRange, evt.type);
    if (!this.textEditHandler_) {
      output.withBraille(
          ChromeVoxState.instance.currentRange, prevRange, evt.type);
    } else {
      // Delegate event handling to the text edit handler for braille.
      this.textEditHandler_.onEvent(evt);
    }
    output.go();
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
        ChromeVoxState.instance.mode === ChromeVoxMode.CLASSIC) {
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
    this.textEditHandler_ = null;

    var node = evt.target;

    // Discard focus events on embeddedObject and client nodes.
    if (node.role == RoleType.embeddedObject || node.role == RoleType.client)
      return;

    // It almost never makes sense to place focus directly on a rootWebArea.
    if (node.role == RoleType.rootWebArea) {
      // Discard focus events for root web areas when focus was previously
      // placed on a descendant.
      var currentRange = ChromeVoxState.instance.currentRange;
      if (currentRange && currentRange.start.node.root == node)
        return;

      // Discard focused root nodes without focused state set.
      if (!node.state.focused)
        return;

      // Try to find a focusable descendant.
      node = node.find({state: {focused: true}}) || node;
    }

    if (this.isEditable_(node))
      this.createTextEditHandlerIfNeeded_(evt.target);

    // Since we queue output mostly for live regions support and there isn't a
    // reliable way to know if this focus event resulted from a user's explicit
    // action, only flush when the focused node is not web content.
    if (node.root.role == RoleType.desktop)
      Output.flushNextSpeechUtterance();

    this.onEventDefault(
        /** @type {!AutomationEvent} */
        ({target: node, type: chrome.automation.EventType.focus}));
  },

  /**
   * Provides all feedback once a load complete event fires.
   * @param {Object} evt
   */
  onLoadComplete: function(evt) {
    ChromeVoxState.instance.refreshMode(evt.target.docUrl);

    // Don't process nodes inside of web content if ChromeVox Next is inactive.
    if (evt.target.root.role != RoleType.desktop &&
        ChromeVoxState.instance.mode === ChromeVoxMode.CLASSIC)
      return;

    // If initial focus was already placed on this page (e.g. if a user starts
    // tabbing before load complete), then don't move ChromeVox's position on
    // the page.
    if (ChromeVoxState.instance.currentRange &&
        ChromeVoxState.instance.currentRange.start.node.role !=
            RoleType.rootWebArea &&
        ChromeVoxState.instance.currentRange.start.node.root.docUrl ==
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
      ChromeVoxState.instance.setCurrentRange(cursors.Range.fromNode(node));

    if (ChromeVoxState.instance.currentRange)
      new Output().withSpeechAndBraille(
              ChromeVoxState.instance.currentRange, null, evt.type)
          .go();
  },

  /** @override */
  onTextChanged: function(evt) {
    if (this.isEditable_(evt.target))
      this.onEditableChanged_(evt);
  },

  /** @override */
  onTextSelectionChanged: function(evt) {
    if (this.isEditable_(evt.target))
      this.onEditableChanged_(evt);
  },

  /**
   * Provides all feedback once a change event in a text field fires.
   * @param {!AutomationEvent} evt
   * @private
   */
  onEditableChanged_: function(evt) {
    // Don't process nodes inside of web content if ChromeVox Next is inactive.
    if (evt.target.root.role != RoleType.desktop &&
        ChromeVoxState.instance.mode === ChromeVoxMode.CLASSIC)
      return;

    if (!evt.target.state.focused)
      return;

    if (!ChromeVoxState.instance.currentRange) {
      this.onEventDefault(evt);
      ChromeVoxState.instance.setCurrentRange(
          cursors.Range.fromNode(evt.target));
    }

    this.createTextEditHandlerIfNeeded_(evt.target);
    // TODO(plundblad): This can currently be null for contenteditables.
    // Clean up when it can't.
    if (this.textEditHandler_)
      this.textEditHandler_.onEvent(evt);
  },

  /**
   * Provides all feedback once a value changed event fires.
   * @param {!AutomationEvent} evt
   */
  onValueChanged: function(evt) {
    // Delegate to the edit text handler if this is an editable.
    if (this.isEditable_(evt.target)) {
      this.onEditableChanged_(evt);
      return;
    }

    // Don't process nodes inside of web content if ChromeVox Next is inactive.
    if (evt.target.root.role != RoleType.desktop &&
        ChromeVoxState.instance.mode === ChromeVoxMode.CLASSIC)
      return;

    var range = cursors.Range.fromNode(evt.target);
    new Output().withSpeechAndBraille(range, range, evt.type)
        .go();
  },

  /**
   * Handle updating the active indicator when the document scrolls.
   * @override
   */
  onScrollPositionChanged: function(evt) {
    var currentRange = ChromeVoxState.instance.currentRange;
    if (currentRange)
      new Output().withLocation(currentRange, null, evt.type).go();
  },

  /**
   * Create an editable text handler for the given node if needed.
   * @param {!AutomationNode} node
   */
  createTextEditHandlerIfNeeded_: function(node) {
    if (!this.textEditHandler_ ||
        this.textEditHandler_.node !== node) {
      this.textEditHandler_ = editing.TextEditHandler.createForNode(node);
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

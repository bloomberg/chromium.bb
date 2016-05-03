// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles automation from a desktop automation node.
 */

goog.provide('DesktopAutomationHandler');

goog.require('AutomationObjectConstructorInstaller');
goog.require('BaseAutomationHandler');
goog.require('ChromeVoxState');
goog.require('editing.TextEditHandler');

goog.scope(function() {
var AutomationEvent = chrome.automation.AutomationEvent;
var AutomationNode = chrome.automation.AutomationNode;
var Dir = constants.Dir;
var EventType = chrome.automation.EventType;
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

  /**
   * The last time we handled a value changed event.
   * @type {!Date}
   * @private
   */
  this.lastValueChanged_ = new Date(0);

  var e = EventType;
  this.addListener_(e.activedescendantchanged, this.onActiveDescendantChanged);
  this.addListener_(e.alert, this.onAlert);
  this.addListener_(e.ariaAttributeChanged, this.onEventIfInRange);
  this.addListener_(e.checkedStateChanged, this.onEventIfInRange);
  this.addListener_(e.focus, this.onFocus);
  this.addListener_(e.hover, this.onEventWithFlushedOutput);
  this.addListener_(e.loadComplete, this.onLoadComplete);
  this.addListener_(e.menuEnd, this.onMenuEnd);
  this.addListener_(e.menuListItemSelected, this.onEventDefault);
  this.addListener_(e.menuStart, this.onMenuStart);
  this.addListener_(e.scrollPositionChanged, this.onScrollPositionChanged);
  this.addListener_(e.selection, this.onEventWithFlushedOutput);
  this.addListener_(e.textChanged, this.onTextChanged);
  this.addListener_(e.textSelectionChanged, this.onTextSelectionChanged);
  this.addListener_(e.valueChanged, this.onValueChanged);

  AutomationObjectConstructorInstaller.init(node, function() {
    chrome.automation.getFocus((function(focus) {
      if (ChromeVoxState.instance.mode != ChromeVoxMode.FORCE_NEXT)
        return;

      if (focus) {
        this.onFocus(
            new chrome.automation.AutomationEvent(EventType.focus, focus));
      }
    }).bind(this));
  }.bind(this));
};

/**
 * Time to wait until processing more value changed events.
 * @const {number}
 */
DesktopAutomationHandler.VMIN_VALUE_CHANGE_DELAY_MS = 500;

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
    output.withRichSpeech(
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
   * @param {!AutomationEvent} evt
   */
  onEventIfInRange: function(evt) {
    // TODO(dtseng): Consider the end of the current range as well.
    if (AutomationUtil.isDescendantOf(
        global.backgroundObj.currentRange.start.node, evt.target) ||
            evt.target.state.focused)
      this.onEventDefault(evt);
  },

  /**
   * @param {!AutomationEvent} evt
   */
  onEventWithFlushedOutput: function(evt) {
    Output.flushNextSpeechUtterance();
    this.onEventDefault(evt);
  },

  /**
   * Makes an announcement without changing focus.
   * @param {!AutomationEvent} evt
   */
  onActiveDescendantChanged: function(evt) {
    if (!evt.target.activeDescendant)
      return;
    this.onEventDefault(new chrome.automation.AutomationEvent(
        EventType.focus, evt.target.activeDescendant));
  },

  /**
   * Makes an announcement without changing focus.
   * @param {!AutomationEvent} evt
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
   * @param {!AutomationEvent} evt
   */
  onFocus: function(evt) {
    // Invalidate any previous editable text handler state.
    this.textEditHandler_ = null;

    var node = evt.target;

    // Discard focus events on embeddedObject and client nodes.
    if (node.role == RoleType.embeddedObject || node.role == RoleType.client)
      return;

    this.createTextEditHandlerIfNeeded_(evt.target);

    // Since we queue output mostly for live regions support and there isn't a
    // reliable way to know if this focus event resulted from a user's explicit
    // action, only flush when the focused node is not web content.
    if (node.root.role == RoleType.desktop)
      Output.flushNextSpeechUtterance();

    this.onEventDefault(
        new chrome.automation.AutomationEvent(EventType.focus, node));
  },

  /**
   * Provides all feedback once a load complete event fires.
   * @param {!AutomationEvent} evt
   */
  onLoadComplete: function(evt) {
    ChromeVoxState.instance.refreshMode(evt.target.docUrl);

    // Don't process nodes inside of web content if ChromeVox Next is inactive.
    if (evt.target.root.role != RoleType.desktop &&
        ChromeVoxState.instance.mode === ChromeVoxMode.CLASSIC)
      return;

    chrome.automation.getFocus((function(focus) {
      if (!focus)
        return;

      // If initial focus was already placed on this page (e.g. if a user starts
      // tabbing before load complete), then don't move ChromeVox's position on
      // the page.
      if (ChromeVoxState.instance.currentRange &&
          ChromeVoxState.instance.currentRange.start.node.root == focus.root)
        return;

      ChromeVoxState.instance.setCurrentRange(cursors.Range.fromNode(focus));
      new Output().withRichSpeechAndBraille(
          ChromeVoxState.instance.currentRange, null, evt.type).go();
    }).bind(this));
  },


    /**
   * Provides all feedback once a text changed event fires.
   * @param {!AutomationEvent} evt
   */
  onTextChanged: function(evt) {
    if (evt.target.state.editable)
      this.onEditableChanged_(evt);
  },

  /**
   * Provides all feedback once a text selection changed event fires.
   * @param {!AutomationEvent} evt
   */
  onTextSelectionChanged: function(evt) {
    if (evt.target.state.editable)
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
    if (evt.target.state.editable) {
      this.onEditableChanged_(evt);
      return;
    }

    // Don't process nodes inside of web content if ChromeVox Next is inactive.
    if (evt.target.root.role != RoleType.desktop &&
        ChromeVoxState.instance.mode === ChromeVoxMode.CLASSIC)
      return;

    var t = evt.target;
    if (t.state.focused ||
        t.root.role == RoleType.desktop ||
        AutomationUtil.isDescendantOf(
            global.backgroundObj.currentRange.start.node, t)) {
      if (new Date() - this.lastValueChanged_ <=
          DesktopAutomationHandler.VMIN_VALUE_CHANGE_DELAY_MS)
        return;

      this.lastValueChanged_ = new Date();

      new Output().format('$value', evt.target)
          .go();
    }
  },

  /**
   * Handle updating the active indicator when the document scrolls.
   * @param {!AutomationEvent} evt
   */
  onScrollPositionChanged: function(evt) {
    if (ChromeVoxState.instance.mode === ChromeVoxMode.CLASSIC)
      return;

    var currentRange = ChromeVoxState.instance.currentRange;
    if (currentRange)
      new Output().withLocation(currentRange, null, evt.type).go();
  },

  /**
   * Provides all feedback once a menu start event fires.
   * @param {!AutomationEvent} evt
   */
  onMenuStart: function(evt) {
    global.backgroundObj.startExcursion();
    this.onEventDefault(evt);
  },

  /**
   * Provides all feedback once a menu end event fires.
   * @param {!AutomationEvent} evt
   */
  onMenuEnd: function(evt) {
    this.onEventDefault(evt);
    global.backgroundObj.endExcursion();
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

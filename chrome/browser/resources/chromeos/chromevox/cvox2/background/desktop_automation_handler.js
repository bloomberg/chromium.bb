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
goog.require('CustomAutomationEvent');
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
   * @private
   */
  this.textEditHandler_ = null;

  /**
   * The last time we handled a value changed event.
   * @type {!Date}
   * @private
   */
  this.lastValueChanged_ = new Date(0);

  this.addListener_(EventType.ACTIVEDESCENDANTCHANGED,
                    this.onActiveDescendantChanged);
  this.addListener_(EventType.ALERT,
                    this.onAlert);
  this.addListener_(EventType.ARIA_ATTRIBUTE_CHANGED,
                    this.onAriaAttributeChanged);
  this.addListener_(EventType.AUTOCORRECTION_OCCURED,
                    this.onEventIfInRange);
  this.addListener_(EventType.BLUR,
                    this.onBlur);
  this.addListener_(EventType.CHECKED_STATE_CHANGED,
                    this.onCheckedStateChanged);
  this.addListener_(EventType.CHILDREN_CHANGED,
                    this.onChildrenChanged);
  this.addListener_(EventType.EXPANDED_CHANGED,
                    this.onEventIfInRange);
  this.addListener_(EventType.FOCUS,
                    this.onFocus);
  this.addListener_(EventType.HOVER,
                    this.onHover);
  this.addListener_(EventType.INVALID_STATUS_CHANGED,
                    this.onEventIfInRange);
  this.addListener_(EventType.LOAD_COMPLETE,
                    this.onLoadComplete);
  this.addListener_(EventType.MENU_END,
                    this.onMenuEnd);
  this.addListener_(EventType.MENU_LIST_ITEM_SELECTED,
                    this.onEventIfSelected);
  this.addListener_(EventType.MENU_START,
                    this.onMenuStart);
  this.addListener_(EventType.ROW_COLLAPSED,
                    this.onEventIfInRange);
  this.addListener_(EventType.ROW_EXPANDED,
                    this.onEventIfInRange);
  this.addListener_(EventType.SCROLL_POSITION_CHANGED,
                    this.onScrollPositionChanged);
  this.addListener_(EventType.SELECTION,
                    this.onSelection);
  this.addListener_(EventType.TEXT_CHANGED,
                    this.onTextChanged);
  this.addListener_(EventType.TEXT_SELECTION_CHANGED,
                    this.onTextSelectionChanged);
  this.addListener_(EventType.VALUE_CHANGED,
                    this.onValueChanged);

  AutomationObjectConstructorInstaller.init(node, function() {
    chrome.automation.getFocus((function(focus) {
      if (ChromeVoxState.instance.mode != ChromeVoxMode.FORCE_NEXT)
        return;

      if (focus) {
        var event = new CustomAutomationEvent(EventType.FOCUS, focus, 'page');
        this.onFocus(event);
      }
    }).bind(this));
  }.bind(this));
};

/**
 * Time to wait until processing more value changed events.
 * @const {number}
 */
DesktopAutomationHandler.VMIN_VALUE_CHANGE_DELAY_MS = 500;

/**
 * Controls announcement of non-user-initiated events.
 * @type {boolean}
 */
DesktopAutomationHandler.announceActions = false;

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

    // Decide whether to announce and sync this event.
    if (!DesktopAutomationHandler.announceActions && evt.eventFrom == 'action')
      return;

    var prevRange = ChromeVoxState.instance.currentRange;

    ChromeVoxState.instance.setCurrentRange(cursors.Range.fromNode(node));

    if (!this.shouldOutput_(evt))
      return;

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
    if (!this.shouldOutput_(evt))
      return;

    var prev = ChromeVoxState.instance.currentRange;
    if (prev.contentEquals(cursors.Range.fromNode(evt.target)) ||
        evt.target.state.focused) {
      // Category flush here since previous focus events via navigation can
      // cause double speak.
      Output.forceModeForNextSpeechUtterance(cvox.QueueMode.CATEGORY_FLUSH);

      // Intentionally skip setting range.
      new Output()
          .withRichSpeechAndBraille(cursors.Range.fromNode(evt.target),
                                    prev,
                                    Output.EventType.NAVIGATE)
          .go();
    }
  },

  /**
   * @param {!AutomationEvent} evt
   */
  onEventIfSelected: function(evt) {
    if (evt.target.state.selected)
      this.onEventDefault(evt);
  },

  /**
   * @param {!AutomationEvent} evt
   */
  onEventWithFlushedOutput: function(evt) {
    Output.forceModeForNextSpeechUtterance(cvox.QueueMode.FLUSH);
    this.onEventDefault(evt);
  },

  /**
   * @param {!AutomationEvent} evt
   */
  onAriaAttributeChanged: function(evt) {
    if (evt.target.state.editable)
      return;
    this.onEventIfInRange(evt);
  },

  /**
   * @param {!AutomationEvent} evt
   */
  onHover: function(evt) {
    if (ChromeVoxState.instance.currentRange &&
        evt.target == ChromeVoxState.instance.currentRange.start.node)
      return;
    Output.forceModeForNextSpeechUtterance(cvox.QueueMode.FLUSH);
    this.onEventDefault(evt);
  },

  /**
   * Makes an announcement without changing focus.
   * @param {!AutomationEvent} evt
   */
  onActiveDescendantChanged: function(evt) {
    if (!evt.target.activeDescendant || !evt.target.state.focused)
      return;
    var event = new CustomAutomationEvent(
        EventType.FOCUS, evt.target.activeDescendant, evt.eventFrom);
    this.onEventDefault(event);
  },

  /**
   * Makes an announcement without changing focus.
   * @param {!AutomationEvent} evt
   */
  onAlert: function(evt) {
    var node = evt.target;
    if (!node || !this.shouldOutput_(evt))
      return;

    var range = cursors.Range.fromNode(node);

    new Output().withSpeechAndBraille(range, null, evt.type).go();
  },

  onBlur: function(evt) {
    // Nullify focus if it no longer exists.
    chrome.automation.getFocus(function(focus) {
      if (!focus)
        ChromeVoxState.instance.setCurrentRange(null);
    });
  },

  /**
   * Provides all feedback once a checked state changed event fires.
   * @param {!AutomationEvent} evt
   */
  onCheckedStateChanged: function(evt) {
    if (!AutomationPredicate.checkable(evt.target))
      return;

    Output.forceModeForNextSpeechUtterance(cvox.QueueMode.CATEGORY_FLUSH);
    var event = new CustomAutomationEvent(
        EventType.CHECKED_STATE_CHANGED, evt.target, evt.eventFrom);
    this.onEventIfInRange(event);
  },

  /**
   * @param {!AutomationEvent} evt
   */
  onChildrenChanged: function(evt) {
    if (evt.target.state.richlyEditable) {
      // Generic tree changes within richly editable text e.g. inline text box
      // data might require editable text updates. Further note that children
      // change events can and do come after text/text selection changes.
      var rootEditable = evt.target;
      while (rootEditable.parent && rootEditable.parent.state.richlyEditable)
        rootEditable = rootEditable.parent;
      this.onEditableChanged_(new CustomAutomationEvent(
          EventType.TEXT_CHANGED, rootEditable, evt.eventFrom));
      return;
    }

    if (!this.shouldOutput_(evt))
      return;

    var curRange = ChromeVoxState.instance.currentRange;

    // Always refresh the braille contents.
    if (curRange && curRange.equals(cursors.Range.fromNode(evt.target))) {
      new Output().withBraille(curRange, curRange, Output.EventType.NAVIGATE)
          .go();
    }

    this.onActiveDescendantChanged(evt);
  },

  /**
   * Provides all feedback once a focus event fires.
   * @param {!AutomationEvent} evt
   */
  onFocus: function(evt) {
    // Invalidate any previous editable text handler state.
    this.textEditHandler_ = null;

    var node = evt.target;

    // Discard focus events on embeddedObject.
    if (node.role == RoleType.EMBEDDED_OBJECT)
      return;

    this.createTextEditHandlerIfNeeded_(evt.target);

    // Category flush speech triggered by events with no source. This includes
    // views.
    if (evt.eventFrom == '')
      Output.forceModeForNextSpeechUtterance(cvox.QueueMode.CATEGORY_FLUSH);
    if (!node.root)
      return;

    var root = AutomationUtil.getTopLevelRoot(node.root);
    // If we're crossing out of a root, save it in case it needs recovering.
    var prevRange = ChromeVoxState.instance.currentRange;
    var prevNode = prevRange ? prevRange.start.node : null;
    if (prevNode) {
    var prevRoot = AutomationUtil.getTopLevelRoot(prevNode);
      if (prevRoot && prevRoot !== root)
      ChromeVoxState.instance.focusRecoveryMap.set(prevRoot, prevRange);
    }
    // If a previous node was saved for this focus, restore it.
    var savedRange = ChromeVoxState.instance.focusRecoveryMap.get(root);
    ChromeVoxState.instance.focusRecoveryMap.delete(root);
    if (savedRange) {
      ChromeVoxState.instance.navigateToRange(savedRange, false);
      return;
    }
    var event = new CustomAutomationEvent(EventType.FOCUS, node, evt.eventFrom);
    this.onEventDefault(event);
  },

  /**
   * Provides all feedback once a load complete event fires.
   * @param {!AutomationEvent} evt
   */
  onLoadComplete: function(evt) {
    chrome.automation.getFocus(function(focus) {
      if (!focus || !AutomationUtil.isDescendantOf(focus, evt.target))
        return;

      // Create text edit handler, if needed, now in order not to miss initial
      // value change if text field has already been focused when initializing
      // ChromeVox.
      this.createTextEditHandlerIfNeeded_(focus);

      // If auto read is set, skip focus recovery and start reading from the top.
      if (localStorage['autoRead'] == 'true' &&
          AutomationUtil.getTopLevelRoot(evt.target) == evt.target) {
        ChromeVoxState.instance.setCurrentRange(cursors.Range.fromNode(evt.target));
        cvox.ChromeVox.tts.stop();
        CommandHandler.onCommand('readFromHere');
        return;
      }

      // If initial focus was already placed on this page (e.g. if a user starts
      // tabbing before load complete), then don't move ChromeVox's position on
      // the page.
      if (ChromeVoxState.instance.currentRange &&
          ChromeVoxState.instance.currentRange.start.node.root == focus.root)
        return;

      var o = new Output();
      if (focus.role == RoleType.ROOT_WEB_AREA) {
        // Restore to previous position.
        var url = focus.docUrl;
        url = url.substring(0, url.indexOf('#')) || url;
        var pos = cvox.ChromeVox.position[url];
        if (pos) {
          focus = AutomationUtil.hitTest(focus.root, pos) || focus;
          if (focus != focus.root)
            o.format('$name', focus.root);
        }
      }
      ChromeVoxState.instance.setCurrentRange(cursors.Range.fromNode(focus));
      if (!this.shouldOutput_(evt))
        return;

      Output.forceModeForNextSpeechUtterance(cvox.QueueMode.FLUSH);
      o.withRichSpeechAndBraille(
          ChromeVoxState.instance.currentRange, null, evt.type).go();
    }.bind(this));
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
    var topRoot = AutomationUtil.getTopLevelRoot(evt.target);
    if (!evt.target.state.focused ||
        (topRoot &&
            topRoot.parent &&
            !topRoot.parent.state.focused))
      return;

    if (!ChromeVoxState.instance.currentRange) {
      this.onEventDefault(evt);
      ChromeVoxState.instance.setCurrentRange(
          cursors.Range.fromNode(evt.target));
    }

    this.createTextEditHandlerIfNeeded_(evt.target);

    // Sync the ChromeVox range to the editable, if a selection exists.
    var anchorObject = evt.target.root.anchorObject;
    var anchorOffset = evt.target.root.anchorOffset || 0;
    var focusObject = evt.target.root.focusObject;
    var focusOffset = evt.target.root.focusOffset || 0;
    if (anchorObject && focusObject) {
      var selectedRange = new cursors.Range(
          new cursors.WrappingCursor(anchorObject, anchorOffset),
          new cursors.WrappingCursor(focusObject, focusOffset));

      // Sync ChromeVox range with selection.
      ChromeVoxState.instance.setCurrentRange(selectedRange);
    }

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

    if (!this.shouldOutput_(evt))
      return;

    var t = evt.target;
    if (t.state.focused ||
        t.root.role == RoleType.DESKTOP ||
        AutomationUtil.isDescendantOf(
            ChromeVoxState.instance.currentRange.start.node, t)) {
      if (new Date() - this.lastValueChanged_ <=
          DesktopAutomationHandler.VMIN_VALUE_CHANGE_DELAY_MS)
        return;

      this.lastValueChanged_ = new Date();

      var output = new Output();

      if (t.root.role == RoleType.DESKTOP)
        output.withQueueMode(cvox.QueueMode.FLUSH);

      output.format('$value', evt.target).go();
    }
  },

  /**
   * Handle updating the active indicator when the document scrolls.
   * @param {!AutomationEvent} evt
   */
  onScrollPositionChanged: function(evt) {
    var currentRange = ChromeVoxState.instance.currentRange;
    if (currentRange && currentRange.isValid() && this.shouldOutput_(evt))
      new Output().withLocation(currentRange, null, evt.type).go();
  },

  /**
   * @param {!AutomationEvent} evt
   */
  onSelection: function(evt) {
    // Invalidate any previous editable text handler state since some nodes,
    // like menuitems, can receive selection while focus remains on an editable
    // leading to braille output routing to the editable.
    this.textEditHandler_ = null;

    chrome.automation.getFocus(function(focus) {
      // Desktop tabs get "selection" when there's a focused webview during tab
      // switching.
      if (focus.role == RoleType.WEB_VIEW && evt.target.role == RoleType.TAB) {
        ChromeVoxState.instance.setCurrentRange(
            cursors.Range.fromNode(focus.firstChild));
        return;
      }

      // Some cases (e.g. in overview mode), require overriding the assumption
      // that focus is an ancestor of a selection target.
      var override = evt.target.role == RoleType.MENU_ITEM ||
          (evt.target.root == focus.root &&
              focus.root.role == RoleType.DESKTOP);
      Output.forceModeForNextSpeechUtterance(cvox.QueueMode.FLUSH);
      if (override || AutomationUtil.isDescendantOf(evt.target, focus))
        this.onEventDefault(evt);
    }.bind(this));
  },

  /**
   * Provides all feedback once a menu start event fires.
   * @param {!AutomationEvent} evt
   */
  onMenuStart: function(evt) {
    ChromeVoxState.instance.markCurrentRange();
    this.onEventDefault(evt);
  },

  /**
   * Provides all feedback once a menu end event fires.
   * @param {!AutomationEvent} evt
   */
  onMenuEnd: function(evt) {
    this.onEventDefault(evt);

    // This is a work around for Chrome context menus not firing a focus event
    // after you close them.
    chrome.automation.getFocus(function(focus) {
      if (focus) {
        var event = new CustomAutomationEvent(EventType.FOCUS, focus, 'page');
        this.onFocus(event);
      }
    }.bind(this));
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
   * Once an event handler updates ChromeVox's range based on |evt|
   * which updates mode, returns whether |evt| should be outputted.
   * @return {boolean}
   */
  shouldOutput_: function(evt) {
    var mode = ChromeVoxState.instance.mode;
    // Only output desktop rooted nodes or web nodes for next engine modes.
    return evt.target.root.role == RoleType.DESKTOP ||
        (mode == ChromeVoxMode.NEXT ||
         mode == ChromeVoxMode.FORCE_NEXT ||
         mode == ChromeVoxMode.CLASSIC_COMPAT);
  }
};

/**
 * Initializes global state for DesktopAutomationHandler.
 * @private
 */
DesktopAutomationHandler.init_ = function() {
  chrome.automation.getDesktop(function(desktop) {
    ChromeVoxState.desktopAutomationHandler =
        new DesktopAutomationHandler(desktop);
  });
};

DesktopAutomationHandler.init_();

});  // goog.scope

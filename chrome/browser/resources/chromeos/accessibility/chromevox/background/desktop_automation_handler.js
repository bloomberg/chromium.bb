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
goog.require('CommandHandler');
goog.require('CustomAutomationEvent');
goog.require('editing.TextEditHandler');

goog.scope(function() {
const AutomationNode = chrome.automation.AutomationNode;
const EventType = chrome.automation.EventType;
const RoleType = chrome.automation.RoleType;
const StateType = chrome.automation.StateType;

DesktopAutomationHandler = class extends BaseAutomationHandler {
  /**
   * @param {!AutomationNode} node
   */
  constructor(node) {
    super(node);

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

    /** @private {AutomationNode} */
    this.lastValueTarget_ = null;

    /** @private {string} */
    this.lastRootUrl_ = '';

    /** @private {boolean} */
    this.shouldIgnoreDocumentSelectionFromAction_ = false;

    /** @private {number?} */
    this.delayedAttributeOutputId_;

    /** @private {!Date} */
    this.lastHoverExit_ = new Date();

    /** @private {!AutomationNode|undefined} */
    this.lastHoverTarget_;

    this.addListener_(EventType.ALERT, this.onAlert);
    this.addListener_(EventType.BLUR, this.onBlur);
    this.addListener_(
        EventType.DOCUMENT_SELECTION_CHANGED, this.onDocumentSelectionChanged);
    this.addListener_(EventType.FOCUS, this.onFocus);
    this.addListener_(EventType.HOVER, this.onHover);

    // Note that live region changes from views are really announcement
    // events. Their target nodes contain no live region semantics and have no
    // relation to live regions which are supported in |LiveRegions|.
    this.addListener_(EventType.LIVE_REGION_CHANGED, this.onLiveRegionChanged);

    this.addListener_(EventType.LOAD_COMPLETE, this.onLoadComplete);
    this.addListener_(EventType.MENU_END, this.onMenuEnd);
    this.addListener_(EventType.MENU_START, this.onMenuStart);
    this.addListener_(
        EventType.SCROLL_POSITION_CHANGED, this.onScrollPositionChanged);
    this.addListener_(EventType.SELECTION, this.onSelection);
    this.addListener_(EventType.TEXT_CHANGED, this.onEditableChanged_);
    this.addListener_(
        EventType.TEXT_SELECTION_CHANGED, this.onEditableChanged_);
    this.addListener_(EventType.VALUE_CHANGED, this.onValueChanged);

    AutomationObjectConstructorInstaller.init(node, function() {
      chrome.automation.getFocus((function(focus) {
                                   if (focus) {
                                     const event = new CustomAutomationEvent(
                                         EventType.FOCUS, focus, 'page');
                                     this.onFocus(event);
                                   }
                                 }).bind(this));
    }.bind(this));
  }

  /** @type {editing.TextEditHandler} */
  get textEditHandler() {
    return this.textEditHandler_;
  }

  /**
   * @return {!AutomationNode|undefined} The target of the last observed hover
   *     event.
   */
  get lastHoverTarget() {
    return this.lastHoverTarget_;
  }

  /** @override */
  willHandleEvent_(evt) {
    return false;
  }

  /**
   * Handles the result of a hit test.
   * @param {!AutomationNode} node The hit result.
   */
  onHitTestResult(node) {
    // It is possible that the user moved since we requested a hit test.  Bail
    // if the current range is valid and on the same page as the hit result
    // (but not the root).
    if (ChromeVoxState.instance.currentRange &&
        ChromeVoxState.instance.currentRange.start &&
        ChromeVoxState.instance.currentRange.start.node &&
        ChromeVoxState.instance.currentRange.start.node.root) {
      const cur = ChromeVoxState.instance.currentRange.start.node;
      if (cur.role != RoleType.ROOT_WEB_AREA &&
          AutomationUtil.getTopLevelRoot(node) ==
              AutomationUtil.getTopLevelRoot(cur)) {
        return;
      }
    }

    chrome.automation.getFocus(function(focus) {
      if (!focus && !node) {
        return;
      }

      focus = node || focus;
      const focusedRoot = AutomationUtil.getTopLevelRoot(focus);
      const output = new Output();
      if (focus != focusedRoot && focusedRoot) {
        output.format('$name', focusedRoot);
      }

      // Even though we usually don't output events from actions, hit test
      // results should generate output.
      const range = cursors.Range.fromNode(focus);
      ChromeVoxState.instance.setCurrentRange(range);
      output.withRichSpeechAndBraille(range, null, Output.EventType.NAVIGATE)
          .go();
    });
  }

  /**
   * @param {!ChromeVoxEvent} evt
   */
  onHover(evt) {
    if (!GestureCommandHandler.getEnabled()) {
      return;
    }

    EventSourceState.set(EventSourceType.TOUCH_GESTURE);

    // Save the last hover target for use by the gesture handler.
    this.lastHoverTarget_ = evt.target;

    let target = evt.target;
    let targetLeaf = null;
    let targetObject = null;
    while (target && target != target.root) {
      if (!targetObject && AutomationPredicate.touchObject(target)) {
        targetObject = target;
      }
      if (AutomationPredicate.touchLeaf(target)) {
        targetLeaf = target;
      }
      target = target.parent;
    }

    target = targetLeaf || targetObject;
    if (!target) {
      // This clears the anchor point in the TouchExplorationController (so
      // things like double tap won't be directed to the previous target). It
      // also ensures if a user touch explores back to the previous range, it
      // will be announced again.
      ChromeVoxState.instance.setCurrentRange(null);

      // Play a earcon to let the user know they're in the middle of nowhere.
      if ((new Date() - this.lastHoverExit_) >
          DesktopAutomationHandler.MIN_HOVER_EXIT_SOUND_DELAY_MS) {
        ChromeVoxState.instance.nextEarcons_.engine_.onTouchExitAnchor();
        this.lastHoverExit_ = new Date();
      }
      chrome.tts.stop();
      return;
    }

    if (ChromeVoxState.instance.currentRange &&
        target == ChromeVoxState.instance.currentRange.start.node) {
      return;
    }

    if (!this.createTextEditHandlerIfNeeded_(target)) {
      this.textEditHandler_ = null;
    }

    ChromeVoxState.instance.nextEarcons_.engine_.onTouchEnterAnchor();
    Output.forceModeForNextSpeechUtterance(QueueMode.FLUSH);
    this.onEventDefault(
        new CustomAutomationEvent(evt.type, target, evt.eventFrom));
  }

  /**
   * Makes an announcement without changing focus.
   * @param {!ChromeVoxEvent} evt
   */
  onAlert(evt) {
    const node = evt.target;
    const range = cursors.Range.fromNode(node);

    new Output()
        .withSpeechCategory(TtsCategory.LIVE)
        .withSpeechAndBraille(range, null, evt.type)
        .go();
  }

  onBlur(evt) {
    // Nullify focus if it no longer exists.
    chrome.automation.getFocus(function(focus) {
      if (!focus) {
        ChromeVoxState.instance.setCurrentRange(null);
      }
    });
  }

  /**
   * @param {!ChromeVoxEvent} evt
   */
  onDocumentSelectionChanged(evt) {
    let selectionStart = evt.target.selectionStartObject;

    // No selection.
    if (!selectionStart) {
      return;
    }

    // A caller requested this event be ignored.
    if (this.shouldIgnoreDocumentSelectionFromAction_ &&
        evt.eventFrom == 'action') {
      return;
    }

    // Editable selection.
    if (selectionStart.state[StateType.EDITABLE]) {
      selectionStart =
          AutomationUtil.getEditableRoot(selectionStart) || selectionStart;
      this.onEditableChanged_(
          new CustomAutomationEvent(evt.type, selectionStart, evt.eventFrom));
    }

    // Non-editable selections are handled in |Background|.
  }

  /**
   * Provides all feedback once a focus event fires.
   * @param {!ChromeVoxEvent} evt
   */
  onFocus(evt) {
    if (evt.target.role == RoleType.ROOT_WEB_AREA &&
        evt.eventFrom != 'action') {
      chrome.automation.getFocus(
          this.maybeRecoverFocusAndOutput_.bind(this, evt));
      return;
    }

    // Invalidate any previous editable text handler state.
    if (!this.createTextEditHandlerIfNeeded_(evt.target, true)) {
      this.textEditHandler_ = null;
    }

    const node = evt.target;

    // Discard focus events on embeddedObject and webView.
    if (node.role == RoleType.EMBEDDED_OBJECT ||
        node.role == RoleType.PLUGIN_OBJECT || node.role == RoleType.WEB_VIEW) {
      return;
    }

    if (node.role == RoleType.UNKNOWN) {
      return;
    }

    if (!node.root) {
      return;
    }

    // Update the focused root url, which gets used as part of focus recovery.
    this.lastRootUrl_ = node.root.docUrl || '';

    // Consider the case when a user presses tab rapidly. The key events may
    // come in far before the accessibility focus events. We therefore must
    // category flush here or the focus events will all queue up.
    Output.forceModeForNextSpeechUtterance(QueueMode.CATEGORY_FLUSH);

    const event =
        new CustomAutomationEvent(EventType.FOCUS, node, evt.eventFrom);
    this.onEventDefault(event);

    // Refresh the handler, if needed, now that ChromeVox focus is up to date.
    this.createTextEditHandlerIfNeeded_(node);
  }

  /**
   * @param {!ChromeVoxEvent} evt
   */
  onLiveRegionChanged(evt) {
    if (evt.target.root.role == RoleType.DESKTOP ||
        evt.target.root.role == RoleType.APPLICATION) {
      if (evt.target.containerLiveStatus != 'assertive' &&
          evt.target.containerLiveStatus != 'polite') {
        return;
      }

      const output = new Output();
      if (evt.target.containerLiveStatus == 'assertive') {
        output.withQueueMode(QueueMode.CATEGORY_FLUSH);
      } else {
        output.withQueueMode(QueueMode.QUEUE);
      }

      output
          .withRichSpeechAndBraille(
              cursors.Range.fromNode(evt.target), null, evt.type)
          .withSpeechCategory(TtsCategory.LIVE)
          .go();
    }
  }

  /**
   * Provides all feedback once a load complete event fires.
   * @param {!ChromeVoxEvent} evt
   */
  onLoadComplete(evt) {
    // A load complete gets fired on the desktop node when display metrics
    // change.
    if (evt.target.role == RoleType.DESKTOP) {
      const msg = evt.target.state[StateType.HORIZONTAL] ? 'device_landscape' :
                                                           'device_portrait';
      new Output().format('@' + msg).go();
      return;
    }

    // We are only interested in load completes on valid top level roots.
    const top = AutomationUtil.getTopLevelRoot(evt.target);
    if (!top || top != evt.target.root || !top.docUrl) {
      return;
    }

    this.lastRootUrl_ = '';
    chrome.automation.getFocus(function(focus) {
      // In some situations, ancestor windows get focused before a descendant
      // webView/rootWebArea. In particular, a window that gets opened but no
      // inner focus gets set. We catch this generically by re-targetting focus
      // if focus is the ancestor of the load complete target (below).
      const focusIsAncestor = AutomationUtil.isDescendantOf(evt.target, focus);
      const focusIsDescendant =
          AutomationUtil.isDescendantOf(focus, evt.target);
      if (!focus || (!focusIsAncestor && !focusIsDescendant)) {
        return;
      }

      if (focusIsAncestor) {
        focus = evt.target;
      }

      // Create text edit handler, if needed, now in order not to miss initial
      // value change if text field has already been focused when initializing
      // ChromeVox.
      this.createTextEditHandlerIfNeeded_(focus);

      // If auto read is set, skip focus recovery and start reading from the
      // top.
      if (localStorage['autoRead'] == 'true' &&
          AutomationUtil.getTopLevelRoot(evt.target) == evt.target) {
        ChromeVoxState.instance.setCurrentRange(
            cursors.Range.fromNode(evt.target));
        ChromeVox.tts.stop();
        CommandHandler.onCommand('readFromHere');
        return;
      }

      this.maybeRecoverFocusAndOutput_(evt, focus);
    }.bind(this));
  }

  /**
   * Sets whether document selections from actions should be ignored.
   * @param {boolean} val
   */
  ignoreDocumentSelectionFromAction(val) {
    this.shouldIgnoreDocumentSelectionFromAction_ = val;
  }

  /**
   * Provides all feedback once a change event in a text field fires.
   * @param {!ChromeVoxEvent} evt
   * @private
   */
  onEditableChanged_(evt) {
    // Document selections only apply to rich editables, text selections to
    // non-rich editables.
    if (evt.type != EventType.DOCUMENT_SELECTION_CHANGED &&
        evt.target.state[StateType.RICHLY_EDITABLE]) {
      return;
    }

    if (!this.createTextEditHandlerIfNeeded_(evt.target)) {
      return;
    }

    if (!ChromeVoxState.instance.currentRange) {
      this.onEventDefault(evt);
      ChromeVoxState.instance.setCurrentRange(
          cursors.Range.fromNode(evt.target));
    }

    // Sync the ChromeVox range to the editable, if a selection exists.
    const selectionStartObject = evt.target.root.selectionStartObject;
    const selectionStartOffset = evt.target.root.selectionStartOffset || 0;
    const selectionEndObject = evt.target.root.selectionEndObject;
    const selectionEndOffset = evt.target.root.selectionEndOffset || 0;
    if (selectionStartObject && selectionEndObject) {
      // Sync to the selection's deep equivalent especially in editables, where
      // selection is often on the root text field with a child offset.
      const selectedRange = new cursors.Range(
          new cursors.WrappingCursor(selectionStartObject, selectionStartOffset)
              .deepEquivalent,
          new cursors.WrappingCursor(selectionEndObject, selectionEndOffset)
              .deepEquivalent);

      // Sync ChromeVox range with selection.
      ChromeVoxState.instance.setCurrentRange(selectedRange);
    }
    this.textEditHandler_.onEvent(evt);
  }

  /**
   * Provides all feedback once a value changed event fires.
   * @param {!ChromeVoxEvent} evt
   */
  onValueChanged(evt) {
    // Skip root web areas.
    if (evt.target.role == RoleType.ROOT_WEB_AREA) {
      return;
    }

    // Skip all unfocused text fields.
    if (!evt.target.state[StateType.FOCUSED] &&
        evt.target.state[StateType.EDITABLE]) {
      return;
    }

    // Delegate to the edit text handler if this is an editable, with the
    // exception of spin buttons.
    if (evt.target.state[StateType.EDITABLE] &&
        evt.target.role != RoleType.SPIN_BUTTON) {
      this.onEditableChanged_(evt);
      return;
    }

    const t = evt.target;
    const fromDesktop = t.root.role == RoleType.DESKTOP;
    if (t.state.focused || fromDesktop ||
        AutomationUtil.isDescendantOf(
            ChromeVoxState.instance.currentRange.start.node, t)) {
      if (new Date() - this.lastValueChanged_ <=
          DesktopAutomationHandler.VMIN_VALUE_CHANGE_DELAY_MS) {
        return;
      }

      this.lastValueChanged_ = new Date();

      const output = new Output();
      output.withQueueMode(QueueMode.CATEGORY_FLUSH);

      if (fromDesktop &&
          (!this.lastValueTarget_ || this.lastValueTarget_ !== t)) {
        const range = cursors.Range.fromNode(t);
        output.withRichSpeechAndBraille(
            range, range, Output.EventType.NAVIGATE);
        this.lastValueTarget_ = t;
      } else {
        output.format(
            '$if($value, $value, $if($valueForRange, $valueForRange))', t);
      }
      output.go();
    }
  }

  /**
   * Handle updating the active indicator when the document scrolls.
   * @param {!ChromeVoxEvent} evt
   */
  onScrollPositionChanged(evt) {
    const currentRange = ChromeVoxState.instance.currentRange;
    if (currentRange && currentRange.isValid()) {
      new Output().withLocation(currentRange, null, evt.type).go();
    }
  }

  /**
   * @param {!ChromeVoxEvent} evt
   */
  onSelection(evt) {
    // Invalidate any previous editable text handler state since some nodes,
    // like menuitems, can receive selection while focus remains on an
    // editable leading to braille output routing to the editable.
    this.textEditHandler_ = null;

    chrome.automation.getFocus((focus) => {
      // Desktop tabs get "selection" when there's a focused webview during
      // tab switching. Ignore it.
      if (evt.target.role == RoleType.TAB &&
          evt.target.root.role == RoleType.DESKTOP) {
        return;
      }

      // Some cases (e.g. in overview mode), require overriding the assumption
      // that focus is an ancestor of a selection target.
      const override = AutomationPredicate.menuItem(evt.target) ||
          (evt.target.root == focus.root &&
           focus.root.role == RoleType.DESKTOP) ||
          evt.target.role === RoleType.IME_CANDIDATE;
      if (override || AutomationUtil.isDescendantOf(evt.target, focus)) {
        this.onEventDefault(evt);
      }
    });
  }

  /**
   * Provides all feedback once a menu start event fires.
   * @param {!ChromeVoxEvent} evt
   */
  onMenuStart(evt) {
    ChromeVoxState.instance.markCurrentRange();
    this.onEventDefault(evt);
  }

  /**
   * Provides all feedback once a menu end event fires.
   * @param {!ChromeVoxEvent} evt
   */
  onMenuEnd(evt) {
    this.onEventDefault(evt);

    // This is a work around for Chrome context menus not firing a focus event
    // after you close them.
    chrome.automation.getFocus(function(focus) {
      if (focus) {
        const event = new CustomAutomationEvent(EventType.FOCUS, focus, 'page');
        this.onFocus(event);
      }
    }.bind(this));
  }

  /**
   * Create an editable text handler for the given node if needed.
   * @param {!AutomationNode} node
   * @param {boolean=} opt_onFocus True if called within a focus event
   *     handler. False by default.
   * @return {boolean} True if the handler exists (created/already present).
   */
  createTextEditHandlerIfNeeded_(node, opt_onFocus) {
    if (!node.state.editable) {
      return false;
    }

    if (!ChromeVoxState.instance.currentRange ||
        !ChromeVoxState.instance.currentRange.start ||
        !ChromeVoxState.instance.currentRange.start.node) {
      return false;
    }

    const topRoot = AutomationUtil.getTopLevelRoot(node);
    if (!node.state.focused ||
        (topRoot && topRoot.parent && !topRoot.parent.state.focused)) {
      return false;
    }

    // Re-target the node to the root of the editable.
    let target = node;
    target = AutomationUtil.getEditableRoot(target);
    let voxTarget = ChromeVoxState.instance.currentRange.start.node;
    voxTarget = AutomationUtil.getEditableRoot(voxTarget) || voxTarget;

    // It is possible that ChromeVox has range over some other node when a
    // text field is focused. Only allow this when focus is on a desktop node,
    // ChromeVox is over the keyboard, or during focus events.
    if (!target || !voxTarget ||
        (!opt_onFocus && target != voxTarget &&
         target.root.role != RoleType.DESKTOP &&
         voxTarget.root.role != RoleType.DESKTOP &&
         !AutomationUtil.isDescendantOf(target, voxTarget) &&
         !AutomationUtil.getAncestors(voxTarget.root)
              .find((n) => n.role == RoleType.KEYBOARD))) {
      return false;
    }

    if (!this.textEditHandler_ || this.textEditHandler_.node !== target) {
      this.textEditHandler_ = editing.TextEditHandler.createForNode(target);
    }

    return !!this.textEditHandler_;
  }

  /**
   * @param {ChromeVoxEvent} evt
   * @private
   */
  maybeRecoverFocusAndOutput_(evt, focus) {
    const focusedRoot = AutomationUtil.getTopLevelRoot(focus);
    if (!focusedRoot) {
      return;
    }

    let curRoot;
    if (ChromeVoxState.instance.currentRange) {
      curRoot = AutomationUtil.getTopLevelRoot(
          ChromeVoxState.instance.currentRange.start.node);
    }

    // If initial focus was already placed inside this page (e.g. if a user
    // starts tabbing before load complete), then don't move ChromeVox's
    // position on the page.
    if (curRoot && focusedRoot == curRoot &&
        this.lastRootUrl_ == focusedRoot.docUrl) {
      return;
    }

    this.lastRootUrl_ = focusedRoot.docUrl || '';
    const o = new Output();
    // Restore to previous position.
    let url = focusedRoot.docUrl;
    url = url.substring(0, url.indexOf('#')) || url;
    const pos = ChromeVox.position[url];

    // Disallow recovery for chrome urls.
    if (pos && url.indexOf('chrome://') != 0) {
      focusedRoot.hitTestWithReply(
          pos.x, pos.y, this.onHitTestResult.bind(this));
      return;
    }

    // This catches initial focus (i.e. on startup).
    if (!curRoot && focus != focusedRoot) {
      o.format('$name', focusedRoot);
    }

    ChromeVoxState.instance.setCurrentRange(cursors.Range.fromNode(focus));

    o.withRichSpeechAndBraille(
         ChromeVoxState.instance.currentRange, null, evt.type)
        .go();
  }

  /**
   * Initializes global state for DesktopAutomationHandler.
   */
  static init() {
    if (DesktopAutomationHandler.instance) {
      throw new Error('DesktopAutomationHandler.instance already exists.');
    }

    chrome.automation.getDesktop(function(desktop) {
      DesktopAutomationHandler.instance = new DesktopAutomationHandler(desktop);
    });
  }
};

/**
 * Time to wait until processing more value changed events.
 * @const {number}
 */
DesktopAutomationHandler.VMIN_VALUE_CHANGE_DELAY_MS = 50;

/**
 * Time to wait before announcing attribute changes that are otherwise too
 * disruptive.
 * @const {number}
 */
DesktopAutomationHandler.ATTRIBUTE_DELAY_MS = 1500;

/**
 * Controls announcement of non-user-initiated events.
 * @type {boolean}
 */
DesktopAutomationHandler.announceActions = false;

/** @const {number} */
DesktopAutomationHandler.MIN_HOVER_EXIT_SOUND_DELAY_MS = 500;

/**
 * Global instance.
 * @type {DesktopAutomationHandler}
 */
DesktopAutomationHandler.instance;

});  // goog.scope

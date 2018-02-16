// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var AutomationEvent = chrome.automation.AutomationEvent;
var EventType = chrome.automation.EventType;
var RoleType = chrome.automation.RoleType;

// CrosSelectToSpeakStartSpeechMethod enums.
// These values are persited to logs and should not be renumbered or re-used.
// See tools/metrics/histograms/enums.xml.
const START_SPEECH_METHOD_MOUSE = 0;
const START_SPEECH_METHOD_KEYSTROKE = 1;
// The number of enum values in CrosSelectToSpeapStartSpeechMethod. This should
// be kept in sync with the enum count in tools/metrics/histograms/enums.xml.
const START_SPEECH_METHOD_COUNT = 2;

/**
 * Node state. Nodes can be on-screen like normal, or they may
 * be invisible if they are in a tab that is not in the foreground
 * or similar, or they may be invalid if they were removed from their
 * root, i.e. if they were in a window that was closed.
 * @enum {number}
 */
const NodeState = {
  NODE_STATE_INVALID: 0,
  NODE_STATE_INVISIBLE: 1,
  NODE_STATE_NORMAL: 2,
};

/**
 * Gets the first window containing this node.
 */
function getNearestContainingWindow(node) {
  // Go upwards to root nodes' parents until we find the first window.
  if (node.root.role == RoleType.ROOT_WEB_AREA) {
    var nextRootParent = node;
    while (nextRootParent != null && nextRootParent.role != RoleType.WINDOW &&
           nextRootParent.root != null &&
           nextRootParent.root.role == RoleType.ROOT_WEB_AREA) {
      nextRootParent = nextRootParent.root.parent;
    }
    return nextRootParent;
  }
  // If the parent isn't a root web area, just walk up the tree to find the
  // nearest window.
  var parent = node;
  while (parent != null && parent.role != chrome.automation.RoleType.WINDOW) {
    parent = parent.parent;
  }
  return parent;
}

/**
 * Gets the current visiblity state for a given node.
 *
 * @param {AutomationNode} node The starting node.
 * @return {NodeState} the current node state.
 */
function getNodeState(node) {
  if (node === undefined || node.root === null || node.root === undefined) {
    // The node has been removed from the tree, perhaps because the
    // window was closed.
    return NodeState.NODE_STATE_INVALID;
  }
  // This might not be populated correctly on children nodes even if their
  // parents or roots are now invisible.
  // TODO: Update the C++ bindings to set 'invisible' automatically based
  // on parents, rather than going through parents in JS below.
  if (node.state.invisible) {
    return NodeState.NODE_STATE_INVISIBLE;
  }
  // Walk up the tree to make sure the window it is in is not invisible.
  var window = getNearestContainingWindow(node);
  if (window != null && window.state[chrome.automation.StateType.INVISIBLE]) {
    return NodeState.NODE_STATE_INVISIBLE;
  }
  // TODO: Also need a check for whether the window is minimized,
  // which would also return NodeState.NODE_STATE_INVISIBLE.
  return NodeState.NODE_STATE_NORMAL;
}

/**
 * Returns true if a node should be ignored by Select-to-Speak.
 * @param {AutomationNode} node The node to test
 * @param {boolean} includeOffscreen Whether to include offscreen nodes.
 * @return {boolean} whether this node should be ignored.
 */
function shouldIgnoreNode(node, includeOffscreen) {
  return (
      !node.name || !node.location || node.state.invisible ||
      (node.state.offscreen && !includeOffscreen));
}

/**
 * Finds all nodes within the subtree rooted at |node| that overlap
 * a given rectangle.
 * @param {AutomationNode} node The starting node.
 * @param {{left: number, top: number, width: number, height: number}} rect
 *     The bounding box to search.
 * @param {Array<AutomationNode>} nodes The matching node array to be
 *     populated.
 * @return {boolean} True if any matches are found.
 */
function findAllMatching(node, rect, nodes) {
  var found = false;
  for (var c = node.firstChild; c; c = c.nextSibling) {
    if (findAllMatching(c, rect, nodes))
      found = true;
  }

  if (found)
    return true;

  // Closure needs node.location check here to allow the next few
  // lines to compile.
  if (shouldIgnoreNode(node, /* don't include offscreen */ false) ||
      node.location === undefined)
    return false;

  if (overlaps(node.location, rect)) {
    if (!node.children || node.children.length == 0 ||
        node.children[0].role != RoleType.INLINE_TEXT_BOX) {
      // Only add a node if it has no inlineTextBox children. If
      // it has text children, they will be more precisely bounded
      // and specific, so no need to add the parent node.
      nodes.push(node);
      return true;
    }
  }

  return false;
}

/**
 * Class representing a position on the accessibility, made of a
 * selected node and the offset of that selection.
 * @typedef {{node: (!AutomationNode),
 *            offset: (number)}}
 */
var Position;

/**
 * Finds the deep equivalent node where a selection starts given a node
 * object and selection offset. This is meant to be used in conjunction with
 * the anchorObject/anchorOffset and focusObject/focusOffset of the
 * automation API.
 * @param {AutomationNode} parent The parent node of the selection,
 * similar to chrome.automation.focusObject.
 * @param {number} offset The integer offset of the selection. This is
 * similar to chrome.automation.focusOffset.
 * @return {!Position} The node matching the selected offset.
 */
function getDeepEquivalentForSelection(parent, offset) {
  if (parent.children.length == 0)
    return {node: parent, offset: offset};
  // Create a stack of children nodes to search through.
  let nodesToCheck = parent.children.slice().reverse();
  let index = 0;
  var node;
  // Delve down into the children recursively to find the
  // one at this offset.
  while (nodesToCheck.length > 0) {
    node = nodesToCheck.pop();
    if (node.children.length > 0) {
      nodesToCheck = nodesToCheck.concat(node.children.slice().reverse());
    } else {
      index += node.name ? node.name.length : 0;
      if (index > offset) {
        return {node: node, offset: offset - index + node.name.length};
      }
    }
  }
  // We are off the end of the last node.
  return {node: node, offset: node.name ? node.name.length : 0};
}

/**
 * @constructor
 */
var SelectToSpeak = function() {
  /** @private {AutomationNode} */
  this.node_ = null;

  /** @private {boolean} */
  this.trackingMouse_ = false;

  /** @private {boolean} */
  this.didTrackMouse_ = false;

  /** @private {boolean} */
  this.isSearchKeyDown_ = false;

  /** @private {boolean} */
  this.isSelectionKeyDown_ = false;

  /** @private {!Set<number>} */
  this.keysCurrentlyDown_ = new Set();

  /** @private {!Set<number>} */
  this.keysPressedTogether_ = new Set();

  /** @private {{x: number, y: number}} */
  this.mouseStart_ = {x: 0, y: 0};

  /** @private {{x: number, y: number}} */
  this.mouseEnd_ = {x: 0, y: 0};

  chrome.automation.getDesktop(function(desktop) {
    this.desktop_ = desktop;

    // After the user selects a region of the screen, we do a hit test at
    // the center of that box using the automation API. The result of the
    // hit test is a MOUSE_RELEASED accessibility event.
    desktop.addEventListener(
        EventType.MOUSE_RELEASED, this.onAutomationHitTest_.bind(this), true);

    // When Select-To-Speak is active, we do a hit test on the active node
    // and the result is a HOVER accessibility event. This event is used to
    // check that the current node is in the foreground window.
    desktop.addEventListener(
        EventType.HOVER, this.onHitTestCheckCurrentNodeMatches_.bind(this),
        true);
  }.bind(this));

  /** @private {?string} */
  this.voiceNameFromPrefs_ = null;

  /** @private {?string} */
  this.voiceNameFromLocale_ = null;

  /** @private {Set<string>} */
  this.validVoiceNames_ = new Set();

  /** @private {number} */
  this.speechRate_ = 1.0;

  /** @private {number} */
  this.speechPitch_ = 1.0;

  /** @private {boolean} */
  this.wordHighlight_ = true;

  /** @const {string} */
  this.color_ = '#f73a98';

  /** @private {string} */
  this.highlightColor_ = '#5e9bff';

  /** @private {?NodeGroupItem} */
  this.currentNode_ = null;

  /** @private {number} */
  this.currentNodeGroupIndex_ = -1;

  /** @private {?Object} */
  this.currentNodeWord_ = null;

  /** @private {?AutomationNode} */
  this.currentBlockParent_ = null;

  /** @private {boolean} */
  this.visible_ = true;

  /**
   * The interval ID from a call to setInterval, which is set whenever
   * speech is in progress.
   * @private {number|undefined}
   */
  this.intervalId_;

  // Enable reading selection at keystroke when experimental accessibility
  // features are enabled.
  // TODO(katie): When the feature is approved, remove this variable and
  // callback. The feature will be always enabled.
  this.readSelectionEnabled_ = false;
  chrome.commandLinePrivate.hasSwitch(
      'enable-experimental-accessibility-features', (result) => {
        this.readSelectionEnabled_ = result;
      });

  /** @private {Audio} */
  this.null_selection_tone_ = new Audio('earcons/null_selection.ogg');

  this.initPreferences_();

  this.setUpEventListeners_();
};

/** @const {number} */
SelectToSpeak.SEARCH_KEY_CODE = 91;

/** @const {number} */
SelectToSpeak.CONTROL_KEY_CODE = 17;

/** @const {number} */
SelectToSpeak.READ_SELECTION_KEY_CODE = 83;

/** @const {number} */
SelectToSpeak.NODE_STATE_TEST_INTERVAL_MS = 1000;

SelectToSpeak.prototype = {
  /**
   * Called when the mouse is pressed and the user is in a mode where
   * select-to-speak is capturing mouse events (for example holding down
   * Search).
   *
   * @param {!Event} evt The DOM event
   * @return {boolean} True if the default action should be performed;
   *    we always return false because we don't want any other event
   *    handlers to run.
   */
  onMouseDown_: function(evt) {
    // If the user hasn't clicked 'search', or if they are currently
    // trying to highlight a selection, don't track the mouse.
    if (!this.isSearchKeyDown_ || this.isSelectionKeyDown_)
      return false;

    this.trackingMouse_ = true;
    this.didTrackMouse_ = true;
    this.mouseStart_ = {x: evt.screenX, y: evt.screenY};
    chrome.tts.stop();

    // Fire a hit test event on click to warm up the cache.
    this.desktop_.hitTest(evt.screenX, evt.screenY, EventType.MOUSE_PRESSED);

    this.onMouseMove_(evt);
    return false;
  },

  /**
   * Called when the mouse is moved or dragged and the user is in a
   * mode where select-to-speak is capturing mouse events (for example
   * holding down Search).
   *
   * @param {!Event} evt The DOM event
   * @return {boolean} True if the default action should be performed.
   */
  onMouseMove_: function(evt) {
    if (!this.trackingMouse_)
      return false;

    var rect = rectFromPoints(
        this.mouseStart_.x, this.mouseStart_.y, evt.screenX, evt.screenY);
    chrome.accessibilityPrivate.setFocusRing([rect], this.color_);
    return false;
  },

  /**
   * Called when the mouse is released and the user is in a
   * mode where select-to-speak is capturing mouse events (for example
   * holding down Search).
   *
   * @param {!Event} evt
   * @return {boolean} True if the default action should be performed.
   */
  onMouseUp_: function(evt) {
    if (!this.trackingMouse_)
      return false;
    this.onMouseMove_(evt);
    this.trackingMouse_ = false;

    this.clearFocusRingAndNode_();

    this.mouseEnd_ = {x: evt.screenX, y: evt.screenY};
    var ctrX = Math.floor((this.mouseStart_.x + this.mouseEnd_.x) / 2);
    var ctrY = Math.floor((this.mouseStart_.y + this.mouseEnd_.y) / 2);

    // Do a hit test at the center of the area the user dragged over.
    // This will give us some context when searching the accessibility tree.
    // The hit test will result in a EventType.MOUSE_RELEASED event being
    // fired on the result of that hit test, which will trigger
    // onAutomationHitTest_.
    this.desktop_.hitTest(ctrX, ctrY, EventType.MOUSE_RELEASED);
    return false;
  },

  /**
   * Called in response to our hit test after the mouse is released,
   * when the user is in a mode where select-to-speak is capturing
   * mouse events (for example holding down Search).
   *
   * @param {!AutomationEvent} evt The automation event.
   */
  onAutomationHitTest_: function(evt) {
    // Walk up to the nearest window, web area, toolbar, or dialog that the
    // hit node is contained inside. Only speak objects within that
    // container. In the future we might include other container-like
    // roles here.
    var root = evt.target;
    // TODO: Use AutomationPredicate.root instead?
    while (root.parent && root.role != RoleType.WINDOW &&
           root.role != RoleType.ROOT_WEB_AREA &&
           root.role != RoleType.DESKTOP && root.role != RoleType.DIALOG &&
           root.role != RoleType.ALERT_DIALOG &&
           root.role != RoleType.TOOLBAR) {
      root = root.parent;
    }

    var rect = rectFromPoints(
        this.mouseStart_.x, this.mouseStart_.y, this.mouseEnd_.x,
        this.mouseEnd_.y);
    var nodes = [];
    chrome.automation.getFocus(function(focusedNode) {
      // In some cases, e.g. ARC++, the window received in the hit test request,
      // which is computed based on which window is the event handler for the
      // hit point, isn't the part of the tree that contains the actual
      // content. In such cases, use focus to get the root.
      if (!findAllMatching(root, rect, nodes) && focusedNode)
        findAllMatching(focusedNode.root, rect, nodes);
      this.startSpeechQueue_(nodes);
      this.recordStartEvent_(START_SPEECH_METHOD_MOUSE);
    }.bind(this));
  },

  /**
   * @param {!Event} evt
   */
  onKeyDown_: function(evt) {
    if (this.keysPressedTogether_.size == 0 &&
        evt.keyCode == SelectToSpeak.SEARCH_KEY_CODE) {
      this.isSearchKeyDown_ = true;
    } else if (
        this.readSelectionEnabled_ && this.keysCurrentlyDown_.size == 1 &&
        evt.keyCode == SelectToSpeak.READ_SELECTION_KEY_CODE &&
        !this.trackingMouse_) {
      // Only go into selection mode if we aren't already tracking the mouse.
      this.isSelectionKeyDown_ = true;
    } else if (!this.trackingMouse_) {
      // Some other key was pressed.
      this.isSearchKeyDown_ = false;
    }

    this.keysCurrentlyDown_.add(evt.keyCode);
    this.keysPressedTogether_.add(evt.keyCode);
  },

  /**
   * @param {!Event} evt
   */
  onKeyUp_: function(evt) {
    if (evt.keyCode == SelectToSpeak.READ_SELECTION_KEY_CODE) {
      if (this.isSelectionKeyDown_ && this.keysPressedTogether_.size == 2 &&
          this.keysPressedTogether_.has(evt.keyCode) &&
          this.keysPressedTogether_.has(SelectToSpeak.SEARCH_KEY_CODE)) {
        chrome.tts.isSpeaking(this.cancelIfSpeaking_.bind(this));
        chrome.automation.getFocus(this.requestSpeakSelectedText_.bind(this));
      }
      this.isSelectionKeyDown_ = false;
    } else if (evt.keyCode == SelectToSpeak.SEARCH_KEY_CODE) {
      this.isSearchKeyDown_ = false;

      // If we were in the middle of tracking the mouse, cancel it.
      if (this.trackingMouse_) {
        this.trackingMouse_ = false;
        this.stopAll_();
      }
    }

    // Stop speech when the user taps and releases Control or Search
    // without using the mouse or pressing any other keys along the way.
    if (!this.didTrackMouse_ &&
        (evt.keyCode == SelectToSpeak.SEARCH_KEY_CODE ||
         evt.keyCode == SelectToSpeak.CONTROL_KEY_CODE) &&
        this.keysPressedTogether_.has(evt.keyCode) &&
        this.keysPressedTogether_.size == 1) {
      this.trackingMouse_ = false;
      chrome.tts.isSpeaking(this.cancelIfSpeaking_.bind(this));
    }

    this.keysCurrentlyDown_.delete(evt.keyCode);
    if (this.keysCurrentlyDown_.size == 0) {
      this.keysPressedTogether_.clear();
      this.didTrackMouse_ = false;
    }
  },

  /**
   * Queues up selected text for reading.
   */
  requestSpeakSelectedText_: function(focusedNode) {
    // If nothing is selected, return early.
    if (!focusedNode || !focusedNode.root || !focusedNode.root.anchorObject ||
        !focusedNode.root.focusObject) {
      this.onNullSelection_();
      return;
    }
    let anchorObject = focusedNode.root.anchorObject;
    let anchorOffset = focusedNode.root.anchorOffset || 0;
    let focusObject = focusedNode.root.focusObject;
    let focusOffset = focusedNode.root.focusOffset || 0;
    if (anchorObject === focusObject && anchorOffset == focusOffset) {
      this.onNullSelection_();
      return;
    }
    let anchorPosition =
        getDeepEquivalentForSelection(anchorObject, anchorOffset);
    let focusPosition = getDeepEquivalentForSelection(focusObject, focusOffset);
    let firstPosition;
    let lastPosition;
    if (anchorPosition.node === focusPosition.node) {
      if (anchorPosition.offset < focusPosition.offset) {
        firstPosition = anchorPosition;
        lastPosition = focusPosition;
      } else {
        lastPosition = anchorPosition;
        firstPosition = focusPosition;
      }
    } else {
      let dir =
          AutomationUtil.getDirection(anchorPosition.node, focusPosition.node);
      // Highlighting may be forwards or backwards. Make sure we start at the
      // first node.
      if (dir == constants.Dir.FORWARD) {
        firstPosition = anchorPosition;
        lastPosition = focusPosition;
      } else {
        lastPosition = anchorPosition;
        firstPosition = focusPosition;
      }
    }

    // Adjust such that non-text types don't have offsets into their names.
    if (firstPosition.node.role != RoleType.STATIC_TEXT &&
        firstPosition.node.role != RoleType.INLINE_TEXT_BOX) {
      firstPosition.offset = 0;
    }
    if (lastPosition.node.role != RoleType.STATIC_TEXT &&
        lastPosition.node.role != RoleType.INLINE_TEXT_BOX) {
      lastPosition.offset =
          lastPosition.node.name ? lastPosition.node.name.length : 0;
    }

    let nodes = [];
    let selectedNode = firstPosition.node;
    if (selectedNode.name && firstPosition.offset < selectedNode.name.length &&
        !shouldIgnoreNode(selectedNode, /* include offscreen */ true)) {
      // Initialize to the first node in the list if it's valid and inside
      // of the offset bounds.
      nodes.push(selectedNode);
    } else {
      // The selectedNode actually has no content selected. Let the list
      // initialize itself to the next node in the loop below.
      // This can happen if you click-and-drag starting after the text in
      // a first line to highlight text in a second line.
      firstPosition.offset = 0;
    }
    while (selectedNode && selectedNode != lastPosition.node &&
           AutomationUtil.getDirection(selectedNode, lastPosition.node) ===
               constants.Dir.FORWARD) {
      // TODO: Is there a way to optimize the directionality checking of
      // AutomationUtil.getDirection(selectedNode, finalNode)?
      // For example, by making a helper and storing partial computation?
      selectedNode = AutomationUtil.findNextNode(
          selectedNode, constants.Dir.FORWARD,
          AutomationPredicate.leafWithText);
      if (selectedNode) {
        if (!shouldIgnoreNode(selectedNode, /* include offscreen */ true))
          nodes.push(selectedNode);
      } else {
        break;
      }
    }

    this.startSpeechQueue_(nodes, firstPosition.offset, lastPosition.offset);

    this.recordStartEvent_(START_SPEECH_METHOD_KEYSTROKE);
  },

  /**
   * Plays a tone to let the user know they did the correct
   * keystroke but nothing was selected.
   */
  onNullSelection_: function() {
    this.null_selection_tone_.play();
  },

  /**
   * Stop speech. If speech was in-progress, the interruption
   * event will be caught and clearFocusRingAndNode_ will be
   * called, stopping visual feedback as well.
   * If speech was not in progress, i.e. if the user was drawing
   * a focus ring on the screen, this still clears the visual
   * focus ring.
   */
  stopAll_: function() {
    chrome.tts.stop();
    this.clearFocusRing_();
  },

  /**
   * Clears the current focus ring and node, but does
   * not stop the speech.
   */
  clearFocusRingAndNode_: function() {
    this.clearFocusRing_();
    // Clear the node and also stop the interval testing.
    this.currentNode_ = null;
    this.currentNodeGroupIndex_ = -1;
    this.currentNodeWord_ = null;
    clearInterval(this.intervalId_);
    this.intervalId_ = undefined;
  },

  /**
   * Clears the focus ring, but does not clear the current
   * node.
   */
  clearFocusRing_: function() {
    chrome.accessibilityPrivate.setFocusRing([]);
    chrome.accessibilityPrivate.setHighlights([], this.highlightColor_);
  },

  /**
   * Set up event listeners for mouse and keyboard events. These are
   * forwarded to us from the SelectToSpeakEventHandler so they should
   * be interpreted as global events on the whole screen, not local to
   * any particular window.
   */
  setUpEventListeners_: function() {
    document.addEventListener('keydown', this.onKeyDown_.bind(this));
    document.addEventListener('keyup', this.onKeyUp_.bind(this));
    document.addEventListener('mousedown', this.onMouseDown_.bind(this));
    document.addEventListener('mousemove', this.onMouseMove_.bind(this));
    document.addEventListener('mouseup', this.onMouseUp_.bind(this));
  },

  /**
   * Enqueue speech commands for all of the given nodes.
   * @param {Array<AutomationNode>} nodes The nodes to speak.
   * @param {number=} opt_startIndex The index into the first node's text
   * at which to start speaking. If this is not passed, will start at 0.
   * @param {number=} opt_endIndex The index into the last node's text
   * at which to end speech. If this is not passed, will stop at the end.
   */
  startSpeechQueue_: function(nodes, opt_startIndex, opt_endIndex) {
    chrome.tts.stop();
    if (this.intervalRef_ !== undefined) {
      clearInterval(this.intervalRef_);
    }
    this.intervalRef_ = setInterval(
        this.testCurrentNode_.bind(this),
        SelectToSpeak.NODE_STATE_TEST_INTERVAL_MS);
    for (var i = 0; i < nodes.length; i++) {
      let node = nodes[i];
      let nodeGroup = buildNodeGroup(nodes, i);
      if (i == 0) {
        // We need to start in the middle of a node. Remove all text before
        // the start index so that it is not spoken.
        // Backfill with spaces so that index counting functions don't get
        // confused.
        // Must check opt_startIndex in its own if statement to make the
        // Closure compiler happy.
        if (opt_startIndex !== undefined) {
          if (nodeGroup.nodes[0].hasInlineText) {
            // The first node is inlineText type. Find the start index in
            // its staticText parent.
            let startIndexInParent = getStartCharIndexInParent(nodes[0]);
            opt_startIndex += startIndexInParent;
            nodeGroup.text = ' '.repeat(opt_startIndex) +
                nodeGroup.text.substr(opt_startIndex);
          }
        }
      }
      let isFirst = i == 0;
      // Advance i to the end of this group, to skip all nodes it contains.
      i = nodeGroup.endIndex;
      let isLast = (i == nodes.length - 1);
      if (isLast && opt_endIndex !== undefined && nodeGroup.nodes.length > 0) {
        // We need to stop in the middle of a node. Remove all text after
        // the end index so it is not spoken. Backfill with spaces so that
        // index counting functions don't get confused.
        // This only applies to inlineText nodes.
        if (nodeGroup.nodes[nodeGroup.nodes.length - 1].hasInlineText) {
          let startIndexInParent = getStartCharIndexInParent(nodes[i]);
          opt_endIndex += startIndexInParent;
          nodeGroup.text = nodeGroup.text.substr(
              0,
              nodeGroup.nodes[nodeGroup.nodes.length - 1].startChar +
                  opt_endIndex);
        }
      }
      if (nodeGroup.nodes.length == 0 && !isLast) {
        continue;
      }

      let options = {
        rate: this.speechRate_,
        pitch: this.speechPitch_,
        'enqueue': true,
        onEvent:
            (event) => {
              if (event.type == 'start' && nodeGroup.nodes.length > 0) {
                this.currentBlockParent_ = nodeGroup.blockParent;
                this.currentNodeGroupIndex_ = 0;
                this.currentNode_ =
                    nodeGroup.nodes[this.currentNodeGroupIndex_];
                if (this.wordHighlight_) {
                  // At 'start', find the first word and highlight that.
                  // Clear the previous word in the node.
                  this.currentNodeWord_ = null;
                  // If this is the first nodeGroup, pass the opt_startIndex.
                  // If this is the last nodeGroup, pass the opt_endIndex.
                  this.updateNodeHighlight_(
                      nodeGroup.text, event.charIndex,
                      isFirst ? opt_startIndex : undefined,
                      isLast ? opt_endIndex : undefined);
                } else {
                  this.testCurrentNode_();
                }
              } else if (
                  event.type == 'interrupted' || event.type == 'cancelled') {
                this.clearFocusRingAndNode_();
              } else if (event.type == 'end') {
                if (isLast) {
                  this.clearFocusRingAndNode_();
                }
              } else if (event.type == 'word') {
                console.debug(
                    nodeGroup.text + ' (index ' + event.charIndex + ')');
                console.debug('-'.repeat(event.charIndex) + '^');
                if (this.currentNodeGroupIndex_ + 1 < nodeGroup.nodes.length) {
                  let next = nodeGroup.nodes[this.currentNodeGroupIndex_ + 1];
                  // Check if we've reached this next node yet using the
                  // character index of the event. Add 1 for the space character
                  // between words, and another to make it to the start of the
                  // next node name.
                  if (event.charIndex + 2 >= next.startChar) {
                    // Move to the next node.
                    this.currentNodeGroupIndex_ += 1;
                    this.currentNode_ = next;
                    this.currentNodeWord_ = null;
                    if (!this.wordHighlight_) {
                      // If we are doing a per-word highlight, we will test the
                      // node after figuring out what the currently highlighted
                      // word is.
                      this.testCurrentNode_();
                    }
                  }
                }
                if (this.wordHighlight_) {
                  this.updateNodeHighlight_(
                      nodeGroup.text, event.charIndex, undefined,
                      isLast ? opt_endIndex : undefined);
                } else {
                  this.currentNodeWord_ = null;
                }
              }
            }
      };

      // Pick the voice name from prefs first, or the one that matches
      // the locale next, but don't pick a voice that isn't currently
      // loaded. If no voices are found, leave the voiceName option
      // unset to let the browser try to route the speech request
      // anyway if possible.
      console.debug('Pref: ' + this.voiceNameFromPrefs_);
      console.debug('Locale: ' + this.voiceNameFromLocale_);
      var valid = '';
      this.validVoiceNames_.forEach(function(voiceName) {
        if (valid)
          valid += ',';
        valid += voiceName;
      });
      console.debug('Valid: ' + valid);
      if (this.voiceNameFromPrefs_ &&
          this.validVoiceNames_.has(this.voiceNameFromPrefs_)) {
        options['voiceName'] = this.voiceNameFromPrefs_;
      } else if (
          this.voiceNameFromLocale_ &&
          this.validVoiceNames_.has(this.voiceNameFromLocale_)) {
        options['voiceName'] = this.voiceNameFromLocale_;
      }

      chrome.tts.speak(nodeGroup.text || '', options);
    }
  },

  /**
   * Cancels the current speech queue if speech is in progress.
   */
  cancelIfSpeaking_: function(speaking) {
    if (speaking) {
      this.stopAll_();
      this.recordCancelEvent_();
    }
  },

  /**
   * Converts the speech rate into an enum based on
   * tools/metrics/histograms/enums.xml.
   * These values are persisted to logs. Entries should not be
   * renumbered and numeric values should never be reused.
   * @return {number} the current speech rate as an int for metrics.
   */
  speechRateToSparceHistogramInt_: function() {
    return this.speechRate_ * 100;
  },

  /**
   * Converts the speech pitch into an enum based on
   * tools/metrics/histograms/enums.xml.
   * These values are persisted to logs. Entries should not be
   * renumbered and numeric values should never be reused.
   * @return {number} the current speech pitch as an int for metrics.
   */
  speechPitchToSparceHistogramInt_: function() {
    return this.speechPitch_ * 100;
  },

  /**
   * Records an event that Select-to-Speak has begun speaking.
   * @param {number} method The CrosSelectToSpeakStartSpeechMethod enum
   *    that reflects how this event was triggered by the user.
   */
  recordStartEvent_: function(method) {
    chrome.metricsPrivate.recordUserAction(
        'Accessibility.CrosSelectToSpeak.StartSpeech');
    chrome.metricsPrivate.recordSparseValue(
        'Accessibility.CrosSelectToSpeak.SpeechRate',
        this.speechRateToSparceHistogramInt_());
    chrome.metricsPrivate.recordSparseValue(
        'Accessibility.CrosSelectToSpeak.SpeechPitch',
        this.speechPitchToSparceHistogramInt_());
    chrome.metricsPrivate.recordBoolean(
        'Accessibility.CrosSelectToSpeak.WordHighlighting',
        this.wordHighlight_);
    chrome.metricsPrivate.recordEnumerationValue(
        'Accessibility.CrosSelectToSpeak.StartSpeechMethod', method,
        START_SPEECH_METHOD_COUNT);
  },

  /**
   * Records an event that Select-to-Speak speech has been canceled.
   */
  recordCancelEvent_: function() {
    chrome.metricsPrivate.recordUserAction(
        'Accessibility.CrosSelectToSpeak.CancelSpeech');
  },

  /**
   * Loads preferences from chrome.storage, sets default values if
   * necessary, and registers a listener to update prefs when they
   * change.
   */
  initPreferences_: function() {
    var updatePrefs =
        (function() {
          chrome.storage.sync.get(
              ['voice', 'rate', 'pitch', 'wordHighlight', 'highlightColor'],
              (function(prefs) {
                if (prefs['voice']) {
                  this.voiceNameFromPrefs_ = prefs['voice'];
                }
                if (prefs['rate']) {
                  this.speechRate_ = parseFloat(prefs['rate']);
                } else {
                  chrome.storage.sync.set({'rate': this.speechRate_});
                }
                if (prefs['pitch']) {
                  this.speechPitch_ = parseFloat(prefs['pitch']);
                } else {
                  chrome.storage.sync.set({'pitch': this.speechPitch_});
                }
                if (prefs['wordHighlight'] !== undefined) {
                  this.wordHighlight_ = prefs['wordHighlight'];
                } else {
                  chrome.storage.sync.set(
                      {'wordHighlight': this.wordHighlight_});
                }
                if (prefs['highlightColor']) {
                  this.highlightColor_ = prefs['highlightColor'];
                } else {
                  chrome.storage.sync.set(
                      {'highlightColor': this.highlightColor_});
                }
              }).bind(this));
        }).bind(this);

    updatePrefs();
    chrome.storage.onChanged.addListener(updatePrefs);

    this.updateDefaultVoice_();
    window.speechSynthesis.onvoiceschanged = (function() {
                                               this.updateDefaultVoice_();
                                             }).bind(this);
  },

  /**
   * Get the list of TTS voices, and set the default voice if not already set.
   */
  updateDefaultVoice_: function() {
    var uiLocale = chrome.i18n.getMessage('@@ui_locale');
    uiLocale = uiLocale.replace('_', '-').toLowerCase();

    chrome.tts.getVoices(
        (function(voices) {
          console.debug('updateDefaultVoice_ voices: ' + voices.length);
          this.validVoiceNames_ = new Set();

          if (voices.length == 0)
            return;

          voices.forEach((function(voice) {
                           this.validVoiceNames_.add(voice.voiceName);
                         }).bind(this));

          voices.sort(function(a, b) {
            function score(voice) {
              var lang = voice.lang.toLowerCase();
              var s = 0;
              if (lang == uiLocale)
                s += 2;
              if (lang.substr(0, 2) == uiLocale.substr(0, 2))
                s += 1;
              return s;
            }
            return score(b) - score(a);
          });

          this.voiceNameFromLocale_ = voices[0].voiceName;

          chrome.storage.sync.get(
              ['voice'],
              (function(prefs) {
                if (!prefs['voice']) {
                  chrome.storage.sync.set({'voice': voices[0].voiceName});
                }
              }).bind(this));
        }).bind(this));
  },

  /**
   * Hides the speech and focus ring states if necessary based on a node's
   * current state.
   *
   * @param {NodeGroupItem} nodeGroupItem The node to use for updates
   * @param {boolean} inForeground Whether the node is in the foreground window.
   */
  updateFromNodeState_: function(nodeGroupItem, inForeground) {
    switch (getNodeState(nodeGroupItem.node)) {
      case NodeState.NODE_STATE_INVALID:
        // If the node is invalid, stop speaking entirely.
        this.stopAll_();
        break;
      case NodeState.NODE_STATE_INVISIBLE:
        // If it is invisible but still valid, just clear the focus ring.
        // Don't clear the current node because we may still use it
        // if it becomes visibile later.
        this.clearFocusRing_();
        this.visible_ = false;
        break;
      case NodeState.NODE_STATE_NORMAL:
      default:
        if (inForeground && !this.visible_) {
          this.visible_ = true;
          // Just came to the foreground.
          this.updateHighlightAndFocus_(nodeGroupItem);
        } else if (!inForeground) {
          this.clearFocusRing_();
          this.visible_ = false;
        }
    }
  },

  /**
   * Updates the speech and focus ring states based on a node's current state.
   *
   * @param {NodeGroupItem} nodeGroupItem The node to use for updates
   */
  updateHighlightAndFocus_: function(nodeGroupItem) {
    if (!this.visible_) {
      return;
    }
    let node = nodeGroupItem.hasInlineText && this.currentNodeWord_ ?
        findInlineTextNodeByCharacterIndex(
            nodeGroupItem.node, this.currentNodeWord_.start) :
        nodeGroupItem.node;
    if (this.wordHighlight_ && this.currentNodeWord_ != null) {
      // Only show the highlight if this is an inline text box.
      // Otherwise we'd be highlighting entire nodes, like images.
      // Highlight should be only for text.
      // Note that boundsForRange doesn't work on staticText.
      if (node.role == RoleType.INLINE_TEXT_BOX) {
        let charIndexInParent = getStartCharIndexInParent(node);
        chrome.accessibilityPrivate.setHighlights(
            [node.boundsForRange(
                this.currentNodeWord_.start - charIndexInParent,
                this.currentNodeWord_.end - charIndexInParent)],
            this.highlightColor_);
      } else {
        chrome.accessibilityPrivate.setHighlights([], this.highlightColor_);
      }
    }
    // Show the parent element of the currently verbalized node with the
    // focus ring. This is a nicer user-facing behavior than jumping from
    // node to node, as nodes may not correspond well to paragraphs or
    // blocks.
    // TODO: Better test: has no siblings in the group, highlight just
    // the one node. if it has siblings, highlight the parent.
    if (this.currentBlockParent_ != null &&
        node.role == RoleType.INLINE_TEXT_BOX) {
      chrome.accessibilityPrivate.setFocusRing(
          [this.currentBlockParent_.location], this.color_);
    } else {
      chrome.accessibilityPrivate.setFocusRing([node.location], this.color_);
    }
  },

  /**
   * Tests the active node to make sure the bounds are drawn correctly.
   */
  testCurrentNode_: function() {
    if (this.currentNode_ == null) {
      return;
    }
    if (this.currentNode_.node.location === undefined) {
      // Don't do the hit test because there is no location to test against.
      // Just directly update Select To Speak from node state.
      this.updateFromNodeState_(this.currentNode_, false);
    } else {
      this.updateHighlightAndFocus_(this.currentNode_);
      // Do a hit test to make sure the node is not in a background window
      // or minimimized. On the result checkCurrentNodeMatchesHitTest_ will be
      // called, and we will use that result plus the currentNode's state to
      // deterimine how to set the focus and whether to stop speech.
      this.desktop_.hitTest(
          this.currentNode_.node.location.left,
          this.currentNode_.node.location.top, EventType.HOVER);
    }
  },

  /**
   * Checks that the current node is in the same window as the HitTest node.
   * Uses this information to update Select-To-Speak from node state.
   */
  onHitTestCheckCurrentNodeMatches_: function(evt) {
    if (this.currentNode_ == null) {
      return;
    }
    chrome.automation.getFocus(function(focusedNode) {
      var window = getNearestContainingWindow(evt.target);
      var currentWindow = getNearestContainingWindow(this.currentNode_.node);
      var inForeground =
          currentWindow != null && window != null && currentWindow == window;
      if (!inForeground && focusedNode && currentWindow) {
        // See if the focused node window matches the currentWindow.
        // This may happen in some cases, for example, ARC++, when the window
        // which received the hit test request is not part of the tree that
        // contains the actual content. In such cases, use focus to get the
        // appropriate root.
        var focusedWindow = getNearestContainingWindow(focusedNode.root);
        inForeground = focusedWindow != null && currentWindow == focusedWindow;
      }
      this.updateFromNodeState_(this.currentNode_, inForeground);
    }.bind(this));
  },

  /**
   * Updates the currently highlighted node word based on the current text
   * and the character index of an event.
   * @param {string} text The current text
   * @param {number} charIndex The index of a current event in the text.
   * @param {number=} opt_startIndex The index at which to start the highlight.
   * This takes precedence over the charIndex.
   * @param {number=} opt_endIndex The index at which to end the highlight. This
   * takes precedence over the next word end.
   */
  updateNodeHighlight_: function(
      text, charIndex, opt_startIndex, opt_endIndex) {
    if (charIndex >= text.length) {
      // No need to do work if we are at the end of the paragraph.
      return;
    }
    // Get the next word based on the event's charIndex.
    let nextWordStart = getNextWordStart(text, charIndex, this.currentNode_);
    let nextWordEnd = getNextWordEnd(
        text, opt_startIndex === undefined ? nextWordStart : opt_startIndex,
        this.currentNode_);
    // Map the next word into the node's index from the text.
    let nodeStart = opt_startIndex === undefined ?
        nextWordStart - this.currentNode_.startChar :
        opt_startIndex - this.currentNode_.startChar;
    let nodeEnd = Math.min(
        nextWordEnd - this.currentNode_.startChar,
        this.currentNode_.node.name.length);
    if ((this.currentNodeWord_ == null ||
         nodeStart >= this.currentNodeWord_.end) &&
        nodeStart < nodeEnd) {
      // Only update the bounds if they have increased from the
      // previous node. Because tts may send multiple callbacks
      // for the end of one word and the beginning of the next,
      // checking that the current word has changed allows us to
      // reduce extra work.
      this.currentNodeWord_ = {'start': nodeStart, 'end': nodeEnd};
      this.testCurrentNode_();
    }
  },

  // ---------- Functionality for testing ---------- //

  /**
   * Fires a mock key down event for testing.
   * @param {!Event} event The fake key down event to fire. The object
   * must contain at minimum a keyCode.
   */
  fireMockKeyDownEvent: function(event) {
    this.onKeyDown_(event);
  },

  /**
   * Fires a mock key up event for testing.
   * @param {!Event} event The fake key up event to fire. The object
   * must contain at minimum a keyCode.
   */
  fireMockKeyUpEvent: function(event) {
    this.onKeyUp_(event);
  },

  /**
   * Fires a mock mouse down event for testing.
   * @param {!Event} event The fake mouse down event to fire. The object
   * must contain at minimum a screenX and a screenY.
   */
  fireMockMouseDownEvent: function(event) {
    this.onMouseDown_(event);
  },

  /**
   * Fires a mock mouse up event for testing.
   * @param {!Event} event The fake mouse up event to fire. The object
   * must contain at minimum a screenX and a screenY.
   */
  fireMockMouseUpEvent: function(event) {
    this.onMouseUp_(event);
  },

  /**
   * Overrides default setting to read selected text and enables the
   * ability to read selected text at a keystroke. Should only be used
   * for testing.
   */
  enableReadSelectedTextForTesting: function() {
    this.readSelectionEnabled_ = true;
  }
};

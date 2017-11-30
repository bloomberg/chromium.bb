// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var AutomationEvent = chrome.automation.AutomationEvent;
var EventType = chrome.automation.EventType;
var RoleType = chrome.automation.RoleType;

/**
 * Return the rect that encloses two points.
 * @param {number} x1 The first x coordinate.
 * @param {number} y1 The first y coordinate.
 * @param {number} x2 The second x coordinate.
 * @param {number} y2 The second x coordinate.
 * @return {{left: number, top: number, width: number, height: number}}
 */
function rectFromPoints(x1, y1, x2, y2) {
  var left = Math.min(x1, x2);
  var right = Math.max(x1, x2);
  var top = Math.min(y1, y2);
  var bottom = Math.max(y1, y2);
  return {left: left, top: top, width: right - left, height: bottom - top};
}

/**
 * Returns true if |rect1| and |rect2| overlap. The rects must define
 * left, top, width, and height.
 * @param {{left: number, top: number, width: number, height: number}} rect1
 * @param {{left: number, top: number, width: number, height: number}} rect2
 * @return {boolean} True if the rects overlap.
 */
function overlaps(rect1, rect2) {
  var l1 = rect1.left;
  var r1 = rect1.left + rect1.width;
  var t1 = rect1.top;
  var b1 = rect1.top + rect1.height;
  var l2 = rect2.left;
  var r2 = rect2.left + rect2.width;
  var t2 = rect2.top;
  var b2 = rect2.top + rect2.height;
  return (l1 < r2 && r1 > l2 && t1 < b2 && b1 > t2);
}

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
  if (node.root == null) {
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
  var parent = getNearestContainingWindow(node);
  if (parent != null && parent.state[chrome.automation.StateType.INVISIBLE]) {
    return NodeState.NODE_STATE_INVISIBLE;
  }
  // TODO: Also need a check for whether the window is minimized,
  // which would also return NodeState.NODE_STATE_INVISIBLE.
  return NodeState.NODE_STATE_NORMAL;
}

/**
 * Regular expression to find the start of the next word after a word boundary.
 * We cannot use \b\W to find the next word because it does not match many
 * unicode characters.
 * @type {RegExp}
 */
const WORD_START_REGEXP = /\b\S/;

/**
 * Regular expression to find the end of the next word, which is followed by
 * whitespace. We cannot use \w\b to find the end of the previous word because
 * \w does not know about many unicode characters.
 * @type {RegExp}
 */
const WORD_END_REGEXP = /\S\s/;

/**
 * Searches through text starting at an index to find the next word's
 * start boundary.
 * @param {string|undefined} text The string to search through
 * @param {number} indexAfter The index into text at which to start
 *      searching.
 * @param {NodeGroupItem} nodeGroupItem The node whose name we are
 *      searching through.
 * @return {number} The index of the next word's start
 */
function getNextWordStart(text, indexAfter, nodeGroupItem) {
  if (nodeGroupItem.node.wordStarts === undefined) {
    // Try to parse using a regex, which is imperfect.
    // Fall back to the given index if we can't find a match.
    return nextWordHelper(text, indexAfter, WORD_START_REGEXP, indexAfter);
  }
  for (var i = 0; i < nodeGroupItem.node.wordStarts.length; i++) {
    if (nodeGroupItem.node.wordStarts[i] + nodeGroupItem.startChar <
        indexAfter) {
      continue;
    }
    return nodeGroupItem.node.wordStarts[i] + nodeGroupItem.startChar;
  }
  // Default.
  return indexAfter;
}

/**
 * Searches through text starting at an index to find the next word's
 * end boundary.
 * @param {string|undefined} text The string to search through
 * @param {number} indexAfter The index into text at which to start
 *      searching.
 * @param {NodeGroupItem} nodeGroupItem The node whose name we are
 *      searching through.
 * @return {number} The index of the next word's end
 */
function getNextWordEnd(text, indexAfter, nodeGroupItem) {
  if (nodeGroupItem.node.wordEnds === undefined) {
    // Try to parse using a regex, which is imperfect.
    // Fall back to the full length of the text if we can't find a match.
    return nextWordHelper(text, indexAfter, WORD_END_REGEXP, text.length - 1) +
        1;
  }
  for (var i = 0; i < nodeGroupItem.node.wordEnds.length; i++) {
    if (nodeGroupItem.node.wordEnds[i] + nodeGroupItem.startChar < indexAfter) {
      continue;
    }
    return nodeGroupItem.node.wordEnds[i] + nodeGroupItem.startChar;
  }
  // Default.
  return text.length;
}

/**
 * Searches through text to find the first index of a regular expression
 * after a given starting index. Returns a default value if no match is
 * found.
 * @param {string|undefined} text The string to search through
 * @param {number} indexAfter The index at which to start searching
 * @param {RegExp} re A regular expression to search for
 * @param {number} defaultValue The default value to return if no
                     match is found.
 * @return {number} The index found by the regular expression, or -1
 *                    if none found.
 */
function nextWordHelper(text, indexAfter, re, defaultValue) {
  if (text === undefined) {
    return defaultValue;
  }
  let result = re.exec(text.substr(indexAfter));
  if (result != null && result.length > 0) {
    return indexAfter + result.index;
  }
  return defaultValue;
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

  if (!node.name || !node.location || node.state.offscreen ||
      node.state.invisible)
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

  /** @private {boolean} */
  this.wordHighlight_ = false;

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

  /**
   * The interval ID from a call to setInterval, which is set whenever
   * speech is in progress.
   * @private {number|undefined}
   */
  this.intervalId_;

  this.initPreferences_();

  this.setUpEventListeners_();
};

/** @const {number} */
SelectToSpeak.SEARCH_KEY_CODE = 91;

/** @const {number} */
SelectToSpeak.CONTROL_KEY_CODE = 17;

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
    if (!this.isSearchKeyDown_)
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
      this.recordStartEvent_();
    }.bind(this));
  },

  /**
   * @param {!Event} evt
   */
  onKeyDown_: function(evt) {
    if (this.keysPressedTogether_.size == 0 &&
        evt.keyCode == SelectToSpeak.SEARCH_KEY_CODE) {
      this.isSearchKeyDown_ = true;
    } else if (!this.trackingMouse_) {
      this.isSearchKeyDown_ = false;
    }

    this.keysCurrentlyDown_.add(evt.keyCode);
    this.keysPressedTogether_.add(evt.keyCode);
  },

  /**
   * @param {!Event} evt
   */
  onKeyUp_: function(evt) {
    if (evt.keyCode == SelectToSpeak.SEARCH_KEY_CODE) {
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
   */
  startSpeechQueue_: function(nodes) {
    chrome.tts.stop();
    if (this.intervalRef_ !== undefined) {
      clearInterval(this.intervalRef_);
    }
    this.intervalRef_ = setInterval(
        this.testCurrentNode_.bind(this),
        SelectToSpeak.NODE_STATE_TEST_INTERVAL_MS);
    for (var i = 0; i < nodes.length; i++) {
      let node = nodes[i];
      let textToSpeak = '';
      let nodeGroup = buildNodeGroup(nodes, i);
      // Advance i to the end of this group, to skip all nodes it contains.
      i = nodeGroup.endIndex;
      textToSpeak = nodeGroup.text;
      let isLast = (i == nodes.length - 1);
      if (nodeGroup.nodes.length == 0 && !isLast) {
        continue;
      }

      let options = {
        rate: this.speechRate_,
        'enqueue': true,
        onEvent:
            (function(nodeGroup, isLast, event) {
              if (event.type == 'start' && nodeGroup.nodes.length > 0) {
                if (nodeGroup.endIndex != nodeGroup.startIndex) {
                  // The block parent only matters if the block has more
                  // than one item in it.
                  this.currentBlockParent_ = nodeGroup.blockParent;
                } else {
                  this.currentBlockParent_ = null;
                }
                this.currentNodeGroupIndex_ = 0;
                this.currentNode_ =
                    nodeGroup.nodes[this.currentNodeGroupIndex_];
                if (this.wordHighlight_) {
                  // At 'start', find the first word and highlight that.
                  // Clear the previous word in the node.
                  this.currentNodeWord_ = null;
                  this.updateNodeHighlight_(nodeGroup.text, 0);
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
                    // TODO: If the next node is a non-word character, like an
                    // open or closed paren, we should keep moving.
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
                  this.updateNodeHighlight_(nodeGroup.text, event.charIndex);
                } else {
                  this.currentNodeWord_ = null;
                }
              }
            }).bind(this, nodeGroup, isLast)
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

      chrome.tts.speak(textToSpeak || '', options);
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
   * Records an event that Select-to-Speak has begun speaking.
   */
  recordStartEvent_: function() {
    // TODO(katie): Add tracking for speech rate, how STS was activated,
    // whether word highlighting is on or off (as histograms?).
    chrome.metricsPrivate.recordUserAction(
        'Accessibility.CrosSelectToSpeak.StartSpeech');
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
              ['voice', 'rate', 'wordHighlight', 'highlightColor'],
              (function(prefs) {
                if (prefs['voice']) {
                  this.voiceNameFromPrefs_ = prefs['voice'];
                }
                if (prefs['rate']) {
                  this.speechRate_ = parseFloat(prefs['rate']);
                } else {
                  chrome.storage.sync.set({'rate': this.speechRate_});
                }
                if (prefs['wordHighlight'] !== undefined) {
                  this.wordHighlight_ = prefs['wordHighlight'];
                }
                if (prefs['highlightColor']) {
                  this.highlightColor_ = prefs['highlightColor'];
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
   * Updates the speech and focus ring states based on a node's current state.
   *
   * @param {AutomationNode} node The node to use for updates
   * @param {boolean} inForeground Whether the node is in the foreground window.
   */
  updateFromNodeState_: function(node, inForeground) {
    switch (getNodeState(node)) {
      case NodeState.NODE_STATE_INVALID:
        // If the node is invalid, stop speaking entirely.
        this.stopAll_();
        break;
      case NodeState.NODE_STATE_INVISIBLE:
        // If it is invisible but still valid, just clear the focus ring.
        // Don't clear the current node because we may still use it
        // if it becomes visibile later.
        this.clearFocusRing_();
        break;
      case NodeState.NODE_STATE_NORMAL:
      default:
        if (inForeground) {
          if (this.wordHighlight_ && this.currentNodeWord_ != null) {
            // Only show the highlight if this is an inline text box.
            // Otherwise we'd be highlighting entire nodes, like images.
            // Highlight should be only for text.
            // Note that boundsForRange doesn't work on staticText.
            if (node.role == RoleType.INLINE_TEXT_BOX) {
              chrome.accessibilityPrivate.setHighlights(
                  [node.boundsForRange(
                      this.currentNodeWord_.start, this.currentNodeWord_.end)],
                  this.highlightColor_);
            } else {
              chrome.accessibilityPrivate.setHighlights(
                  [], this.highlightColor_);
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
            chrome.accessibilityPrivate.setFocusRing(
                [node.location], this.color_);
          }
        } else {
          this.clearFocusRing_();
        }
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
      this.updateFromNodeState_(this.currentNode_.node, false);
    } else {
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
    var parent = getNearestContainingWindow(evt.target);
    var currentParent = getNearestContainingWindow(this.currentNode_.node);
    var inForeground =
        currentParent != null && parent != null && currentParent == parent;
    this.updateFromNodeState_(this.currentNode_.node, inForeground);
  },

  /**
   * Updates the currently highlighted node word based on the current text
   * and the character index of an event.
   * @param {string} text The current text
   * @param {number} charIndex The index of a current event in the text.
   */
  updateNodeHighlight_: function(text, charIndex) {
    if (charIndex >= text.length - 1) {
      // No need to do work if we are at the end of the paragraph.
      return;
    }
    // Get the next word based on the event's charIndex.
    let nextWordStart = getNextWordStart(text, charIndex, this.currentNode_);
    let nextWordEnd = getNextWordEnd(text, nextWordStart, this.currentNode_);
    // Map the next word into the node's index from the text.
    let nodeStart = nextWordStart - this.currentNode_.startChar;
    let nodeEnd = Math.min(
        nextWordEnd - this.currentNode_.startChar,
        this.currentNode_.node.name.length);
    if ((this.currentNodeWord_ == null ||
         nodeStart >= this.currentNodeWord_.end) &&
        nodeStart != nodeEnd) {
      // Only update the bounds if they have increased from the
      // previous node. Because tts may send multiple callbacks
      // for the end of one word and the beginning of the next,
      // checking that the current word has changed allows us to
      // reduce extra work.
      this.currentNodeWord_ = {'start': nodeStart, 'end': nodeEnd};
      this.testCurrentNode_();
    }
  }
};

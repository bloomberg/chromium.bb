// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var AutomationEvent = chrome.automation.AutomationEvent;
var AutomationNode = chrome.automation.AutomationNode;
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
 * @constructor
 */
var SelectToSpeak = function() {
  /** @private { AutomationNode } */
  this.node_ = null;

  /** @private { boolean } */
  this.trackingMouse_ = false;

  /** @private { boolean } */
  this.didTrackMouse_ = false;

  /** @private { boolean } */
  this.isSearchKeyDown_ = false;

  /** @private { !Set<number> } */
  this.keysCurrentlyDown_ = new Set();

  /** @private { !Set<number> } */
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

  /** @private { ?string } */
  this.voiceNameFromPrefs_ = null;

  /** @private { ?string } */
  this.voiceNameFromLocale_ = null;

  /** @private { Set<string> } */
  this.validVoiceNames_ = new Set();

  /** @private { number } */
  this.speechRate_ = 1.0;

  /** @const { string } */
  this.color_ = '#f73a98';

  /** @private { ?AutomationNode } */
  this.currentNode_ = null;

  /**
   * The interval ID from a call to setInterval, which is set whenever
   * speech is in progress.
   * @private { number|undefined }
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
      if (!this.findAllMatching_(root, rect, nodes) && focusedNode)
        this.findAllMatching_(focusedNode.root, rect, nodes);
      this.startSpeechQueue_(nodes);
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
      this.stopAll_();
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
    clearInterval(this.intervalId_);
    this.intervalId_ = undefined;
  },

  /**
   * Clears the focus ring, but does not clear the current
   * node.
   */
  clearFocusRing_: function() {
    chrome.accessibilityPrivate.setFocusRing([]);
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
   * Finds all nodes within the subtree rooted at |node| that overlap
   * a given rectangle.
   * @param {AutomationNode} node The starting node.
   * @param {{left: number, top: number, width: number, height: number}} rect
   *     The bounding box to search.
   * @param {Array<AutomationNode>} nodes The matching node array to be
   *     populated.
   * @return {boolean} True if any matches are found.
   */
  findAllMatching_: function(node, rect, nodes) {
    var found = false;
    for (var c = node.firstChild; c; c = c.nextSibling) {
      if (this.findAllMatching_(c, rect, nodes))
        found = true;
    }

    if (found)
      return true;

    if (!node.name || !node.location)
      return false;

    if (overlaps(node.location, rect)) {
      nodes.push(node);
      return true;
    }

    return false;
  },

  /**
   * Enqueue speech commands for all of the given nodes.
   * @param {Array<AutomationNode>} nodes The nodes to speak.
   */
  startSpeechQueue_: function(nodes) {
    chrome.tts.stop();
    if (this.intervalRef_ != undefined) {
      clearInterval(this.intervalRef_);
    }
    this.intervalRef_ = setInterval(
        this.testCurrentNode_.bind(this),
        SelectToSpeak.NODE_STATE_TEST_INTERVAL_MS);
    for (var i = 0; i < nodes.length; i++) {
      var node = nodes[i];
      var isLast = (i == nodes.length - 1);

      var options = {
        rate: this.speechRate_,
        'enqueue': true,
        onEvent:
            (function(node, isLast, event) {
              if (event.type == 'start') {
                this.currentNode_ = node;
                this.testCurrentNode_();
              } else if (
                  event.type == 'interrupted' || event.type == 'cancelled') {
                this.clearFocusRingAndNode_();
              } else if (event.type == 'end') {
                if (isLast) {
                  this.clearFocusRingAndNode_();
                }
              }
            }).bind(this, node, isLast)
      };

      // Pick the voice name from prefs first, or the one that matches
      // the locale next, but don't pick a voice that isn't currently
      // loaded. If no voices are found, leave the voiceName option
      // unset to let the browser try to route the speech request
      // anyway if possible.
      console.log('Pref: ' + this.voiceNameFromPrefs_);
      console.log('Locale: ' + this.voiceNameFromLocale_);
      var valid = '';
      this.validVoiceNames_.forEach(function(voiceName) {
        if (valid)
          valid += ',';
        valid += voiceName;
      });
      console.log('Valid: ' + valid);
      if (this.voiceNameFromPrefs_ &&
          this.validVoiceNames_.has(this.voiceNameFromPrefs_)) {
        options['voiceName'] = this.voiceNameFromPrefs_;
      } else if (
          this.voiceNameFromLocale_ &&
          this.validVoiceNames_.has(this.voiceNameFromLocale_)) {
        options['voiceName'] = this.voiceNameFromLocale_;
      }

      chrome.tts.speak(node.name || '', options);
    }
  },

  /**
   * Loads preferences from chrome.storage, sets default values if
   * necessary, and registers a listener to update prefs when they
   * change.
   */
  initPreferences_: function() {
    var updatePrefs = (function() {
                        chrome.storage.sync.get(
                            ['voice', 'rate'],
                            (function(prefs) {
                              if (prefs['voice']) {
                                this.voiceNameFromPrefs_ = prefs['voice'];
                              }
                              if (prefs['rate']) {
                                this.speechRate_ = parseFloat(prefs['rate']);
                              } else {
                                chrome.storage.sync.set(
                                    {'rate': this.speechRate_});
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
          console.log('updateDefaultVoice_ voices: ' + voices.length);
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
   * @param {AutomationNode} node The node to use for udpates
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
          chrome.accessibilityPrivate.setFocusRing(
              [node.location], this.color_);
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
    if (this.currentNode_.location === undefined) {
      // Don't do the hit test because there is no location to test against.
      // Just directly update Select To Speak from node state.
      this.updateFromNodeState_(this.currentNode_, false);
    } else {
      // Do a hit test to make sure the node is not in a background window
      // or minimimized. On the result checkCurrentNodeMatchesHitTest_ will be
      // called, and we will use that result plus the currentNode's state to
      // deterimine how to set the focus and whether to stop speech.
      this.desktop_.hitTest(
          this.currentNode_.location.left, this.currentNode_.location.top,
          EventType.HOVER);
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
    var currentParent = getNearestContainingWindow(this.currentNode_);
    var inForeground =
        currentParent != null && parent != null && currentParent == parent;
    this.updateFromNodeState_(this.currentNode_, inForeground);
  }
};

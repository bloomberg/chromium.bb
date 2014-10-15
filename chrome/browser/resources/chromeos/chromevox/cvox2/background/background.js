// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The entry point for all ChromeVox2 related code for the
 * background page.
 */

goog.provide('Background');
goog.provide('global');

goog.require('AutomationPredicate');
goog.require('AutomationUtil');
goog.require('cursors.Cursor');
goog.require('cvox.TabsApiHandler');

goog.scope(function() {
var Dir = AutomationUtil.Dir;
var EventType = chrome.automation.EventType;
var AutomationNode = chrome.automation.AutomationNode;

/** Classic Chrome accessibility API. */
global.accessibility =
    chrome.accessibilityPrivate || chrome.experimental.accessibility;

/**
 * ChromeVox2 background page.
 * @constructor
 */
Background = function() {
  /**
   * A list of site substring patterns to use with ChromeVox next. Keep these
   * strings relatively specific.
   * @type {!Array.<string>}
   * @private
   */
  this.whitelist_ = ['http://www.chromevox.com/', 'chromevox_next_test'];

  /**
   * @type {cvox.TabsApiHandler}
   * @private
   */
  this.tabsHandler_ = new cvox.TabsApiHandler(cvox.ChromeVox.tts,
                                              cvox.ChromeVox.braille,
                                              cvox.ChromeVox.earcons);

  /**
   * @type {chrome.automation.AutomationNode}
   * @private
   */
  this.currentNode_ = null;

  /**
   * Whether ChromeVox Next is active.
   * @type {boolean}
   * @private
   */
  this.active_ = false;

  // Only needed with unmerged ChromeVox classic loaded before.
  global.accessibility.setAccessibilityEnabled(false);

  // Manually bind all functions to |this|.
  for (var func in this) {
    if (typeof(this[func]) == 'function')
      this[func] = this[func].bind(this);
  }

  /**
   * Maps an automation event to its listener.
   * @type {!Object.<EventType, function(Object) : void>}
   */
  this.listeners_ = {
    focus: this.onFocus,
    loadComplete: this.onLoadComplete
  };

  // Register listeners for ...
  // Desktop.
  chrome.automation.getDesktop(this.onGotTree);

  // Tabs.
  chrome.tabs.onUpdated.addListener(this.onTabUpdated);

  // Commands.
  chrome.commands.onCommand.addListener(this.onGotCommand);
};

Background.prototype = {
  /**
   * Handles chrome.tabs.onUpdated.
   * @param {number} tabId
   * @param {Object} changeInfo
   */
  onTabUpdated: function(tabId, changeInfo) {
    if (changeInfo.status != 'complete')
      return;
    chrome.tabs.get(tabId, function(tab) {
      if (!tab.url)
        return;

      var next = this.isWhitelisted_(tab.url);
      this.toggleChromeVoxVersion({next: next, classic: !next});
    }.bind(this));
  },

  /**
   * Handles all setup once a new automation tree appears.
   * @param {chrome.automation.AutomationNode} root
   */
  onGotTree: function(root) {
    // Register all automation event listeners.
    for (var eventType in this.listeners_)
      root.addEventListener(eventType, this.listeners_[eventType], true);

    if (root.attributes.docLoaded) {
      this.onLoadComplete({target: root});
    }
  },

  /**
   * Handles chrome.commands.onCommand.
   * @param {string} command
   */
  onGotCommand: function(command) {
    if (command == 'toggleChromeVoxVersion') {
      this.toggleChromeVoxVersion();
      return;
    }

    if (!this.active_ || !this.current_)
      return;

    var previous = this.current_;
    var current = this.current_;

    var dir = Dir.FORWARD;
    var pred = null;
    switch (command) {
      case 'nextHeading':
        dir = Dir.FORWARD;
        pred = AutomationPredicate.heading;
        break;
      case 'previousHeading':
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.heading;
        break;
      case 'nextLine':
        dir = Dir.FORWARD;
        pred = AutomationPredicate.inlineTextBox;
        break;
      case 'previousLine':
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.inlineTextBox;
        break;
      case 'nextLink':
        dir = Dir.FORWARD;
        pred = AutomationPredicate.link;
        break;
      case 'previousLink':
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.link;
        break;
      case 'nextElement':
        current = current.role == chrome.automation.RoleType.inlineTextBox ?
            current.parent() : current;
        current = AutomationUtil.findNextNode(current,
            Dir.FORWARD,
            AutomationPredicate.inlineTextBox);
        current = current ? current.parent() : current;
        break;
      case 'previousElement':
        current = current.role == chrome.automation.RoleType.inlineTextBox ?
            current.parent() : current;
        current = AutomationUtil.findNextNode(current,
            Dir.BACKWARD,
            AutomationPredicate.inlineTextBox);
        current = current ? current.parent() : current;
        break;
      case 'goToBeginning':
        current = AutomationUtil.findNodePost(current.root,
            Dir.FORWARD,
            AutomationPredicate.inlineTextBox);
        break;
      case 'goToEnd':
        current = AutomationUtil.findNodePost(current.root,
            Dir.BACKWARD,
            AutomationPredicate.inlineTextBox);
        break;
    }

    if (pred)
      current = AutomationUtil.findNextNode(current, dir, pred);

    if (current) {
      current.focus();

      this.onFocus({target: current});
    }
  },

  /**
   * Provides all feedback once ChromeVox's focus changes.
   * @param {Object} evt
   */
  onFocus: function(evt) {
    var node = evt.target;
    if (!node)
      return;

    this.current_ = node;
    var container = node;
    while (container &&
        (container.role == chrome.automation.RoleType.inlineTextBox ||
        container.role == chrome.automation.RoleType.staticText))
      container = container.parent();

    var role = container ? container.role : node.role;

    var output =
        [node.attributes.name, node.attributes.value, role].join(', ');
    cvox.ChromeVox.tts.speak(output, cvox.QueueMode.FLUSH);
    cvox.ChromeVox.braille.write(cvox.NavBraille.fromText(output));
    chrome.accessibilityPrivate.setFocusRing([evt.target.location]);
  },

  /**
   * Provides all feedback once a load complete event fires.
   * @param {Object} evt
   */
  onLoadComplete: function(evt) {
    if (this.current_)
      return;

    this.current_ = AutomationUtil.findNodePost(evt.target,
        Dir.FORWARD,
        AutomationPredicate.inlineTextBox);
    this.onFocus({target: this.current_});
  },

  /**
   * @private
   * @param {string} url
   * @return {boolean} Whether the given |url| is whitelisted.
   */
  isWhitelisted_: function(url) {
    return this.whitelist_.some(function(item) {
      return url.indexOf(item) != -1;
    }.bind(this));
  },

  /**
   * Disables classic ChromeVox.
   * @param {number} tabId The tab where ChromeVox classic is running in.
   */
  disableClassicChromeVox_: function(tabId) {
    chrome.tabs.executeScript(
          tabId,
          {'code': 'try { window.disableChromeVox(); } catch(e) { }\n',
           'allFrames': true});
  },

  /**
   * Toggles between ChromeVox Next and Classic.
   * @param {{classic: boolean, next: boolean}=} opt_options Forceably set.
  */
  toggleChromeVoxVersion: function(opt_options) {
    if (!opt_options) {
      opt_options = {};
      opt_options.next = !this.active_;
      opt_options.classic = !opt_options.next;
    }

    if (opt_options.next) {
      chrome.automation.getTree(this.onGotTree);
      this.active_ = true;
    } else {
      if (this.active_) {
        for (var eventType in this.listeners_) {
          this.current_.root.removeEventListener(
              eventType, this.listeners_[eventType], true);
        }
      }
      this.active_ = false;
    }

    chrome.tabs.query({active: true}, function(tabs) {
      if (opt_options.classic) {
        cvox.ChromeVox.injectChromeVoxIntoTabs(tabs, true);
      } else {
        tabs.forEach(function(tab) {
          this.disableClassicChromeVox_(tab.id);
        }.bind(this));
      }
    }.bind(this));
  }
};

/** @type {Background} */
global.backgroundObj = new Background();

});  // goog.scope

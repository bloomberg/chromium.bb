// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The entry point for all ChromeVox2 related code for the
 * background page.
 *
 */

goog.provide('cvox2.Background');
goog.provide('cvox2.global');

goog.require('cvox.TabsApiHandler');
goog.require('cvox2.AutomationPredicates');
goog.require('cvox2.AutomationUtil');
goog.require('cvox2.Dir');

/** Classic Chrome accessibility API. */
cvox2.global.accessibility =
    chrome.accessibilityPrivate || chrome.experimental.accessibility;

/**
 * ChromeVox2 background page.
 * @constructor
 */
cvox2.Background = function() {
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

  // Only needed with unmerged ChromeVox classic loaded before.
  cvox2.global.accessibility.setAccessibilityEnabled(false);

  // Manually bind all functions to |this|.
  for (var func in this) {
    if (typeof(this[func]) == 'function')
      this[func] = this[func].bind(this);
  }

  // Register listeners for ...
  // Desktop.
  chrome.automation.getDesktop(this.onGotTree);

  // Tabs.
  chrome.tabs.onUpdated.addListener(this.onTabUpdated);
};

cvox2.Background.prototype = {
  /**
   * Handles chrome.tabs.onUpdated.
   * @param {number} tabId
   * @param {Object} changeInfo
   */
  onTabUpdated: function(tabId, changeInfo) {
    chrome.tabs.get(tabId, function(tab) {
      if (!tab.url)
        return;

      if (!this.isWhitelisted_(tab.url)) {
        chrome.commands.onCommand.removeListener(this.onGotCommand);
        cvox.ChromeVox.injectChromeVoxIntoTabs([tab], true);
        return;
      }

      if (!chrome.commands.onCommand.hasListeners())
        chrome.commands.onCommand.addListener(this.onGotCommand);

      this.disableClassicChromeVox_(tab.id);

      chrome.automation.getTree(this.onGotTree.bind(this));
    }.bind(this));
  },

  /**
   * Handles all setup once a new automation tree appears.
   * @param {chrome.automation.AutomationNode} root
   */
  onGotTree: function(root) {
    // Register all automation event listeners.
    root.addEventListener('focus',
                          this.onFocus,
                          true);
    root.addEventListener('loadComplete',
                          this.onLoadComplete,
                          true);

    if (root.attributes.docLoaded)
      this.onLoadComplete({target: root});
  },

  /**
   * Handles chrome.commands.onCommand.
   * @param {string} command
   */
  onGotCommand: function(command) {
    if (!this.current_)
      return;

    var previous = this.current_;
    var current = this.current_;

    var dir = cvox2.Dir.FORWARD;
    var pred = null;
    switch (command) {
      case 'nextHeading':
        dir = cvox2.Dir.FORWARD;
        pred = cvox2.AutomationPredicates.heading;
        break;
      case 'previousHeading':
        dir = cvox2.Dir.BACKWARD;
        pred = cvox2.AutomationPredicates.heading;
        break;
      case 'nextLine':
        dir = cvox2.Dir.FORWARD;
        pred = cvox2.AutomationPredicates.inlineTextBox;
        break;
      case 'previousLine':
        dir = cvox2.Dir.BACKWARD;
        pred = cvox2.AutomationPredicates.inlineTextBox;
        break;
      case 'nextLink':
        dir = cvox2.Dir.FORWARD;
        pred = cvox2.AutomationPredicates.link;
        break;
      case 'previousLink':
        dir = cvox2.Dir.BACKWARD;
        pred = cvox2.AutomationPredicates.link;
        break;
      case 'nextElement':
        current = current.role == 'inlineTextBox' ?
            current.parent() : current;
        current = cvox2.AutomationUtil.findNextNode(current,
            cvox2.Dir.FORWARD,
            cvox2.AutomationPredicates.inlineTextBox);
        current = current ? current.parent() : current;
        break;
      case 'previousElement':
        current = current.role == 'inlineTextBox' ?
            current.parent() : current;
        current = cvox2.AutomationUtil.findNextNode(current,
            cvox2.Dir.BACKWARD,
            cvox2.AutomationPredicates.inlineTextBox);
        current = current ? current.parent() : current;
        break;
    }

    if (pred)
      current = cvox2.AutomationUtil.findNextNode(current, dir, pred);

    if (current)
      current.focus();

    this.onFocus({target: current || previous});
  },

  /**
   * Provides all feedback once ChromeVox's focus changes.
   * @param {Object} evt
   */
  onFocus: function(evt) {
    var node = evt.target;
    if (!node)
      return;
    var container = node;
    while (container && (container.role == 'inlineTextBox' ||
        container.role == 'staticText'))
      container = container.parent();

    var role = container ? container.role : node.role;

    var output =
        [node.attributes.name, node.attributes.value, role].join(', ');
    cvox.ChromeVox.tts.speak(output, cvox.QueueMode.FLUSH);
    cvox.ChromeVox.braille.write(cvox.NavBraille.fromText(output));
    chrome.accessibilityPrivate.setFocusRing([evt.target.location]);

    this.current_ = node;
  },

  /**
   * Provides all feedback once a load complete event fires.
   * @param {Object} evt
   */
  onLoadComplete: function(evt) {
    this.current_ = cvox2.AutomationUtil.findNodePost(evt.target,
        cvox2.Dir.FORWARD,
        cvox2.AutomationPredicates.inlineTextBox);
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
  }
};

/** @type {cvox2.Background} */
cvox2.global.backgroundObj = new cvox2.Background();

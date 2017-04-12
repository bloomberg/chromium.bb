// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to manage SwitchAccess and interact with other controllers.
 *
 * @constructor
 * @implements {SwitchAccessInterface}
 */
function SwitchAccess() {
  console.log('Switch access is enabled');

  /**
   * User preferences.
   *
   * @type {SwitchAccessPrefs}
   */
  this.switchAccessPrefs = null;

  /**
   * Handles changes to auto-scan.
   *
   * @private {AutoScanManager}
   */
  this.autoScanManager_ = null;

  /**
   * Handles keyboard input.
   *
   * @private {KeyboardHandler}
   */
  this.keyboardHandler_ = null;

  /**
   * Moves to the appropriate node in the accessibility tree.
   *
   * @private {AutomationTreeWalker}
   */
  this.treeWalker_ = null;

  /**
   * Currently selected node.
   *
   * @private {chrome.automation.AutomationNode}
   */
  this.node_ = null;

  /**
   * Root node (i.e., the desktop).
   *
   * @private {chrome.automation.AutomationNode}
   */
  this.root_ = null;

  this.init_();
};

SwitchAccess.prototype = {
  /**
   * Set this.node_ and this.root_ to the desktop node, and set up preferences,
   * controllers, and event listeners.
   *
   * @private
   */
  init_: function() {
    this.switchAccessPrefs = new SwitchAccessPrefs();
    this.autoScanManager_ = new AutoScanManager(this);
    this.keyboardHandler_ = new KeyboardHandler(this);
    this.treeWalker_ = new AutomationTreeWalker();

    chrome.automation.getDesktop(function(desktop) {
      this.node_ = desktop;
      this.root_ = desktop;
      console.log('AutomationNode for desktop is loaded');
      this.printNode_(this.node_);
    }.bind(this));

    document.addEventListener(
        'prefsUpdate', this.handlePrefsUpdate_.bind(this));
  },

  /**
   * Set this.node_ to the next/previous interesting node. If no interesting
   * node is found, set this.node_ to the first/last interesting node. If
   * |doNext| is true, will search for next node. Otherwise, will search for
   * previous node.
   *
   * @param {boolean} doNext
   * @override
   */
  moveToNode: function(doNext) {
    let node = this.treeWalker_.moveToNode(this.node_, this.root_, doNext);
    if (node) {
      this.node_ = node;
      this.printNode_(this.node_);
      chrome.accessibilityPrivate.setFocusRing([this.node_.location]);
    }
  },

  /**
   * Perform the default action on the currently selected node.
   *
   * @override
   */
  doDefault: function() {
    if (!this.node_)
      return;

    this.node_.doDefault();
  },

  /**
   * Open the options page in a new tab.
   *
   * @override
   */
  showOptionsPage: function() {
    let optionsPage = {url: 'options.html'};
    chrome.tabs.create(optionsPage);
  },

  /**
   * Perform actions as the result of actions by the user. Currently, restarts
   * auto-scan if it is enabled.
   *
   * @override
   */
  performedUserAction: function() {
    this.autoScanManager_.restartIfRunning();
  },

  /**
   * Handle a change in user preferences.
   *
   * @param {!Event} event
   * @private
   */
  handlePrefsUpdate_: function(event) {
    let updatedPrefs = event.detail;
    for (let key of Object.keys(updatedPrefs)) {
      switch (key) {
        case 'enableAutoScan':
          this.autoScanManager_.setEnabled(updatedPrefs[key]);
          break;
        case 'autoScanTime':
          this.autoScanManager_.setScanTime(updatedPrefs[key]);
          break;
      }
    }
  },

  // TODO(elichtenberg): Move print functions to a custom logger class. Only
  // log when debuggingEnabled is true.
  /**
   * Print out details about a node.
   *
   * @param {chrome.automation.AutomationNode} node
   * @private
   */
  printNode_: function(node) {
    if (node) {
      console.log('Name = ' + node.name);
      console.log('Role = ' + node.role);
      console.log('Root role = ' + node.root.role);
      if (!node.parent)
        console.log('At index ' + node.indexInParent + ', has no parent');
      else {
        let numSiblings = node.parent.children.length;
        console.log(
            'At index ' + node.indexInParent + ', there are '
            + numSiblings + ' siblings');
      }
      console.log('Has ' + node.children.length + ' children');
    } else {
      console.log('Node is null');
    }
    console.log(node);
    console.log('\n');
  },

  /**
   * Move to the next sibling of this.node_ if it has one.
   *
   * @override
   */
  debugMoveToNext: function() {
    let next = this.treeWalker_.debugMoveToNext(this.node_);
    if (next) {
      this.node_ = next;
      this.printNode_(this.node_);
      chrome.accessibilityPrivate.setFocusRing([this.node_.location]);
    }
  },

  /**
   * Move to the previous sibling of this.node_ if it has one.
   *
   * @override
   */
  debugMoveToPrevious: function() {
    let prev = this.treeWalker_.debugMoveToPrevious(this.node_);
    if (prev) {
      this.node_ = prev;
      this.printNode_(this.node_);
      chrome.accessibilityPrivate.setFocusRing([this.node_.location]);
    }
  },

  /**
   * Move to the first child of this.node_ if it has one.
   *
   * @override
   */
  debugMoveToFirstChild: function() {
    let child = this.treeWalker_.debugMoveToFirstChild(this.node_);
    if (child) {
      this.node_ = child;
      this.printNode_(this.node_);
      chrome.accessibilityPrivate.setFocusRing([this.node_.location]);
    }
  },

  /**
   * Move to the parent of this.node_ if it has one.
   *
   * @override
   */
  debugMoveToParent: function() {
    let parent = this.treeWalker_.debugMoveToParent(this.node_);
    if (parent) {
      this.node_ = parent;
      this.printNode_(this.node_);
      chrome.accessibilityPrivate.setFocusRing([this.node_.location]);
    }
  }
};

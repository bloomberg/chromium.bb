// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var AutomationNode = chrome.automation.AutomationNode;

/**
 * @constructor
 */
var SwitchAccess = function() {
  console.log("Switch access is enabled");

  // Currently selected node.
  /** @private {AutomationNode} */
  this.node_ = null;

  // List of nodes to push to / pop from in case this.node_ is lost.
  /** @private {!Array<!AutomationNode>} */
  this.ancestorList_ = [];

  chrome.automation.getDesktop(function(desktop) {
    this.node_ = desktop;
    console.log("AutomationNode for desktop is loaded");
    this.printDetails_();

    document.addEventListener("keyup", function(event) {
      if (event.key === "1") {
        console.log("1 = go to previous element");
        this.moveToPrevious_();
      } else if (event.key === "2") {
        console.log("2 = go to next element");
        this.moveToNext_();
      } else if (event.key === "3") {
        console.log("3 = go to child element");
        this.moveToFirstChild_();
      } else if (event.key === "4") {
        console.log("4 = go to parent element");
        this.moveToParent_();
      } else if (event.key === "5") {
        console.log("5 is not yet implemented");
        console.log("\n");
      }
      chrome.accessibilityPrivate.setFocusRing([this.node_.location]);
    }.bind(this));
  }.bind(this));
};

SwitchAccess.prototype = {
  /**
   * Move to the previous sibling of this.node_ if it has one.
   */
  moveToPrevious_: function() {
    var previous = this.node_.previousSibling;
    if (previous) {
      this.node_ = previous;
      this.printDetails_();
    } else {
      console.log("Node is first of siblings");
      console.log("\n");
    }
  },

  /**
   * Move to the next sibling of this.node_ if it has one.
   */
  moveToNext_: function() {
    var next = this.node_.nextSibling;
    if (next) {
      this.node_ = next;
      this.printDetails_();
    } else {
      console.log("Node is last of siblings");
      console.log("\n");
    }
  },

  /**
   * Move to the first child of this.node_ if it has one.
   */
  moveToFirstChild_: function() {
    var child = this.node_.firstChild;
    if (child) {
      this.ancestorList_.push(this.node_);
      this.node_ = child;
      this.printDetails_();
    } else {
      console.log("Node has no children");
      console.log("\n");
    }
  },

  /**
   * Move to the parent of this.node_ if it has one. If it does not have a
   * parent but it is not the top level root node, then this.node_ lost track of
   * its neighbors, and we move to an ancestor node.
   */
  moveToParent_: function() {
    var parent = this.node_.parent;
    if (parent) {
      this.ancestorList_.pop();
      this.node_ = parent;
      this.printDetails_();
    } else if (this.ancestorList_.length === 0) {
      console.log("Node has no parent");
      console.log("\n");
    } else {
      console.log(
          "Node could not find its parent, so moved to recent ancestor");
      var ancestor = this.ancestorList_.pop();
      this.node_ = ancestor;
      this.printDetails_();
    }
  },

  /**
   * Print out details about the currently selected node and the list of
   * ancestors.
   *
   * @private
   */
  printDetails_: function() {
    this.printNode_(this.node_);
    console.log(this.ancestorList_);
    console.log("\n");
  },

  /**
   * Print out details about a node.
   *
   * @param {AutomationNode} node
   * @private
   */
  printNode_: function(node) {
    if (node) {
      console.log("Name = " + node.name);
      console.log("Role = " + node.role);
      if (!node.parent) {
        console.log("At index " + node.indexInParent + ", has no parent");
      } else {
        var numSiblings = node.parent.children.length;
        console.log(
            "At index " + node.indexInParent + ", there are "
            + numSiblings + " siblings");
      }
      console.log("Has " + node.children.length + " children");
    } else {
      console.log("Node is null");
    }
    console.log(node);
  }
};

new SwitchAccess();

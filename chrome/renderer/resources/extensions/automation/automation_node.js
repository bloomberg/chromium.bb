// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var automationInternal =
    require('binding').Binding.create('automationInternal').generate();
var utils = require('utils');

/**
 * A single node in the Automation tree.
 * @param {AutomationTree} owner The owning tree.
 * @constructor
 */
var AutomationNodeImpl = function(owner) {
  this.owner = owner;
  this.childIDs = [];
  this.attributes = {};
  this.listeners = {};
};

AutomationNodeImpl.prototype = {
  parent: function() {
    return this.owner.get(this.parentID);
  },

  firstChild: function() {
    var node = this.owner.get(this.childIDs[0]);
    return node;
  },

  lastChild: function() {
    var childIDs = this.childIDs;
    var node = this.owner.get(childIDs[childIDs.length - 1]);
    return node;
  },

  children: function() {
    var children = [];
    for (var i = 0, childID; childID = this.childIDs[i]; i++)
      children.push(this.owner.get(childID));
    return children;
  },

  previousSibling: function() {
    var parent = this.parent();
    if (parent && this.indexInParent > 0)
      return parent.children()[this.indexInParent - 1];
    return undefined;
  },

  nextSibling: function() {
    var parent = this.parent();
    if (parent && this.indexInParent < parent.children().length)
      return parent.children()[this.indexInParent + 1];
    return undefined;
  },

  doDefault: function() {
    this.performAction_('doDefault');
  },

  focus: function() {
    this.performAction_('focus');
  },

  makeVisible: function() {
    this.performAction_('makeVisible');
  },

  setSelection: function(startIndex, endIndex) {
    this.performAction_('setSelection',
                        {startIndex: startIndex, endIndex: endIndex});
  },

  addEventListener: function(eventType, callback, capture) {
    this.removeEventListener(eventType, callback);
    if (!this.listeners[eventType])
      this.listeners[eventType] = [];
    this.listeners[eventType].push([callback, capture]);
  },

  // TODO(dtseng/aboxhall): Check this impl against spec.
  removeEventListener: function(eventType, callback) {
    if (this.listeners[eventType]) {
      var listeners = this.listeners[eventType];
      for (var i = 0; i < listeners.length; i++) {
        if (callback === listeners[i][0])
          listeners.splice(i, 1);
      }
    }
  },

  notifyEventListeners: function(eventType) {
    var listeners = this.listeners[eventType];
    if (!listeners)
      return;
    // TODO(dtseng/aboxhall): Implement capture/bubble.
    for (var i = 0; i < listeners.length; i++)
      listeners[i][0]();
  },

  performAction_: function(actionType, opt_args) {
    // Not yet initialized.
    if (!this.owner.processID ||
        !this.owner.routingID ||
        !this.wrapper.id)
      return;
    automationInternal.performAction({processID: this.owner.processID,
                                      routingID: this.owner.routingID,
                                      automationNodeID: this.wrapper.id,
                                      actionType: actionType},
                                     opt_args || {});
  },
};

var AutomationNode = utils.expose('AutomationNode',
                                  AutomationNodeImpl,
                                  ['parent',
                                   'firstChild',
                                   'lastChild',
                                   'children',
                                   'previousSibling',
                                   'nextSibling',
                                   'doDefault',
                                   'focus',
                                   'makeVisible',
                                   'setSelection',
                                   'addEventListener',
                                   'removeEventListener']);
exports.AutomationNode = AutomationNode;

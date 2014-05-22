// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var AutomationEvent = require('automationEvent').AutomationEvent;
var automationInternal =
    require('binding').Binding.create('automationInternal').generate();
var utils = require('utils');
var IsInteractPermitted =
    requireNative('automationInternal').IsInteractPermitted;

/**
 * A single node in the Automation tree.
 * @param {AutomationTree} owner The owning tree.
 * @constructor
 */
var AutomationNodeImpl = function(owner) {
  this.owner = owner;
  this.childIds = [];
  this.attributes = {};
  this.listeners = {};
  this.location = { left: 0, top: 0, width: 0, height: 0 };
};

AutomationNodeImpl.prototype = {
  id: -1,
  role: '',
  loaded: false,
  state: { busy: true },

  parent: function() {
    return this.owner.get(this.parentID);
  },

  firstChild: function() {
    var node = this.owner.get(this.childIds[0]);
    return node;
  },

  lastChild: function() {
    var childIds = this.childIds;
    var node = this.owner.get(childIds[childIds.length - 1]);
    return node;
  },

  children: function() {
    var children = [];
    for (var i = 0, childID; childID = this.childIds[i]; i++)
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

  focus: function(opt_callback) {
    this.performAction_('focus');
  },

  makeVisible: function(opt_callback) {
    this.performAction_('makeVisible');
  },

  setSelection: function(startIndex, endIndex, opt_callback) {
    this.performAction_('setSelection',
                        { startIndex: startIndex,
                          endIndex: endIndex });
  },

  addEventListener: function(eventType, callback, capture) {
    this.removeEventListener(eventType, callback);
    if (!this.listeners[eventType])
      this.listeners[eventType] = [];
    this.listeners[eventType].push({callback: callback, capture: capture});
  },

  // TODO(dtseng/aboxhall): Check this impl against spec.
  removeEventListener: function(eventType, callback) {
    if (this.listeners[eventType]) {
      var listeners = this.listeners[eventType];
      for (var i = 0; i < listeners.length; i++) {
        if (callback === listeners[i].callback)
          listeners.splice(i, 1);
      }
    }
  },

  dispatchEvent: function(eventType) {
    var path = [];
    var parent = this.parent();
    while (parent) {
      path.push(parent);
      // TODO(aboxhall/dtseng): handle unloaded parent node
      parent = parent.parent();
    }
    path.push(this.owner.wrapper);
    var event = new AutomationEvent(eventType, this.wrapper);

    // Dispatch the event through the propagation path in three phases:
    // - capturing: starting from the root and going down to the target's parent
    // - targeting: dispatching the event on the target itself
    // - bubbling: starting from the target's parent, going back up to the root.
    // At any stage, a listener may call stopPropagation() on the event, which
    // will immediately stop event propagation through this path.
    if (this.dispatchEventAtCapturing_(event, path)) {
      if (this.dispatchEventAtTargeting_(event, path))
        this.dispatchEventAtBubbling_(event, path);
    }
  },

  toString: function() {
    return 'node id=' + this.id +
        ' role=' + this.role +
        ' state=' + JSON.stringify(this.state) +
        ' childIds=' + JSON.stringify(this.childIds) +
        ' attributes=' + JSON.stringify(this.attributes);
  },

  dispatchEventAtCapturing_: function(event, path) {
    privates(event).impl.eventPhase = Event.CAPTURING_PHASE;
    for (var i = path.length - 1; i >= 0; i--) {
      this.fireEventListeners_(path[i], event);
      if (privates(event).impl.propagationStopped)
        return false;
    }
    return true;
  },

  dispatchEventAtTargeting_: function(event) {
    privates(event).impl.eventPhase = Event.AT_TARGET;
    this.fireEventListeners_(this.wrapper, event);
    return !privates(event).impl.propagationStopped;
  },

  dispatchEventAtBubbling_: function(event, path) {
    privates(event).impl.eventPhase = Event.BUBBLING_PHASE;
    for (var i = 0; i < path.length; i++) {
      this.fireEventListeners_(path[i], event);
      if (privates(event).impl.propagationStopped)
        return false;
    }
    return true;
  },

  fireEventListeners_: function(node, event) {
    var nodeImpl = privates(node).impl;
    var listeners = nodeImpl.listeners[event.type];
    if (!listeners)
      return;

    var eventPhase = event.eventPhase;
    for (var i = 0; i < listeners.length; i++) {
      if (eventPhase == Event.CAPTURING_PHASE && !listeners[i].capture)
        continue;
      if (eventPhase == Event.BUBBLING_PHASE && listeners[i].capture)
        continue;

      try {
        listeners[i].callback(event);
      } catch (e) {
        console.error('Error in event handler for ' + event.type +
                      'during phase ' + eventPhase + ': ' +
                      e.message + '\nStack trace: ' + e.stack);
      }
    }
  },

  performAction_: function(actionType, opt_args) {
    // Not yet initialized.
    if (this.owner.processID === undefined ||
        this.owner.routingID === undefined ||
        this.wrapper.id === undefined) {
      return;
    }

    // Check permissions.
    if (!IsInteractPermitted()) {
      throw new Error(actionType + ' requires {"desktop": true} or' +
          ' {"interact": true} in the "automation" manifest key.');
    }

    automationInternal.performAction({ processID: this.owner.processID,
                                       routingID: this.owner.routingID,
                                       automationNodeID: this.wrapper.id,
                                       actionType: actionType },
                                     opt_args || {});
  }
};

var AutomationNode = utils.expose('AutomationNode',
                                  AutomationNodeImpl,
                                  { functions: ['parent',
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
                                                'removeEventListener'],
                                    readonly: ['id',
                                               'role',
                                               'state',
                                               'location',
                                               'attributes',
                                               'loaded'] });

exports.AutomationNode = AutomationNode;

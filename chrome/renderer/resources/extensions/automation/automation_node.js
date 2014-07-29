// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var AutomationEvent = require('automationEvent').AutomationEvent;
var automationInternal =
    require('binding').Binding.create('automationInternal').generate();
var IsInteractPermitted =
    requireNative('automationInternal').IsInteractPermitted;

var lastError = require('lastError');
var logging = requireNative('logging');
var schema = requireNative('automationInternal').GetSchemaAdditions();
var utils = require('utils');

/**
 * A single node in the Automation tree.
 * @param {AutomationRootNodeImpl} root The root of the tree.
 * @constructor
 */
function AutomationNodeImpl(root) {
  this.rootImpl = root;
  this.childIds = [];
  this.attributes = {};
  this.listeners = {};
  this.location = { left: 0, top: 0, width: 0, height: 0 };
}

AutomationNodeImpl.prototype = {
  id: -1,
  role: '',
  state: { busy: true },
  isRootNode: false,

  get root() {
    return this.rootImpl.wrapper;
  },

  parent: function() {
    return this.rootImpl.get(this.parentID);
  },

  firstChild: function() {
    var node = this.rootImpl.get(this.childIds[0]);
    return node;
  },

  lastChild: function() {
    var childIds = this.childIds;
    var node = this.rootImpl.get(childIds[childIds.length - 1]);
    return node;
  },

  children: function() {
    var children = [];
    for (var i = 0, childID; childID = this.childIds[i]; i++) {
      logging.CHECK(this.rootImpl.get(childID));
      children.push(this.rootImpl.get(childID));
    }
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
                        { startIndex: startIndex,
                          endIndex: endIndex });
  },

  addEventListener: function(eventType, callback, capture) {
    this.removeEventListener(eventType, callback);
    if (!this.listeners[eventType])
      this.listeners[eventType] = [];
    this.listeners[eventType].push({callback: callback, capture: !!capture});
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
        ' parentID=' + this.parentID +
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
    if (this.rootImpl.processID === undefined ||
        this.rootImpl.routingID === undefined ||
        this.wrapper.id === undefined) {
      return;
    }

    // Check permissions.
    if (!IsInteractPermitted()) {
      throw new Error(actionType + ' requires {"desktop": true} or' +
          ' {"interact": true} in the "automation" manifest key.');
    }

    automationInternal.performAction({ processID: this.rootImpl.processID,
                                       routingID: this.rootImpl.routingID,
                                       automationNodeID: this.wrapper.id,
                                       actionType: actionType },
                                     opt_args || {});
  }
};

// Maps an attribute to its default value in an invalidated node.
// These attributes are taken directly from the Automation idl.
var AutomationAttributeDefaults = {
  'id': -1,
  'role': '',
  'state': {},
  'location': { left: 0, top: 0, width: 0, height: 0 }
};


var AutomationAttributeTypes = [
  'boolAttributes',
  'floatAttributes',
  'htmlAttributes',
  'intAttributes',
  'intlistAttributes',
  'stringAttributes'
];


/**
 * AutomationRootNode.
 *
 * An AutomationRootNode is the javascript end of an AXTree living in the
 * browser. AutomationRootNode handles unserializing incremental updates from
 * the source AXTree. Each update contains node data that form a complete tree
 * after applying the update.
 *
 * A brief note about ids used through this class. The source AXTree assigns
 * unique ids per node and we use these ids to build a hash to the actual
 * AutomationNode object.
 * Thus, tree traversals amount to a lookup in our hash.
 *
 * The tree itself is identified by the process id and routing id of the
 * renderer widget host.
 * @constructor
 */
function AutomationRootNodeImpl(processID, routingID) {
  AutomationNodeImpl.call(this, this);
  this.processID = processID;
  this.routingID = routingID;
  this.axNodeDataCache_ = {};
}

AutomationRootNodeImpl.prototype = {
  __proto__: AutomationNodeImpl.prototype,

  isRootNode: true,

  get: function(id) {
    if (id == undefined)
      return undefined;

    return this.axNodeDataCache_[id];
  },

  unserialize: function(update) {
    console.log('got update:\n' + JSON.stringify(update));
    var updateState = { pendingNodes: {}, newNodes: {} };
    var oldRootId = this.id;

    if (update.nodeIdToClear < 0) {
        logging.WARNING('Bad nodeIdToClear: ' + update.nodeIdToClear);
        lastError.set('automation',
                      'Bad update received on automation tree',
                      null,
                      chrome);
        return false;
    } else if (update.nodeIdToClear > 0) {
      var nodeToClear = this.axNodeDataCache_[update.nodeIdToClear];
      if (!nodeToClear) {
        logging.WARNING('Bad nodeIdToClear: ' + update.nodeIdToClear +
                        ' (not in cache)');
        lastError.set('automation',
                      'Bad update received on automation tree',
                      null,
                      chrome);
        return false;
      }
      if (nodeToClear === this.wrapper) {
        this.invalidate_(nodeToClear);
      } else {
        var children = nodeToClear.children();
        for (var i = 0; i < children.length; i++)
          this.invalidate_(children[i]);
        privates(nodeToClear).impl.childIds = []
        updateState.pendingNodes[nodeToClear.id] = nodeToClear;
      }
    }

    for (var i = 0; i < update.nodes.length; i++) {
      if (!this.updateNode_(update.nodes[i], updateState))
        return false;
    }

    if (Object.keys(updateState.pendingNodes).length > 0) {
      logging.WARNING('Nodes left pending by the update: ' +
                   updateState.pendingNodes.join(', '));
      lastError.set('automation',
                    'Bad update received on automation tree',
                    null,
                    chrome);
      return false;
    }
    console.log('after update:\n' + this.toString());
    return true;
  },

  destroy: function() {
    this.dispatchEvent(schema.EventType.destroyed);
    this.invalidate_(this.wrapper);
  },

  onAccessibilityEvent: function(eventParams) {
    if (!this.unserialize(eventParams.update)) {
      logging.WARNING('unserialization failed');
      return false;
    }

    var targetNode = this.get(eventParams.targetID);
    if (targetNode) {
      var targetNodeImpl = privates(targetNode).impl;
      targetNodeImpl.dispatchEvent(eventParams.eventType);
    } else {
      logging.WARNING('Got ' + eventParams.eventType +
                      ' event on unknown node: ' + eventParams.targetID +
                      '; this: ' + this.toString());
    }
    return true;
  },

  toString: function() {
    function toStringInternal(node, indent) {
      if (!node)
        return '';
      var output =
          new Array(indent).join(' ') +
          AutomationNodeImpl.prototype.toString.call(node) +
          '\n';
      indent += 2;
      for (var i = 0; i < node.children().length; i++)
        output += toStringInternal(node.children()[i], indent);
      return output;
    }
    return toStringInternal(this, 0);
  },

  invalidate_: function(node) {
    if (!node)
      return;
    var children = node.children();

    for (var i = 0, child; child = children[i]; i++)
      this.invalidate_(child);

    // Retrieve the internal AutomationNodeImpl instance for this node.
    // This object is not accessible outside of bindings code, but we can access
    // it here.
    var nodeImpl = privates(node).impl;
    var id = node.id;
    for (var key in AutomationAttributeDefaults) {
      nodeImpl[key] = AutomationAttributeDefaults[key];
    }
    nodeImpl.childIds = [];
    nodeImpl.loaded = false;
    nodeImpl.id = id;
    delete this.axNodeDataCache_[id];
  },

  load: function(callback) {
    // TODO(dtseng/aboxhall): Implement.
    if (!this.loaded)
      throw 'Unsupported state: root node is not loaded.';

    setTimeout(callback, 0);
  },

  deleteOldChildren_: function(node, newChildIds) {
    // Create a set of child ids in |src| for fast lookup, and return false
    // if a duplicate is found;
    var newChildIdSet = {};
    for (var i = 0; i < newChildIds.length; i++) {
      var childId = newChildIds[i];
      if (newChildIdSet[childId]) {
        logging.WARNING('Node ' + node.id + ' has duplicate child id ' +
                        childId);
        lastError.set('automation',
                      'Bad update received on automation tree',
                      null,
                      chrome);
        return false;
      }
      newChildIdSet[newChildIds[i]] = true;
    }

    // Delete the old children.
    var nodeImpl = privates(node).impl;
    var oldChildIds = nodeImpl.childIds;
    for (var i = 0; i < oldChildIds.length;) {
      var oldId = oldChildIds[i];
      if (!newChildIdSet[oldId]) {
        this.invalidate_(this.axNodeDataCache_[oldId]);
        oldChildIds.splice(i, 1);
      } else {
        i++;
      }
    }
    nodeImpl.childIds = oldChildIds;

    return true;
  },

  createNewChildren_: function(node, newChildIds, updateState) {
    logging.CHECK(node);
    var success = true;
    for (var i = 0; i < newChildIds.length; i++) {
      var childId = newChildIds[i];
      var childNode = this.axNodeDataCache_[childId];
      if (childNode) {
        if (childNode.parent() != node) {
          var parentId = 0;
          if (childNode.parent()) parentId = childNode.parent().id;
          // This is a serious error - nodes should never be reparented.
          // If this case occurs, continue so this node isn't left in an
          // inconsistent state, but return failure at the end.
          logging.WARNING('Node ' + childId + ' reparented from ' +
                          parentId + ' to ' + node.id);
          lastError.set('automation',
                        'Bad update received on automation tree',
                        null,
                        chrome);
          success = false;
          continue;
        }
      } else {
        childNode = new AutomationNode(this);
        this.axNodeDataCache_[childId] = childNode;
        privates(childNode).impl.id = childId;
        updateState.pendingNodes[childNode.id] = childNode;
        updateState.newNodes[childNode.id] = childNode;
      }
      privates(childNode).impl.indexInParent = i;
      privates(childNode).impl.parentID = node.id;
    }

    return success;
  },

  setData_: function(node, nodeData) {
    var nodeImpl = privates(node).impl;
    for (var key in AutomationAttributeDefaults) {
      if (key in nodeData)
        nodeImpl[key] = nodeData[key];
      else
        nodeImpl[key] = AutomationAttributeDefaults[key];
    }
    for (var i = 0; i < AutomationAttributeTypes.length; i++) {
      var attributeType = AutomationAttributeTypes[i];
      for (var attributeName in nodeData[attributeType]) {
        nodeImpl.attributes[attributeName] =
          nodeData[attributeType][attributeName];
      }
    }
  },

  updateNode_: function(nodeData, updateState) {
    var node = this.axNodeDataCache_[nodeData.id];
    var didUpdateRoot = false;
    if (node) {
      delete updateState.pendingNodes[node.id];
    } else {
      if (nodeData.role != schema.RoleType.rootWebArea &&
          nodeData.role != schema.RoleType.desktop) {
        logging.WARNING(String(nodeData.id) +
                     ' is not in the cache and not the new root.');
        lastError.set('automation',
                      'Bad update received on automation tree',
                      null,
                      chrome);
        return false;
      }
      // |this| is an AutomationRootNodeImpl; retrieve the
      // AutomationRootNode instance instead.
      node = this.wrapper;
      didUpdateRoot = true;
      updateState.newNodes[this.id] = this.wrapper;
    }
    this.setData_(node, nodeData);

    // TODO(aboxhall): send onChanged event?
    logging.CHECK(node);
    if (!this.deleteOldChildren_(node, nodeData.childIds)) {
      if (didUpdateRoot) {
        this.invalidate_(this.wrapper);
      }
      return false;
    }
    var nodeImpl = privates(node).impl;

    var success = this.createNewChildren_(node,
                                          nodeData.childIds,
                                          updateState);
    nodeImpl.childIds = nodeData.childIds;
    this.axNodeDataCache_[node.id] = node;

    return success;
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
                                                'removeEventListener',
                                                'toString'],
                                    readonly: ['isRootNode',
                                               'id',
                                               'role',
                                               'state',
                                               'location',
                                               'attributes',
                                               'root',
                                               'toString'] });

var AutomationRootNode = utils.expose('AutomationRootNode',
                                      AutomationRootNodeImpl,
                                      { superclass: AutomationNode,
                                        functions: ['load'],
                                        readonly: ['loaded'] });

exports.AutomationNode = AutomationNode;
exports.AutomationRootNode = AutomationRootNode;

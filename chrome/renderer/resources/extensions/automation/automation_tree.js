// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var Event = require('event_bindings').Event;
var AutomationNode = require('automationNode').AutomationNode;
var utils = require('utils');

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
 * AutomationTree.
 *
 * An AutomationTree is the javascript end of an AXTree living in the browser.
 * AutomationTree handles unserializing incremental updates from the source
 * AXTree. Each update contains node data that form a complete tree after
 * applying the update.
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
var AutomationTreeImpl = function(processID, routingID) {
  this.processID = processID;
  this.routingID = routingID;
  this.listeners = {};
  this.root = new AutomationNode(this);
  this.axNodeDataCache_ = {};
};

AutomationTreeImpl.prototype = {
  root: null,

  get: function(id) {
    return this.axNodeDataCache_[id];
  },

  invalidate: function(node) {
    if (!node)
      return;

    var children = node.children();

    for (var i = 0, child; child = children[i]; i++)
      this.invalidate(child);

    var id = node.id;
    for (var key in AutomationAttributeDefaults) {
      privates(node).impl[key] = AutomationAttributeDefaults[key];
    }
    privates(node).impl.loaded = false;
    privates(node).impl.id = id;
    this.axNodeDataCache_[id] = undefined;
  },

  // TODO(aboxhall): make AutomationTree inherit from AutomationNode (or a
  // mutual ancestor) and remove this code.
  addEventListener: function(eventType, callback, capture) {
    this.removeEventListener(eventType, callback);
    if (!this.listeners[eventType])
      this.listeners[eventType] = [];
    this.listeners[eventType].push({callback: callback, capture: capture});
  },

  removeEventListener: function(eventType, callback) {
    if (this.listeners[eventType]) {
      var listeners = this.listeners[eventType];
      for (var i = 0; i < listeners.length; i++) {
        if (callback === listeners[i].callback)
          listeners.splice(i, 1);
      }
    }
  },

  update: function(data) {
    var didUpdateRoot = false;

    if (data.nodes.length == 0)
      return false;

    for (var i = 0; i < data.nodes.length; i++) {
      var nodeData = data.nodes[i];
      var node = this.axNodeDataCache_[nodeData.id];
      if (!node) {
        node = new AutomationNode(this);
      }

      // Update children.
      var oldChildIDs = privates(node).impl.childIds;
      var newChildIDs = nodeData.childIds || [];
      var newChildIDsHash = {};

      for (var j = 0, newId; newId = newChildIDs[j]; j++) {
        // Hash the new child ids for faster lookup.
        newChildIDsHash[newId] = newId;

        // We need to update all new children's parent id regardless.
        var childNode = this.get(newId);
        if (!childNode) {
          childNode = new AutomationNode(this);
          this.axNodeDataCache_[newId] = childNode;
          privates(childNode).impl.id = newId;
        }
        privates(childNode).impl.indexInParent = j;
        privates(childNode).impl.parentID = nodeData.id;
      }

      for (var k = 0, oldId; oldId = oldChildIDs[k]; k++) {
        // However, we must invalidate all old child ids that are no longer
        // children.
        if (!newChildIDsHash[oldId]) {
          this.invalidate(this.get(oldId));
        }
      }

      if (nodeData.role == 'rootWebArea' || nodeData.role == 'desktop') {
        this.root = node;
        didUpdateRoot = true;
      }
      for (var key in AutomationAttributeDefaults) {
        if (key in nodeData)
          privates(node).impl[key] = nodeData[key];
        else
          privates(node).impl[key] = AutomationAttributeDefaults[key];
      }
      for (var attributeTypeIndex = 0;
           attributeTypeIndex < AutomationAttributeTypes.length;
           attributeTypeIndex++) {
        var attributeType = AutomationAttributeTypes[attributeTypeIndex];
        for (var attributeID in nodeData[attributeType]) {
          privates(node).impl.attributes[attributeID] =
              nodeData[attributeType][attributeID];
        }
      }
      privates(node).impl.childIds = newChildIDs;
      privates(node).impl.loaded = true;
      this.axNodeDataCache_[node.id] = node;
    }
    var node = this.get(data.targetID);
    if (node)
      privates(node).impl.dispatchEvent(data.eventType);
    return true;
  },

  toString: function() {
    function toStringInternal(node, indent) {
      if (!node)
        return '';
      var output =
          new Array(indent).join(' ') + privates(node).impl.toString() + '\n';
      indent += 2;
      for (var i = 0; i < node.children().length; i++)
        output += toStringInternal(node.children()[i], indent);
      return output;
    }
    return toStringInternal(this.root, 0);
  }
};

exports.AutomationTree = utils.expose('AutomationTree',
                                      AutomationTreeImpl,
                                      { functions: ['addEventListener',
                                                    'removeEventListener'],
                                        readonly: ['root'] });

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var Event = require('event_bindings').Event;
var AutomationNode = require('automationNode').AutomationNode;

// Maps an attribute to its default value in an invalidated node.
// These attributes are taken directly from the Automation idl.
var AutomationAttributeDefaults = {
  'id': -1,
  'role': '',
  'state': {}
};


var AutomationAttributeTypes = [
  'bool_attributes',
  'float_attributes',
  'html_attributes',
  'int_attributes',
  'intlist_attributes',
  'string_attributes'
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
 * @constructor
 */
var AutomationTree = function(routingId) {
  privates(this).impl = new AutomationTreeImpl(routingId);

  /**
   * Event fired when a tree update occurs.
   * @deprecated TODO(aboxhall/dtseng): remove this event; it should not be
   * exposed in the public API. Replace with EventListener style API which
   * allows listening for events in the AXEvent enum.
   */
  this.onUpdate = new Event();
};


AutomationTree.prototype = {
  /**
   * The root of this automation tree.
   * @type {Object}
   */
  get root() {
    return privates(this).impl.root;
  }
};


var AutomationTreeImpl = function(routingId) {
  this.routingId = routingId;

  /**
   * Our cache of the native AXTree.
   * @type {!Object.<number, Object>}
   * @private
   */
  this.axNodeDataCache_ = {};
};

AutomationTreeImpl.prototype = {
  root: null,

  /**
   * Looks up AXNodeData from backing AXTree.
   * We assume this cache is complete at the time a client requests a node.
   * @param {number} id The id of the AXNode to retrieve.
   * @return {AutomationNode} The data, if it exists.
   */
  get: function(id) {
    return this.axNodeDataCache_[id];
  },

  /**
   * Invalidates a subtree rooted at |node|.
   * @param {AutomationNode} id The node to invalidate.
   */
  invalidate: function(node) {
    if (!node)
      return;

    var children = node.children();

    for (var i = 0, child; child = children[i]; i++)
      this.invalidate(child);

    var id = node.id;
    for (var key in AutomationAttributeDefaults) {
      node[key] = AutomationAttributeDefaults[key];
    }
    node.id = id;
    this.axNodeDataCache_[id] = undefined;
  },

  /**
   * Send an update to this tree.
   * @param {Object} data The update.
   * @return {boolean} Whether any changes to the tree occurred.
   */
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
      var old_child_ids = privates(node).impl.child_ids;
      var new_child_ids = nodeData.child_ids || [];
      var new_child_ids_hash = {};

      for (var j = 0, newId; newId = new_child_ids[j]; j++) {
        // Hash the new child ids for faster lookup.
        new_child_ids_hash[newId] = newId;

        // We need to update all new children's parent_id regardless.
        var childNode = this.get(newId);
        if (!childNode) {
          childNode = new AutomationNode(this);
          this.axNodeDataCache_[newId] = childNode;
          childNode.id = newId;
        }
        privates(childNode).impl.index_in_parent = j;
        privates(childNode).impl.parent_id = nodeData.id;
      }

      for (var k = 0, oldId; oldId = old_child_ids[k]; k++) {
        // However, we must invalidate all old child ids that are no longer
        // children.
        if (!new_child_ids_hash[oldId]) {
          this.invalidate(this.get(oldId));
        }
      }

      if (nodeData.role == 'root_web_area') {
        this.root = node;
        didUpdateRoot = true;
      }
      for (var key in AutomationAttributeDefaults) {
        // This assumes that we sometimes don't get specific attributes (i.e. in
        // tests). Better safe than sorry.
        if (nodeData[key]) {
          node[key] = nodeData[key];
        } else {
          node[key] = AutomationAttributeDefaults[key];
        }
      }
      node.attributes = {};
      for (var attributeTypeIndex = 0;
           attributeTypeIndex < AutomationAttributeTypes.length;
           attributeTypeIndex++) {
        var attribute_type = AutomationAttributeTypes[attributeTypeIndex];
        for (var attribute_id in nodeData[attribute_type]) {
          node.attributes[attribute_id] =
              nodeData[attribute_type][attribute_id];
        }
      }
      privates(node).impl.child_ids = new_child_ids;
      this.axNodeDataCache_[node.id] = node;
    }
    return true;
  }
};


exports.AutomationTree = AutomationTree;

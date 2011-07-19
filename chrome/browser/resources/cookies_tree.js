// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('ui', function() {
  const Tree = cr.ui.Tree;
  const TreeItem = cr.ui.TreeItem;

  /**
   * Creates a new tree item for sites data.
   * @param {Object=} data Data used to create a cookie tree item.
   * @constructor
   * @extends {TreeItem}
   */
  function CookiesTreeItem(data) {
    var treeItem = new TreeItem({
      label: data.title,
      data: data
    });
    treeItem.__proto__ = CookiesTreeItem.prototype;

    if (data.icon)
      treeItem.icon = data.icon;

    treeItem.decorate();
    return treeItem;
  }

  CookiesTreeItem.prototype = {
    __proto__: TreeItem.prototype,

    /** @inheritDoc */
    decorate: function() {
      this.hasChildren = this.data.hasChildren;
    },

    /** @inheritDoc */
    addAt: function(child, index) {
      TreeItem.prototype.addAt.call(this, child, index);
      if (child.data && child.data.id)
        this.tree.treeLookup[child.data.id] = child;
    },

    /** @inheritDoc */
    remove: function(child) {
      TreeItem.prototype.remove.call(this, child);
      if (child.data && child.data.id)
        delete this.tree.treeLookup[child.data.id];
    },

    /**
     * Clears all children.
     */
    clear: function() {
      // We might leave some garbage in treeLookup for removed children.
      // But that should be okay because treeLookup is cleared when we
      // reload the tree.
      this.lastElementChild.textContent = '';
    },

    /**
     * The tree path id.
     * @type {string}
     */
    get pathId() {
      var parent = this.parentItem;
      if (parent instanceof CookiesTreeItem)
        return parent.pathId + ',' + this.data.id;
      else
        return this.data.id;
    },

    /** @inheritDoc */
    get expanded() {
      return TreeItem.prototype.__lookupGetter__('expanded').call(this);
    },
    set expanded(b) {
      if (b && this.expanded != b)
        chrome.send(this.tree.requestChildrenMessage, [this.pathId]);

      TreeItem.prototype.__lookupSetter__('expanded').call(this, b);
    }
  };

  /**
   * Creates a new cookies tree.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {Tree}
   */
  var CookiesTree = cr.ui.define('tree');

  CookiesTree.prototype = {
    __proto__: Tree.prototype,

    /**
     * Per-tree dict to map from data.id to tree node.
     */
    treeLookup_: null,
    get treeLookup() {
      if (!this.treeLookup_)
        this.treeLookup_ = {};
      return this.treeLookup_;
    },

    /** @inheritDoc */
    addAt: function(child, index) {
      Tree.prototype.addAt.call(this, child, index);
      if (child.data && child.data.id)
        this.treeLookup[child.data.id] = child;
    },

    /** @inheritDoc */
    remove: function(child) {
      Tree.prototype.remove.call(this, child);
      if (child.data && child.data.id)
        delete this.treeLookup[child.data.id];
    },

    /**
     * Add tree nodes by given parent.
     * @param {Object} parent Parent node.
     * @param {number} start Start index of where to insert nodes.
     * @param {Array} nodesData Nodes data array.
     */
    addByParent: function(parent, start, nodesData) {
      for (var i = 0; i < nodesData.length; ++i) {
        parent.addAt(new CookiesTreeItem(nodesData[i]), start + i);
      }

      cr.dispatchSimpleEvent(this, 'change');
    },

    /**
     * Add tree nodes by parent id.
     * @param {string} parentId Id of the parent node.
     * @param {int} start Start index of where to insert nodes.
     * @param {Array} nodesData Nodes data array.
     */
    addByParentId: function(parentId, start, nodesData) {
      var parent = parentId ? this.treeLookup[parentId] : this;
      this.addByParent(parent, start, nodesData);
    },

    /**
     * Removes tree nodes by parent id.
     * @param {string} parentId Id of the parent node.
     * @param {int} start Start index of nodes to remove.
     * @param {int} count Number of nodes to remove.
     */
    removeByParentId: function(parentId, start, count) {
      var parent = parentId ? this.treeLookup[parentId] : this;

      for (; count > 0 && parent.items.length; --count) {
        parent.remove(parent.items[start]);
      }

      cr.dispatchSimpleEvent(this, 'change');
    },

    /**
     * Clears the tree.
     */
    clear: function() {
      // Remove all fields without recreating the object since other code
      // references it.
      for (var id in this.treeLookup){
        delete this.treeLookup[id];
      }
      this.textContent = '';
    },

    /**
     * Unique 'requestChildren' callback message name to send request to
     * underlying CookiesTreeModelAdapter.
     * @type {string}
     */
    requestChildrenMessage_ : null,
    get requestChildrenMessage() {
      return this.requestChildrenMessage_;
    },

    /**
     * Set callback message name.
     * @param {string} loadChildren Message name for 'loadChildren' request.
     */
    doSetCallback: function(loadChildren) {
      this.requestChildrenMessage_  = loadChildren;
    },

    /**
     * Sets the immediate children of given parent node.
     * @param {string} parentId Id of the parent node.
     * @param {Array} children The immediate children of parent node.
     */
    doSetChildren: function(parentId, children) {
      var parent = parentId ? this.treeLookup[parentId] : this;

      parent.clear();
      this.addByParent(parent, 0, children);
    }
  };

  // CookiesTreeModelAdapter callbacks.
  CookiesTree.setCallback = function(treeId, message) {
    $(treeId).doSetCallback(message);
  }

  CookiesTree.onTreeItemAdded = function(treeId, parentId, start, children) {
    $(treeId).addByParentId(parentId, start, children);
  }

  CookiesTree.onTreeItemRemoved = function(treeId, parentId, start, count) {
    $(treeId).removeByParentId(parentId, start, count);
  }

  CookiesTree.setChildren = function(treeId, parentId, children) {
    $(treeId).doSetChildren(parentId, children);
  }

  return {
    CookiesTree: CookiesTree
  };
});

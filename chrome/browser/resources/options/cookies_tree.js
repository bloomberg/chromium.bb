// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const Tree = cr.ui.Tree;
  const TreeItem = cr.ui.TreeItem;

  var treeLookup = {};

  /**
   * Creates a new tree item for sites data.
   * @param {Object=} data Data used to create a cookie tree item.
   * @constructor
   * @extends {TreeItem}
   */
  function CookieTreeItem(data) {
    var treeItem = new TreeItem({
      label: data.title,
      data: data
    });
    treeItem.__proto__ = CookieTreeItem.prototype;

    if (data.icon) {
      treeItem.icon = data.icon;
    }

    treeItem.decorate();
    return treeItem;
  }

  CookieTreeItem.prototype = {
    __proto__: TreeItem.prototype,

    /** @inheritDoc */
    decorate: function() {
      this.hasChildren = this.data.hasChildren;
    },

    /** @inheritDoc */
    addAt: function(child, index) {
      TreeItem.prototype.addAt.call(this, child, index);
      if (child.data && child.data.id)
        treeLookup[child.data.id] = child;
    },

    /** @inheritDoc */
    remove: function(child) {
      TreeItem.prototype.remove.call(this, child);
      if (child.data && child.data.id)
        delete treeLookup[child.data.id];
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
      if (parent && parent instanceof CookieTreeItem) {
        return parent.pathId + ',' + this.data.id;
      } else {
        return this.data.id;
      }
    },

    /** @inheritDoc */
    get expanded() {
      return TreeItem.prototype.__lookupGetter__('expanded').call(this);
    },
    set expanded(b) {
      if (b && this.expanded != b)
        chrome.send('loadCookie', [this.pathId]);

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

    /** @inheritDoc */
    addAt: function(child, index) {
      Tree.prototype.addAt.call(this, child, index);
      if (child.data && child.data.id)
        treeLookup[child.data.id] = child;
    },

    /** @inheritDoc */
    remove: function(child) {
      Tree.prototype.remove.call(this, child);
      if (child.data && child.data.id)
        delete treeLookup[child.data.id];
    },

    /**
     * Clears the tree.
     */
    clear: function() {
      // Remove all fields without recreating the object since other code
      // references it.
      for (var id in treeLookup) {
        delete treeLookup[id];
      }
      this.textContent = '';
    },

    /**
     * Add tree nodes by given parent.
     * @param {Object} parent Parent node.
     * @param {int} start Start index of where to insert nodes.
     * @param {Array} nodesData Nodes data array.
     */
    addByParent: function(parent, start, nodesData) {
      if (!parent) {
        return;
      }

      for (var i = 0; i < nodesData.length; ++i) {
        parent.addAt(new CookieTreeItem(nodesData[i]), start + i);
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
      var parent = parentId ? treeLookup[parentId] : this;
      this.addByParent(parent, start, nodesData);
    },

    /**
     * Removes tree nodes by parent id.
     * @param {string} parentId Id of the parent node.
     * @param {int} start Start index of nodes to remove.
     * @param {int} count Number of nodes to remove.
     */
    removeByParentId: function(parentId, start, count) {
      var parent = parentId ? treeLookup[parentId] : this;
      if (!parent) {
        return;
      }

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
      for (var id in treeLookup){
        delete treeLookup[id];
      }
      this.textContent = '';
    },

    /**
     * Loads the immediate children of given parent node.
     * @param {string} parentId Id of the parent node.
     * @param {Array} children The immediate children of parent node.
     */
    loadChildren: function(parentId, children) {
      var parent = parentId ? treeLookup[parentId] : this;
      if (!parent) {
        return;
      }

      parent.clear();
      this.addByParent(parent, 0, children);
    }
  };

  return {
    CookiesTree: CookiesTree
  };
});

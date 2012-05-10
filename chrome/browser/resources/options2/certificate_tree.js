// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var Tree = cr.ui.Tree;
  /** @const */ var TreeItem = cr.ui.TreeItem;

  /**
   * Creates a new tree item for certificate data.
   * @param {Object=} data Data used to create a certificate tree item.
   * @constructor
   * @extends {TreeItem}
   */
  function CertificateTreeItem(data) {
    // TODO(mattm): other columns
    var treeItem = new TreeItem({
      label: data.name,
      data: data
    });
    treeItem.__proto__ = CertificateTreeItem.prototype;

    if (data.icon) {
      treeItem.icon = data.icon;
    }

    if (data.untrusted) {
      var badge = document.createElement('span');
      badge.classList.add('cert-untrusted');
      badge.textContent = loadTimeData.getString('badgeCertUntrusted');
      treeItem.labelElement.insertBefore(
          badge, treeItem.labelElement.firstChild);
    }

    return treeItem;
  }

  CertificateTreeItem.prototype = {
    __proto__: TreeItem.prototype,

    /**
     * The tree path id/.
     * @type {string}
     */
    get pathId() {
      var parent = this.parentItem;
      if (parent && parent instanceof CertificateTreeItem) {
        return parent.pathId + ',' + this.data.id;
      } else {
        return this.data.id;
      }
    }
  };

  /**
   * Creates a new cookies tree.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {Tree}
   */
  var CertificatesTree = cr.ui.define('tree');

  CertificatesTree.prototype = {
    __proto__: Tree.prototype,

    /** @inheritDoc */
    decorate: function() {
      Tree.prototype.decorate.call(this);
      this.treeLookup_ = {};
    },

    /** @inheritDoc */
    addAt: function(child, index) {
      Tree.prototype.addAt.call(this, child, index);
      if (child.data && child.data.id)
        this.treeLookup_[child.data.id] = child;
    },

    /** @inheritDoc */
    remove: function(child) {
      Tree.prototype.remove.call(this, child);
      if (child.data && child.data.id)
        delete this.treeLookup_[child.data.id];
    },

    /**
     * Clears the tree.
     */
    clear: function() {
      // Remove all fields without recreating the object since other code
      // references it.
      for (var id in this.treeLookup_)
        delete this.treeLookup_[id];
      this.textContent = '';
    },

    /**
     * Populate the tree.
     * @param {Array} nodesData Nodes data array.
     */
    populate: function(nodesData) {
      this.clear();

      for (var i = 0; i < nodesData.length; ++i) {
        var subnodes = nodesData[i]['subnodes'];
        delete nodesData[i]['subnodes'];

        var item = new CertificateTreeItem(nodesData[i]);
        this.addAt(item, i);

        for (var j = 0; j < subnodes.length; ++j) {
          var subitem = new CertificateTreeItem(subnodes[j]);
          item.addAt(subitem, j);
        }
        // Make tree expanded by default.
        item.expanded = true;
      }

      cr.dispatchSimpleEvent(this, 'change');
    },
  };

  return {
    CertificatesTree: CertificatesTree
  };
});


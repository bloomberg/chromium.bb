// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('bmm', function() {
  /**
   * An inorder (document order) iterator for iterating over a bookmark tree.
   *
   * <pre>
   * var it = new TreeIterator(node);
   * while (it.moveNext()) {
   *   print(it.current.title);
   * }
   * </pre>
   *
   * @param {!BookmarkTreeNode} node The node to start at.
   * @constructor
   */
  function TreeIterator(node) {
    this.current_ = node;
    this.parentStack_ = [];
    this.indexStack_ = [];
  }

  /**
   * Helper function for {@code TreeIterator.prototype.next}. This returns the
   * next node in document order.
   * @param {BookmarkTreeNode} node The current node.
   * @param {!Array.<!BookmarkTreeNode>} parents A stack of parents.
   * @param {!Array.<number>} index A stack of indexes.
   * @return {BookmarkTreeNode} The next node or null if no more nodes can be
   *     found.
   */
  function getNext(node, parents, index) {
    var i, p;

    if (!node)
      return null;

    // If the node has children return first child.
    if (node.children && node.children.length) {
      parents.push(node);
      index.push(0);
      return node.children[0];
    }

    if (!parents.length)
      return null;

    // Walk up the parent stack until we find a node that has a next sibling.
    while (node) {
      p = parents[parents.length - 1];
      if (!p)
        return null;
      i = index[index.length - 1];
      if (i + 1 < p.children.length)
        break;
      node = parents.pop();
      index.pop();
    }

    // Walked out of subtree.
    if (!parents.length || !node)
      return null;

    // Return next child.
    i = ++index[index.length - 1];
    p = parents[parents.length - 1];
    return p.children[i];
  }

  TreeIterator.prototype = {
    /**
     * Whether the next move will be the first move.
     * @type {boolean}
     * @private
     */
    first_: true,

    /**
     * Moves the iterator to the next item.
     * @return {boolean} Whether we succeeded moving to the next item. This
     * returns false when we have moved off the end of the iterator.
     */
    moveNext: function() {
      // The first call to this should move us to the first node.
      if (this.first_) {
        this.first_ = false;
        return true;
      }
      this.current_ = getNext(this.current_, this.parentStack_,
                              this.indexStack_);

      return !!this.current_;
    },

    /**
     * The current item. This throws an exception if trying to access after
     * {@code moveNext} has returned false or before {@code moveNext} has been
     * called.
     * @type {!BookmarkTreeNode}
     */
    get current() {
      if (!this.current_ || this.first_)
        throw Error('No such element');
      return this.current_;
    }
  };

  return {
    TreeIterator: TreeIterator
  };
});

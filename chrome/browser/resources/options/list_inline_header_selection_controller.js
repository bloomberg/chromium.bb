// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const ListSelectionController = cr.ui.ListSelectionController;

  /**
   * Creates a selection controller with a delegate that controls whether or
   * not individual items are selectable. This is used for lists containing
   * subgroups with headings that are in the list, since the headers themselves
   * should not be selectable.
   *
   * @param {cr.ui.ListSelectionModel} selectionModel The selection model to
   *     interact with.
   * @param {*} selectabilityDelegate A delegate responding to
   *     canSelectIndex(index).
   *
   * @constructor
   * @extends {!cr.ui.ListSelectionController}
   */
  function ListInlineHeaderSelectionController(selectionModel,
                                               selectabilityDelegate) {
    ListSelectionController.call(this, selectionModel);
    this.selectabilityDelegate_ = selectabilityDelegate;
  }

  ListInlineHeaderSelectionController.prototype = {
    __proto__: ListSelectionController.prototype,

    /** @inheritDoc */
    getIndexBelow: function(index) {
      var next = ListSelectionController.prototype.getIndexBelow.call(this,
                                                                      index);
      if (next == -1 || this.canSelect(next))
        return next;
      return this.getIndexBelow(next);
    },

    /** @inheritDoc */
    getNextIndex: function(index) {
      return this.getIndexBelow(index);
    },

    /** @inheritDoc */
    getIndexAbove: function(index) {
      var previous = ListSelectionController.prototype.getIndexAbove.call(
          this, index);
      if (previous == -1 || this.canSelect(previous))
        return previous;
      return this.getIndexAbove(previous);
    },

    /** @inheritDoc */
    getPreviousIndex: function(index) {
      return this.getIndexAbove(index);
    },

    /** @inheritDoc */
    getFirstIndex: function(index) {
      var first = ListSelectionController.prototype.getFirstIndex.call(this);
      if (this.canSelect(first))
        return first;
      return this.getNextIndex(first);
    },

    /** @inheritDoc */
    getLastIndex: function(index) {
      var last = ListSelectionController.prototype.getLastIndex.call(this);
      if (this.canSelect(last))
        return last;
      return this.getPreviousIndex(last);
    },

    /** @inheritDoc */
    handleMouseDownUp: function(e, index) {
      if (this.canSelect(index)) {
        ListSelectionController.prototype.handleMouseDownUp.call(
            this, e, index);
      }
    },

    /**
     * Returns true if the given index is selectable.
     * @private
     * @param {number} index The index to check.
     */
    canSelect: function(index) {
      return this.selectabilityDelegate_.canSelectIndex(index);
    }
  };

  return {
    ListInlineHeaderSelectionController: ListInlineHeaderSelectionController
  };
});

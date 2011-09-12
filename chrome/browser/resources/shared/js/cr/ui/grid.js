// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// require: list_selection_model.js
// require: list_selection_controller.js
// require: list.js

/**
 * @fileoverview This implements a grid control. Grid contains a bunch of
 * similar elements placed in multiple columns. It's pretty similar to the list,
 * except the multiple columns layout.
 */

cr.define('cr.ui', function() {
  const ListSelectionController = cr.ui.ListSelectionController;
  const List = cr.ui.List;
  const ListItem = cr.ui.ListItem;

  /**
   * Creates a new grid item element.
   * @param {*} dataItem The data item.
   * @constructor
   * @extends {cr.ui.ListItem}
   */
  function GridItem(dataItem) {
    var el = cr.doc.createElement('span');
    el.dataItem = dataItem;
    el.__proto__ = GridItem.prototype;
    return el;
  }

  GridItem.prototype = {
    __proto__: ListItem.prototype,

    /**
     * Called when an element is decorated as a grid item.
     */
    decorate: function() {
      ListItem.prototype.decorate.call(this, arguments);
      this.textContent = this.dataItem;
    }
  };

  /**
   * Creates a new grid element.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {cr.ui.List}
   */
  var Grid = cr.ui.define('grid');

  Grid.prototype = {
    __proto__: List.prototype,

    /**
     * The number of columns in the grid. Either set by the user, or lazy
     * calculated as the maximum number of items fitting in the grid width.
     * @type {number}
     * @private
     */
    columns_: 0,

    /**
     * Function used to create grid items.
     * @type {function(): !GridItem}
     * @override
     */
    itemConstructor_: GridItem,

    /**
     * In the case of multiple columns lead item must have the same height
     * as a regular item.
     * @type {number}
     * @override
     */
    get leadItemHeight() {
      return this.getItemHeight_();
    },
    set leadItemHeight(height) {
      // Lead item height cannot be set.
    },

    /**
     * @return {number} The number of columns determined by width of the grid
     *     and width of the items.
     * @private
     */
    getColumnCount_: function() {
      var size = this.getItemSize_();
      var width = size.width + size.marginHorizontal; // Uncollapse margin.
      return width ? Math.floor(this.clientWidth / width) : 0;
    },

    /**
     * The number of columns in the grid. If not set, determined automatically
     * as the maximum number of items fitting in the grid width.
     * @type {number}
     */
    get columns() {
      if (!this.columns_) {
        this.columns_ = this.getColumnCount_();
      }
      return this.columns_ || 1;
    },
    set columns(value) {
      if (value >= 0 && value != this.columns_) {
        this.columns_ = value;
        this.redraw();
      }
    },

    /**
     * @param {number} index The index of the item.
     * @return {number} The top position of the item inside the list, not taking
     *     into account lead item. May vary in the case of multiple columns.
     * @override
     */
    getItemTop: function(index) {
      return Math.floor(index / this.columns) * this.getItemHeight_();
    },

    /**
     * @param {number} index The index of the item.
     * @return {number} The row of the item. May vary in the case
     *     of multiple columns.
     * @override
     */
    getItemRow: function(index) {
      return Math.floor(index / this.columns);
    },

    /**
     * @param {number} row The row.
     * @return {number} The index of the first item in the row.
     * @override
     */
    getFirstItemInRow: function(row) {
      return row * this.columns;
    },

    /**
     * Creates the selection controller to use internally.
     * @param {cr.ui.ListSelectionModel} sm The underlying selection model.
     * @return {!cr.ui.ListSelectionController} The newly created selection
     *     controller.
     * @override
     */
    createSelectionController: function(sm) {
      return new GridSelectionController(sm, this);
    },

    /**
     * Calculates the number of items fitting in viewport given the index of
     * first item and heights.
     * @param {number} itemHeight The height of the item.
     * @param {number} firstIndex Index of the first item in viewport.
     * @param {number} scrollTop The scroll top position.
     * @return {number} The number of items in view port.
     * @override
     */
    getItemsInViewPort: function(itemHeight, firstIndex, scrollTop) {
      var columns = this.columns;
      var clientHeight = this.clientHeight;
      var count = this.autoExpands_ ? this.dataModel.length : Math.max(
          columns * (Math.ceil(clientHeight / itemHeight) + 1),
          this.countItemsInRange_(firstIndex, scrollTop + clientHeight));
      count = columns * Math.ceil(count / columns);
      count = Math.min(count, this.dataModel.length - firstIndex);
      return count;
    },

    /**
     * Adds items to the list and {@code newCachedItems}.
     * @param {number} firstIndex The index of first item, inclusively.
     * @param {number} lastIndex The index of last item, exclusively.
     * @param {Object.<string, ListItem>} cachedItems Old items cache.
     * @param {Object.<string, ListItem>} newCachedItems New items cache.
     * @override
     */
    addItems: function(firstIndex, lastIndex, cachedItems, newCachedItems) {
      var listItem;
      var dataModel = this.dataModel;
      var spacers = this.spacers_ || {};
      var spacerIndex = 0;
      var columns = this.columns;

      for (var y = firstIndex; y < lastIndex; y++) {
        if (y % columns == 0 && y > 0) {
          var spacer = spacers[spacerIndex];
          if (!spacer) {
            spacer = this.ownerDocument.createElement('div');
            spacer.className = 'spacer';
            spacers[spacerIndex] = spacer;
          }
          this.appendChild(spacer);
          spacerIndex++;
        }
        var dataItem = dataModel.item(y);
        listItem = cachedItems[y] || this.createItem(dataItem);
        listItem.listIndex = y;
        this.appendChild(listItem);
        newCachedItems[y] = listItem;
      }

      this.spacers_ = spacers;
    },

    /**
     * Returns the height of after filler in the list.
     * @param {number} lastIndex The index of item past the last in viewport.
     * @param {number} itemHeight The height of the item.
     * @return {number} The height of after filler.
     * @override
     */
    getAfterFillerHeight: function(lastIndex, itemHeight) {
      var columns = this.columns;
      // We calculate the row of last item, and the row of last shown item.
      // The difference is the number of rows not shown.
      var afterRows = Math.floor((this.dataModel.length - 1) / columns) -
          Math.floor((lastIndex - 1) / columns);
      return afterRows * itemHeight;
    }
  };

  /**
   * Creates a selection controller that is to be used with grids.
   * @param {cr.ui.ListSelectionModel} selectionModel The selection model to
   *     interact with.
   * @param {cr.ui.Grid} grid The grid to interact with.
   * @constructor
   * @extends {!cr.ui.ListSelectionController}
   */
  function GridSelectionController(selectionModel, grid) {
    this.selectionModel_ = selectionModel;
    this.grid_ = grid;
  }

  GridSelectionController.prototype = {
    __proto__: ListSelectionController.prototype,

    /**
     * Returns the index below (y axis) the given element.
     * @param {number} index The index to get the index below.
     * @return {number} The index below or -1 if not found.
     * @override
     */
    getIndexBelow: function(index) {
      var last = this.getLastIndex();
      if (index == last) {
        return -1;
      }
      index += this.grid_.columns;
      return Math.min(index, last);
    },

    /**
     * Returns the index above (y axis) the given element.
     * @param {number} index The index to get the index above.
     * @return {number} The index below or -1 if not found.
     * @override
     */
    getIndexAbove: function(index) {
      if (index == 0) {
        return -1;
      }
      index -= this.grid_.columns;
      return Math.max(index, 0);
    },

    /**
     * Returns the index before (x axis) the given element.
     * @param {number} index The index to get the index before.
     * @return {number} The index before or -1 if not found.
     * @override
     */
    getIndexBefore: function(index) {
      return index - 1;
    },

    /**
     * Returns the index after (x axis) the given element.
     * @param {number} index The index to get the index after.
     * @return {number} The index after or -1 if not found.
     * @override
     */
    getIndexAfter: function(index) {
      if (index == this.getLastIndex()) {
        return -1;
      }
      return index + 1;
    }
  };

  return {
    Grid: Grid,
    GridItem: GridItem,
    GridSelectionController: GridSelectionController
  }
});

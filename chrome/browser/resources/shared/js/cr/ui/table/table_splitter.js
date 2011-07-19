// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This implements a splitter element which can be used to resize
 * table columns.
 *
 * Each splitter is associated with certain column and resizes it when dragged.
 * It is column model responsibility to resize other columns accordingly.
 */

cr.define('cr.ui', function() {
  const Splitter = cr.ui.Splitter;

  /**
   * Creates a new table splitter element.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {Splitter}
   */
  var TableSplitter = cr.ui.define('div');

  TableSplitter.prototype = {
    __proto__: Splitter.prototype,

    table_: null,

    columnIndex_: null,

    /**
     * Initializes the element.
     */
    decorate: function() {
      Splitter.prototype.decorate.call(this);

      this.classList.add('table-header-splitter');
    },

    /**
     * Handles start of the splitter dragging.
     * Saves starting width of the column and changes the cursor.
     * @param {Event} e Splitter event.
     */
    handleSplitterDragStart: function() {
      var cm = this.table_.columnModel;
      this.ownerDocument.documentElement.classList.add('col-resize');

      this.columnWidth_ = cm.getWidth(this.columnIndex);
      this.nextColumnWidth_ = cm.getWidth(this.columnIndex + 1);
    },

    /**
     * Handles spliter moves. Sets new width of the column.
     * @param {Event} e Splitter event.
     */
    handleSplitterDragMove: function(deltaX) {
      var cm = this.table_.columnModel;

      var clientWidth = this.table_.querySelector('list').clientWidth;
      var percentChange = deltaX * 100 / clientWidth;
      var newColumnWidth = this.columnWidth_ + percentChange;

      cm.setWidth(this.columnIndex, newColumnWidth);
    },

    /**
     * Handles end of the splitter dragging. Restores cursor.
     * @param {Event} e Splitter event.
     */
    handleSplitterDragEnd: function() {
      this.ownerDocument.documentElement.classList.remove('col-resize');
    },
  };

  /**
   * The column index.
   * @type {number}
   */
  cr.defineProperty(TableSplitter, 'columnIndex');

  /**
   * The table associated with the splitter.
   * @type {cr.ui.Table}
   */
  cr.defineProperty(TableSplitter, 'table');

  return {
    TableSplitter: TableSplitter
  };
});

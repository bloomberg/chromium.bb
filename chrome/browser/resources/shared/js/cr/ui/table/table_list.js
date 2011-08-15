// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This extends cr.ui.List for use in the table.
 */

cr.define('cr.ui.table', function() {
  const List = cr.ui.List;
  const TableRow = cr.ui.table.TableRow;
  const ListItem = cr.ui.ListItem;

  /**
   * Creates a new table list element.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {cr.ui.List}
   */
  var TableList = cr.ui.define('list');

  TableList.prototype = {
    __proto__: List.prototype,

    table_: null,

     /**
     * Initializes the element.
     */
    decorate: function() {
      List.prototype.decorate.apply(this);
      this.className = 'list';
    },

    /**
     * Resizes columns.
     */
    resize: function() {
      var cm = this.table_.columnModel;

      var cells = this.querySelectorAll('.table-row-cell');
      if (cells.length % cm.size != 0) {
        this.redraw();
        return;
      }
      var rowsCount = cells.length / cm.size;

      for (var row = 0; row < rowsCount; row++) {
        for (var i = 0; i < cm.size; i++) {
          cells[row * cm.size + i].style.width = cm.getWidth(i) + '%';
        }
      }
    },

    /**
     * Redraws the viewport.
     */
    redraw: function() {
      if (!this.table_.columnModel)
        return;
      List.prototype.redraw.call(this);
    },

    /**
     * Creates a new list item.
     * @param {*} dataItem The value to use for the item.
     * @return {!ListItem} The newly created list item.
     */
    createItem: function(dataItem) {
      return this.table_.getRenderFunction().call(null, dataItem, this.table_);
    },

    renderFunction_: function(dataItem, table) {
      var cm = table.columnModel;
      var listItem = new ListItem({label: ''});

      listItem.className = 'table-row';

      for (var i = 0; i < cm.size; i++) {
        var cell = table.ownerDocument.createElement('div');
        cell.style.width = cm.getWidth(i) + '%';
        cell.className = 'table-row-cell';
        cell.appendChild(
            cm.getRenderFunction(i).call(null, dataItem, cm.getId(i), table));

        listItem.appendChild(cell);
      }

      return listItem;
    },
  };

  /**
   * The table associated with the list.
   * @type {cr.ui.Table}
   */
  cr.defineProperty(TableList, 'table');

  return {
    TableList: TableList
  };
});

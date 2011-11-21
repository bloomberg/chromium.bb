// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This is a table column representation
 */

cr.define('cr.ui.table', function() {
  const EventTarget = cr.EventTarget;
  const Event = cr.Event;

  /**
   * A table column that wraps column ids and settings.
   * @param {!Array} columnIds Array of column ids.
   * @constructor
   * @extends {EventTarget}
   */
  function TableColumn(id, name, width) {
    this.id_ = id;
    this.name_ = name;
    this.width_ = width;
  }

  TableColumn.prototype = {
    __proto__: EventTarget.prototype,

    id_: null,

    name_: null,

    width_: null,

    /**
     * Clones column.
     * @return {cr.ui.table.TableColumn} Clone of the given column.
     */
    clone: function() {
      var tableColumn = new TableColumn(this.id_, this.name_, this.width_);
      tableColumn.renderFunction = this.renderFunction_;
      tableColumn.headerRenderFunction = this.headerRenderFunction_;
      return tableColumn;
    },

    /**
     * Renders table cell. This is the default render function.
     * @param {*} dataItem The data item to be rendered.
     * @param {string} columnId The column id.
     * @param {cr.ui.Table} table The table.
     * @return {HTMLElement} Rendered element.
     */
    renderFunction_: function(dataItem, columnId, table) {
      var div = table.ownerDocument.createElement('div');
      div.textContent = dataItem[columnId];
      return div;
    },

    /**
     * Renders table header. This is the default render function.
     * @param {cr.ui.Table} table The table.
     * @return {HTMLElement} Rendered element.
     */
    headerRenderFunction_: function(table) {
      return table.ownerDocument.createTextNode(this.name);
    },
  };

  /**
   * The column id.
   * @type {string}
   */
  cr.defineProperty(TableColumn, 'id');

  /**
   * The column name
   * @type {string}
   */
  cr.defineProperty(TableColumn, 'name');

  /**
   * The column width.
   * @type {number}
   */
  cr.defineProperty(TableColumn, 'width');

  /**
   * The column render function.
   * @type {Function(*, string, cr.ui.Table): HTMLElement}
   */
  cr.defineProperty(TableColumn, 'renderFunction');

  /**
   * The column header render function.
   * @type {Function(cr.ui.Table): HTMLElement}
   */
  cr.defineProperty(TableColumn, 'headerRenderFunction');

  return {
    TableColumn: TableColumn
  };
});

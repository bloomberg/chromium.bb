// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This implements a table control.
 */

cr.define('cr.ui', function() {
  const TableSelectionModel = cr.ui.table.TableSelectionModel;
  const ListSelectionController = cr.ui.ListSelectionController;
  const ArrayDataModel = cr.ui.ArrayDataModel;
  const TableColumnModel = cr.ui.table.TableColumnModel;
  const TableList = cr.ui.table.TableList;
  const TableHeader = cr.ui.table.TableHeader;

  /**
   * Creates a new table element.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var Table = cr.ui.define('div');

  Table.prototype = {
    __proto__: HTMLDivElement.prototype,

    columnModel_: new TableColumnModel([]),

    /**
     * The table data model.
     *
     * @type {cr.ui.table.TableDataModel}
     */
    get dataModel() {
      return this.list_.dataModel;
    },
    set dataModel(dataModel) {
      if (this.list_.dataModel != dataModel) {
        this.list_.dataModel = dataModel;
        if (this.list_.dataModel) {
          this.list_.dataModel.removeEventListener('splice', this.boundRedraw_);
          this.list_.dataModel.removeEventListener('sorted',
                                                   this.boundHandleSorted_);
        }
        this.list_.dataModel = dataModel;
        this.list_.dataModel.table = this;


        if (this.list_.dataModel) {
          this.list_.dataModel.addEventListener('splice', this.boundRedraw_);
          this.list_.dataModel.addEventListener('sorted',
                                                this.boundHandleSorted_);
        }
        this.header_.redraw();
      }
    },

    /**
     * The table column model.
     *
     * @type {cr.ui.table.TableColumnModel}
     */
    get columnModel() {
      return this.columnModel_;
    },
    set columnModel(columnModel) {
      if (this.columnModel_ != columnModel) {
        if (this.columnModel_) {
          this.columnModel_.removeEventListener('change', this.boundRedraw_);
          this.columnModel_.removeEventListener('resize', this.boundResize_);
        }
        this.columnModel_ = columnModel;

        if (this.columnModel_) {
          this.columnModel_.addEventListener('change', this.boundRedraw_);
          this.columnModel_.addEventListener('resize', this.boundResize_);
        }
        this.redraw();
      }
    },

    /**
     * The table selection model.
     *
     * @type
     * {cr.ui.table.TableSelectionModel|cr.ui.table.TableSingleSelectionModel}
     */
    get selectionModel() {
      return this.list_.selectionModel;
    },
    set selectionModel(selectionModel) {
      if (this.list_.selectionModel != selectionModel) {
        if (this.dataModel)
          selectionModel.adjust(0, 0, this.dataModel.length);
        this.list_.selectionModel = selectionModel;
        this.redraw();
      }
    },

    /**
     * Sets width of the column at the given index.
     *
     * @param {number} index The index of the column.
     * @param {number} Column width.
     */
    setColumnWidth: function(index, width) {
      this.columnWidths_[index] = width;
    },

    /**
     * Initializes the element.
     */
    decorate: function() {
      this.list_ = this.ownerDocument.createElement('list');
      TableList.decorate(this.list_);
      this.list_.selectionModel = new TableSelectionModel(this);
      this.list_.table = this;

      this.header_ = this.ownerDocument.createElement('div');
      TableHeader.decorate(this.header_);
      this.header_.table = this;

      this.classList.add('table');
      this.appendChild(this.header_);
      this.appendChild(this.list_);
      this.ownerDocument.defaultView.addEventListener(
          'resize', this.header_.updateWidth.bind(this.header_));

      this.boundRedraw_ = this.redraw.bind(this);
      this.boundResize_ = this.resize.bind(this);
      this.boundHandleSorted_ = this.handleSorted_.bind(this);

      // Make table focusable
      if (!this.hasAttribute('tabindex'))
        this.tabIndex = 0;
      this.addEventListener('focus', this.handleElementFocus_, true);
      this.addEventListener('blur', this.handleElementBlur_, true);
    },

    /**
     * Resize the table columns.
     */
    resize: function() {
      // We resize columns only instead of full redraw.
      this.list_.resize();
      this.header_.resize();
    },

    /**
     * Ensures that a given index is inside the viewport.
     * @param {number} index The index of the item to scroll into view.
     * @return {boolean} Whether any scrolling was needed.
     */
    scrollIndexIntoView: function(i) {
      this.list_.scrollIndexIntoView(i);
    },

    /**
     * Find the list item element at the given index.
     * @param {number} index The index of the list item to get.
     * @return {ListItem} The found list item or null if not found.
     */
    getListItemByIndex: function(index) {
      return this.list_.getListItemByIndex(index);
    },

    /**
     * Redraws the table.
     * This forces the list to remove all cached items.
     */
    redraw: function() {
      this.list_.startBatchUpdates();
      if (this.list_.dataModel) {
        for (var i = 0; i < this.list_.dataModel.length; i++) {
          this.list_.redrawItem(i);
        }
      }
      this.list_.endBatchUpdates();
      this.list_.redraw();
      this.header_.redraw();
    },

    /**
     * This handles data model 'sorted' event.
     * After sorting we need to
     *  - adjust selection
     *  - redraw all the items
     *  - scroll the list to show selection.
     * @param {Event} e The 'sorted' event.
     */
    handleSorted_: function(e) {
      var sm = this.list_.selectionModel;
      sm.adjustToReordering(e.sortPermutation);

      this.redraw();
      if (sm.leadIndex != -1)
        this.list_.scrollIndexIntoView(sm.leadIndex)
    },

    /**
     * Sort data by the given column.
     * @param {number} index The index of the column to sort by.
     */
    sort: function(i) {
      var cm = this.columnModel_;
      var sortStatus = this.list_.dataModel.sortStatus;
      if (sortStatus.field == cm.getId(i)) {
        var sortDirection = sortStatus.direction == 'desc' ? 'asc' : 'desc';
        this.list_.dataModel.sort(sortStatus.field, sortDirection);
      } else {
        this.list_.dataModel.sort(cm.getId(i), 'asc');
      }
    },

    /**
     * Called when an element in the table is focused. Marks the table as having
     * a focused element, and dispatches an event if it didn't have focus.
     * @param {Event} e The focus event.
     * @private
     */
    handleElementFocus_: function(e) {
      if (!this.hasElementFocus) {
        this.hasElementFocus = true;
        // Force styles based on hasElementFocus to take effect.
        this.list_.redraw();
      }
    },

    /**
     * Called when an element in the table is blurred. If focus moves outside
     * the table, marks the table as no longer having focus and dispatches an
     * event.
     * @param {Event} e The blur event.
     * @private
     */
    handleElementBlur_: function(e) {
      // When the blur event happens we do not know who is getting focus so we
      // delay this a bit until we know if the new focus node is outside the
      // table.
      var table = this;
      var list = this.list_;
      var doc = e.target.ownerDocument;
      window.setTimeout(function() {
        var activeElement = doc.activeElement;
        if (!table.contains(activeElement)) {
          table.hasElementFocus = false;
          // Force styles based on hasElementFocus to take effect.
          list.redraw();
        }
      });
    },
  };

  /**
   * Whether the table or one of its descendents has focus. This is necessary
   * because table contents can contain controls that can be focused, and for
   * some purposes (e.g., styling), the table can still be conceptually focused
   * at that point even though it doesn't actually have the page focus.
   */
  cr.defineProperty(Table, 'hasElementFocus', cr.PropertyKind.BOOL_ATTR);

  return {
    Table: Table
  };
});

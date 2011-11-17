// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This implements a table header.
 */

cr.define('cr.ui.table', function() {
  const TableSplitter = cr.ui.TableSplitter;

  /**
   * Creates a new table header.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var TableHeader = cr.ui.define('div');

  TableHeader.prototype = {
    __proto__: HTMLDivElement.prototype,

    table_: null,

    /**
     * Initializes the element.
     */
    decorate: function() {
      this.className = 'table-header';

      this.headerInner_ = this.ownerDocument.createElement('div');
      this.headerInner_.className = 'table-header-inner';
      this.appendChild(this.headerInner_);
    },

    /**
     * Updates table header width. Header width depends on list having a
     * vertical scrollbar.
     */
    updateWidth: function() {
      // Header should not span over the vertical scrollbar of the list.
      var list = this.table_.querySelector('list');
      this.headerInner_.style.width = list.clientWidth + 'px';
    },

    /**
     * Resizes columns.
     */
    resize: function() {
      var cm = this.table_.columnModel;

      var headerCells = this.querySelectorAll('.table-header-cell');
      if (headerCells.length != cm.size) {
        this.redraw();
        return;
      }
      this.headerInner_.textContent = '';
      for (var i = 0; i < cm.size; i++) {
        headerCells[i].style.width = cm.getWidth(i) + '%';
        this.headerInner_.appendChild(headerCells[i]);
      }
      this.appendSplitters_();
    },

    /**
     * Redraws table header.
     */
    redraw: function() {
      var cm = this.table_.columnModel;
      var dm = this.table_.dataModel;

      this.updateWidth();
      this.headerInner_.textContent = '';

      if (!cm || ! dm) {
        return;
      }

      for (var i = 0; i < cm.size; i++) {
        var cell = this.ownerDocument.createElement('div');
        cell.style.width = cm.getWidth(i) + '%';
        cell.className = 'table-header-cell';
        if (dm.isSortable(cm.getId(i)))
          cell.addEventListener('click',
                                this.createSortFunction_(i).bind(this));

        cell.appendChild(this.createHeaderLabel_(i));
        this.headerInner_.appendChild(cell);
      }
      this.appendSplitters_();
    },

    /**
     * Appends column splitters to the table header.
     */
    appendSplitters_: function() {
      var cm = this.table_.columnModel;

      var leftPercent = 0;
      for (var i = 0; i < cm.size - 1; i++) {
        leftPercent += cm.getWidth(i);

        // splitter should use CSS for background image.
        var splitter = new TableSplitter({table: this.table_});
        splitter.columnIndex = i;

        var rtl = this.ownerDocument.defaultView.getComputedStyle(this).
            direction == 'rtl';
        splitter.style.left = rtl ? 100 - leftPercent + '%' : leftPercent + '%';

        this.headerInner_.appendChild(splitter);
      }
    },

    /**
     * Renders column header. Appends text label and sort arrow if needed.
     * @param {number} index Column index.
     */
    createHeaderLabel_: function(index) {
      var cm = this.table_.columnModel;
      var dm = this.table_.dataModel;

      var labelDiv = this.ownerDocument.createElement('div');
      labelDiv.className = 'table-header-label';

      var span = this.ownerDocument.createElement('span');
      span.textContent = cm.getName(index);
      var rtl = this.ownerDocument.defaultView.getComputedStyle(this).
          direction == 'rtl';
      if (rtl) {
        span.style.backgroundPosition = 'left';
        span.style.paddingRight= '0';
      } else {
        span.style.backgroundPosition = 'right';
        span.style.paddingLeft= '0';
      }
      if (dm) {
        if (dm.sortStatus.field == cm.getId(index)) {
          if (dm.sortStatus.direction == 'desc')
            span.className = 'table-header-sort-image-desc';
          else
            span.className = 'table-header-sort-image-asc';
        }
      }
      labelDiv.appendChild(span);
      return labelDiv;
    },

    /**
     * Creates sort function for given column.
     * @param {number} index The index of the column to sort by.
     */
    createSortFunction_: function(index) {
      return function() {
        this.table_.sort(index);
      }.bind(this);
    },
  };

  /**
   * The table associated with the header.
   * @type {cr.ui.Table}
   */
  cr.defineProperty(TableHeader, 'table');

  return {
    TableHeader: TableHeader
  };
});

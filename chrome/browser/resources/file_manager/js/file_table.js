// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for utility functions.
 */
var filelist = {};

/**
 * Custom column model for advanced auto-resizing.
 *
 * @param {Array.<cr.ui.table.TableColumn>} tableColumns Table columns.
 * @extends {cr.ui.table.TableColumnModel}
 * @constructor
 */
function FileTableColumnModel(tableColumns) {
  cr.ui.table.TableColumnModel.call(this, tableColumns);
}

/**
 * The columns whose index is less than the constant are resizable.
 * @const
 * @type {number}
 * @private
 */
FileTableColumnModel.RESIZABLE_LENGTH_ = 4;

/**
 * Inherits from cr.ui.TableColumnModel.
 */
FileTableColumnModel.prototype.__proto__ =
    cr.ui.table.TableColumnModel.prototype;

/**
 * Minimum width of column.
 * @const
 * @type {number}
 * @private
 */
FileTableColumnModel.MIN_WIDTH_ = 10;

/**
 * Sets column width so that the column dividers move to the specified position.
 * This function also check the width of each column and keep the width larger
 * than MIN_WIDTH_.
 *
 * @private
 * @param {Array.<number>} newPos Positions of each column dividers.
 */
FileTableColumnModel.prototype.applyColumnPositions_ = function(newPos) {
  // Check the minimum width and adjust the positions.
  for (var i = 0; i < newPos.length - 2; i++) {
    if (newPos[i + 1] - newPos[i] < FileTableColumnModel.MIN_WIDTH_) {
      newPos[i + 1] = newPos[i] + FileTableColumnModel.MIN_WIDTH_;
    }
  }
  for (var i = newPos.length - 1; i >= 2; i--) {
    if (newPos[i] - newPos[i - 1] < FileTableColumnModel.MIN_WIDTH_) {
      newPos[i - 1] = newPos[i] - FileTableColumnModel.MIN_WIDTH_;
    }
  }
  // Set the new width of columns
  for (var i = 0; i < FileTableColumnModel.RESIZABLE_LENGTH_; i++) {
    this.columns_[i].width = newPos[i + 1] - newPos[i];
  }
};

/**
 * Normalizes widths to make their sum 100% if possible. Uses the proportional
 * approach with some additional constraints.
 *
 * @param {number} contentWidth Target width.
 * @override
 */
FileTableColumnModel.prototype.normalizeWidths = function(contentWidth) {
  var totalWidth = 0;
  var fixedWidth = 0;
  // Some columns have fixed width.
  for (var i = 0; i < this.columns_.length; i++) {
    if (i < FileTableColumnModel.RESIZABLE_LENGTH_)
      totalWidth += this.columns_[i].width;
    else
      fixedWidth += this.columns_[i].width;
  }
  var newTotalWidth = Math.max(contentWidth - fixedWidth, 0);
  var positions = [0];
  var sum = 0;
  for (var i = 0; i < FileTableColumnModel.RESIZABLE_LENGTH_; i++) {
    var column = this.columns_[i];
    sum += column.width;
    // Faster alternative to Math.floor for non-negative numbers.
    positions[i + 1] = ~~(newTotalWidth * sum / totalWidth);
  }
  this.applyColumnPositions_(positions);
};

/**
 * Handles to the start of column resizing by splitters.
 */
FileTableColumnModel.prototype.handleSplitterDragStart = function() {
  this.columnPos_ = [0];
  for (var i = 0; i < this.columns_.length; i++) {
    this.columnPos_[i + 1] = this.columns_[i].width + this.columnPos_[i];
  }
};

/**
 * Handles to the end of column resizing by splitters.
 */
FileTableColumnModel.prototype.handleSplitterDragEnd = function() {
  this.columnPos_ = null;
};

/**
 * Sets the width of column with keeping the total width of table.
 * @param {number} columnIndex Index of column that is resized.
 * @param {number} columnWidth New width of the column.
 */
FileTableColumnModel.prototype.setWidthAndKeepTotal = function(
    columnIndex, columnWidth) {
  // Skip to resize 'selection' column
  if (columnIndex < 0 ||
      columnIndex >= FileTableColumnModel.RESIZABLE_LENGTH_ ||
      !this.columnPos_) {
    return;
  }

  // Calculate new positions of column splitters.
  var newPosStart =
      this.columnPos_[columnIndex] + Math.max(columnWidth,
                                              FileTableColumnModel.MIN_WIDTH_);
  var newPos = [];
  var posEnd = this.columnPos_[FileTableColumnModel.RESIZABLE_LENGTH_];
  for (var i = 0; i < columnIndex + 1; i++) {
    newPos[i] = this.columnPos_[i];
  }
  for (var i = columnIndex + 1;
       i < FileTableColumnModel.RESIZABLE_LENGTH_;
       i++) {
    var posStart = this.columnPos_[columnIndex + 1];
    newPos[i] = (posEnd - newPosStart) *
                (this.columnPos_[i] - posStart) /
                (posEnd - posStart) +
                newPosStart;
    // Faster alternative to Math.floor for non-negative numbers.
    newPos[i] = ~~newPos[i];
  }
  newPos[columnIndex] = this.columnPos_[columnIndex];
  newPos[FileTableColumnModel.RESIZABLE_LENGTH_] = posEnd;
  this.applyColumnPositions_(newPos);

  // Notifiy about resizing
  cr.dispatchSimpleEvent(this, 'resize');
};

/**
 * Custom splitter that resizes column with retaining the sum of all the column
 * width.
 */
var FileTableSplitter = cr.ui.define('div');

/**
 * Inherits from cr.ui.TableSplitter.
 */
FileTableSplitter.prototype.__proto__ = cr.ui.TableSplitter.prototype;

/**
 * Handles the drag start event.
 */
FileTableSplitter.prototype.handleSplitterDragStart = function() {
  cr.ui.TableSplitter.prototype.handleSplitterDragStart.call(this);
  this.table_.columnModel.handleSplitterDragStart();
};

/**
 * Handles the drag move event.
 * @param {number} deltaX Horizontal mouse move offset.
 */
FileTableSplitter.prototype.handleSplitterDragMove = function(deltaX) {
  this.table_.columnModel.setWidthAndKeepTotal(this.columnIndex,
                                               this.columnWidth_ + deltaX,
                                               true);
};

/**
 * Handles the drag end event.
 */
FileTableSplitter.prototype.handleSplitterDragEnd = function() {
  cr.ui.TableSplitter.prototype.handleSplitterDragEnd.call(this);
  this.table_.columnModel.handleSplitterDragEnd();
};

/**
 * File list Table View.
 * @constructor
 */
function FileTable() {
  throw new Error('Designed to decorate elements');
}

/**
 * Inherits from cr.ui.Table.
 */
FileTable.prototype.__proto__ = cr.ui.Table.prototype;

/**
 * Decorates the element.
 * @param {HTMLElement} self Table to decorate.
 * @param {MetadataCache} metadataCache To retrieve metadata.
 * @param {boolean} fullPage True if it's full page File Manager,
 *                           False if a file open/save dialog.
 */
FileTable.decorate = function(self, metadataCache, fullPage) {
  cr.ui.Table.decorate(self);
  self.__proto__ = FileTable.prototype;
  self.metadataCache_ = metadataCache;
  self.collator_ = Intl.Collator([], {numeric: true, sensitivity: 'base'});

  var columns = [
    new cr.ui.table.TableColumn('name', str('NAME_COLUMN_LABEL'),
                                fullPage ? 386 : 324),
    new cr.ui.table.TableColumn('size', str('SIZE_COLUMN_LABEL'),
                                110, true),
    new cr.ui.table.TableColumn('type', str('TYPE_COLUMN_LABEL'),
                                fullPage ? 110 : 110),
    new cr.ui.table.TableColumn('modificationTime',
                                str('DATE_COLUMN_LABEL'),
                                fullPage ? 150 : 210)
  ];

  columns[0].renderFunction = self.renderName_.bind(self);
  columns[1].renderFunction = self.renderSize_.bind(self);
  columns[1].defaultOrder = 'desc';
  columns[2].renderFunction = self.renderType_.bind(self);
  columns[3].renderFunction = self.renderDate_.bind(self);
  columns[3].defaultOrder = 'desc';

  var tableColumnModelClass;
  tableColumnModelClass = FileTableColumnModel;
  if (self.showCheckboxes) {
    columns.push(new cr.ui.table.TableColumn('selection',
                                             '',
                                             50, true));
    columns[4].renderFunction = self.renderSelection_.bind(self);
    columns[4].headerRenderFunction =
        self.renderSelectionColumnHeader_.bind(self);
    columns[4].fixed = true;
  }

  var columnModel = Object.create(tableColumnModelClass.prototype, {
    /**
     * The number of columns.
     * @type {number}
     */
    size: {
      /**
       * @this {FileTableColumnModel}
       * @return {number} Number of columns.
       */
      get: function() {
        return this.totalSize;
      }
    },

    /**
     * The number of columns.
     * @type {number}
     */
    totalSize: {
      /**
       * @this {FileTableColumnModel}
       * @return {number} Number of columns.
       */
      get: function() {
        return columns.length;
      }
    },

    /**
     * Obtains a column by the specified horizontal positon.
     */
    getHitColumn: {
      /**
       * @this {FileTableColumnModel}
       * @param {number} x Horizontal position.
       * @return {object} The object that contains column index, column width,
       *     and hitPosition where the horizontal position is hit in the column.
       */
      value: function(x) {
        for (var i = 0; x >= this.columns_[i].width; i++) {
          x -= this.columns_[i].width;
        }
        if (i >= this.columns_.length)
          return null;
        return {index: i, hitPosition: x, width: this.columns_[i].width};
      }
    }
  });

  tableColumnModelClass.call(columnModel, columns);
  self.columnModel = columnModel;
  self.setDateTimeFormat(true);
  self.setRenderFunction(self.renderTableRow_.bind(self,
      self.getRenderFunction()));

  self.scrollBar_ = MainPanelScrollBar();
  self.scrollBar_.initialize(self, self.list);
  // Keep focus on the file list when clicking on the header.
  self.header.addEventListener('mousedown', function(e) {
    self.list.focus();
    e.preventDefault();
  });

  var handleSelectionChange = function() {
    var selectAll = self.querySelector('#select-all-checkbox');
    if (selectAll)
      self.updateSelectAllCheckboxState_(selectAll);
  };

  self.relayoutAggregation_ =
      new AsyncUtil.Aggregation(self.relayoutImmediately_.bind(self));

  Object.defineProperty(self.list_, 'selectionModel', {
    /**
     * @this {cr.ui.List}
     * @return {cr.ui.ListSelectionModel} The current selection model.
     */
    get: function() {
      return this.selectionModel_;
    },
    /**
     * @this {cr.ui.List}
     */
    set: function(value) {
      var sm = this.selectionModel;
      if (sm)
        sm.removeEventListener('change', handleSelectionChange);

      util.callInheritedSetter(this, 'selectionModel', value);
      sm = value;

      if (sm)
        sm.addEventListener('change', handleSelectionChange);
      handleSelectionChange();
    }
  });

  // Override header#redraw to use FileTableSplitter.
  self.header_.redraw = function() {
    this.__proto__.redraw.call(this);
    // Extend table splitters
    var splitters = this.querySelectorAll('.table-header-splitter');
    for (var i = 0; i < splitters.length; i++) {
      if (splitters[i] instanceof FileTableSplitter)
        continue;
      FileTableSplitter.decorate(splitters[i]);
    }
  };

  // Save the last selection. This is used by shouldStartDragSelection.
  self.list.addEventListener('mousedown', function(e) {
    this.lastSelection_ = this.selectionModel.selectedIndexes;
  }.bind(self), true);
};

/**
 * Sets date and time format.
 * @param {boolean} use12hourClock True if 12 hours clock, False if 24 hours.
 */
FileTable.prototype.setDateTimeFormat = function(use12hourClock) {
  this.timeFormatter_ = Intl.DateTimeFormat(
        [] /* default locale */,
        {hour: 'numeric', minute: 'numeric',
         hour12: use12hourClock});
  this.dateFormatter_ = Intl.DateTimeFormat(
        [] /* default locale */,
        {year: 'numeric', month: 'short', day: 'numeric',
         hour: 'numeric', minute: 'numeric',
         hour12: use12hourClock});
};

/**
 * Obtains if the drag selection should be start or not by referring the mouse
 * event.
 * @param {MouseEvent} event Drag start event.
 * @return {boolean} True if the mouse is hit to the background of the list.
 */
FileTable.prototype.shouldStartDragSelection = function(event) {
  // If the shift key is pressed, it should starts drag selection.
  if (event.shiftKey)
    return true;

  // If the position values are negative, it points the out of list.
  // It should start the drag selection.
  var pos = DragSelector.getScrolledPosition(this.list, event);
  if (!pos)
    return false;
  if (pos.x < 0 || pos.y < 0)
    return true;

  // If the item index is out of range, it should start the drag selection.
  var itemHeight = this.list.measureItem().height;
  // Faster alternative to Math.floor for non-negative numbers.
  var itemIndex = ~~(pos.y / itemHeight);
  if (itemIndex >= this.list.dataModel.length)
    return true;

  // If the pointed item is already selected, it should not start the drag
  // selection.
  if (this.lastSelection_.indexOf(itemIndex) != -1)
    return false;

  // If the horizontal value is not hit to column, it shoud start the drag
  // selection.
  var hitColumn = this.columnModel.getHitColumn(pos.x);
  if (!hitColumn)
    return true;

  // Check if the point is on the column contents or not.
  var item = this.list.getListItemByIndex(itemIndex);
  switch (this.columnModel.columns_[hitColumn.index].id) {
    case 'name':
      var spanElement = item.querySelector('.filename-label span');
      var spanRect = spanElement.getBoundingClientRect();
      // The this.list.cachedBounds_ object is set by
      // DragSelector.getScrolledPosition.
      if (!this.list.cachedBounds)
        return true;
      var textRight =
          spanRect.left - this.list.cachedBounds.left + spanRect.width;
      return textRight <= hitColumn.hitPosition;
    default:
      return true;
  }
};

/**
 * Update check and disable states of the 'Select all' checkbox.
 * @param {HTMLInputElement} checkbox The checkbox. If not passed, using
 *     the default one.
 * @private
 */
FileTable.prototype.updateSelectAllCheckboxState_ = function(checkbox) {
  // TODO(serya): introduce this.selectionModel.selectedCount.
  checkbox.checked = this.dataModel.length > 0 &&
      this.dataModel.length == this.selectionModel.selectedIndexes.length;
  checkbox.disabled = this.dataModel.length == 0;
};

/**
 * Prepares the data model to be sorted by columns.
 * @param {cr.ui.ArrayDataModel} dataModel Data model to prepare.
 */
FileTable.prototype.setupCompareFunctions = function(dataModel) {
  dataModel.setCompareFunction('name',
                               this.compareName_.bind(this));
  dataModel.setCompareFunction('modificationTime',
                               this.compareMtime_.bind(this));
  dataModel.setCompareFunction('size',
                               this.compareSize_.bind(this));
  dataModel.setCompareFunction('type',
                               this.compareType_.bind(this));
};

/**
 * Render the Name column of the detail table.
 *
 * Invoked by cr.ui.Table when a file needs to be rendered.
 *
 * @param {Entry} entry The Entry object to render.
 * @param {string} columnId The id of the column to be rendered.
 * @param {cr.ui.Table} table The table doing the rendering.
 * @return {HTMLDivElement} Created element.
 * @private
 */
FileTable.prototype.renderName_ = function(entry, columnId, table) {
  var label = this.ownerDocument.createElement('div');
  label.appendChild(this.renderIconType_(entry, columnId, table));
  label.entry = entry;
  label.className = 'detail-name';
  label.appendChild(filelist.renderFileNameLabel(this.ownerDocument, entry));
  return label;
};

/**
 * Render the Selection column of the detail table.
 *
 * Invoked by cr.ui.Table when a file needs to be rendered.
 *
 * @param {Entry} entry The Entry object to render.
 * @param {string} columnId The id of the column to be rendered.
 * @param {cr.ui.Table} table The table doing the rendering.
 * @return {HTMLDivElement} Created element.
 * @private
 */
FileTable.prototype.renderSelection_ = function(entry, columnId, table) {
  var label = this.ownerDocument.createElement('div');
  label.className = 'selection-label';
  if (this.selectionModel.multiple) {
    var checkBox = this.ownerDocument.createElement('input');
    filelist.decorateSelectionCheckbox(checkBox, entry, this.list);
    label.appendChild(checkBox);
  }
  return label;
};

/**
 * Render the Size column of the detail table.
 *
 * @param {Entry} entry The Entry object to render.
 * @param {string} columnId The id of the column to be rendered.
 * @param {cr.ui.Table} table The table doing the rendering.
 * @return {HTMLDivElement} Created element.
 * @private
 */
FileTable.prototype.renderSize_ = function(entry, columnId, table) {
  var div = this.ownerDocument.createElement('div');
  div.className = 'size';
  this.updateSize_(
      div, entry, this.metadataCache_.getCached(entry, 'filesystem'));

  return div;
};

/**
 * Sets up or updates the size cell.
 *
 * @param {HTMLDivElement} div The table cell.
 * @param {Entry} entry The corresponding entry.
 * @param {Object} filesystemProps Metadata.
 * @private
 */
FileTable.prototype.updateSize_ = function(div, entry, filesystemProps) {
  if (!filesystemProps) {
    div.textContent = '...';
  } else if (filesystemProps.size == -1) {
    div.textContent = '--';
  } else if (filesystemProps.size == 0 &&
             FileType.isHosted(entry)) {
    div.textContent = '--';
  } else {
    div.textContent = util.bytesToString(filesystemProps.size);
  }
};

/**
 * Render the Type column of the detail table.
 *
 * @param {Entry} entry The Entry object to render.
 * @param {string} columnId The id of the column to be rendered.
 * @param {cr.ui.Table} table The table doing the rendering.
 * @return {HTMLDivElement} Created element.
 * @private
 */
FileTable.prototype.renderType_ = function(entry, columnId, table) {
  var div = this.ownerDocument.createElement('div');
  div.className = 'type';
  div.textContent = FileType.getTypeString(entry);
  return div;
};

/**
 * Render the Date column of the detail table.
 *
 * @param {Entry} entry The Entry object to render.
 * @param {string} columnId The id of the column to be rendered.
 * @param {cr.ui.Table} table The table doing the rendering.
 * @return {HTMLDivElement} Created element.
 * @private
 */
FileTable.prototype.renderDate_ = function(entry, columnId, table) {
  var div = this.ownerDocument.createElement('div');
  div.className = 'date';

  this.updateDate_(div,
      this.metadataCache_.getCached(entry, 'filesystem'));
  return div;
};

/**
 * Sets up or updates the date cell.
 *
 * @param {HTMLDivElement} div The table cell.
 * @param {Object} filesystemProps Metadata.
 * @private
 */
FileTable.prototype.updateDate_ = function(div, filesystemProps) {
  if (!filesystemProps) {
    div.textContent = '...';
    return;
  }

  var modTime = filesystemProps.modificationTime;
  var today = new Date();
  today.setHours(0);
  today.setMinutes(0);
  today.setSeconds(0);
  today.setMilliseconds(0);

  /**
   * Number of milliseconds in a day.
   */
  var MILLISECONDS_IN_DAY = 24 * 60 * 60 * 1000;

  if (modTime >= today &&
      modTime < today.getTime() + MILLISECONDS_IN_DAY) {
    div.textContent = strf('TIME_TODAY', this.timeFormatter_.format(modTime));
  } else if (modTime >= today - MILLISECONDS_IN_DAY && modTime < today) {
    div.textContent = strf('TIME_YESTERDAY',
                           this.timeFormatter_.format(modTime));
  } else {
    div.textContent =
        this.dateFormatter_.format(filesystemProps.modificationTime);
  }
};

/**
 * Updates the file metadata in the table item.
 *
 * @param {Element} item Table item.
 * @param {Entry} entry File entry.
 */
FileTable.prototype.updateFileMetadata = function(item, entry) {
  var props = this.metadataCache_.getCached(entry, 'filesystem');
  this.updateDate_(item.querySelector('.date'), props);
  this.updateSize_(item.querySelector('.size'), entry, props);
};

/**
 * Updates list items 'in place' on metadata change.
 * @param {string} type Type of metadata change.
 * @param {Object.<sting, Object>} propsMap Map from entry URLs to metadata
 *                                          properties.
 */
FileTable.prototype.updateListItemsMetadata = function(type, propsMap) {
  var forEachCell = function(selector, callback) {
    var cells = this.querySelectorAll(selector);
    for (var i = 0; i < cells.length; i++) {
      var cell = cells[i];
      var listItem = this.list_.getListItemAncestor(cell);
      var entry = this.dataModel.item(listItem.listIndex);
      if (entry) {
        var props = propsMap[entry.toURL()];
        if (props)
          callback.call(this, cell, entry, props, listItem);
      }
    }
  }.bind(this);
  if (type == 'filesystem') {
    forEachCell('.table-row-cell > .date', function(item, entry, props) {
      this.updateDate_(item, props);
    });
    forEachCell('.table-row-cell > .size', function(item, entry, props) {
      this.updateSize_(item, entry, props);
    });
  } else if (type == 'drive') {
    // The cell name does not matter as the entire list item is needed.
    forEachCell('.table-row-cell > .date',
                function(item, entry, props, listItem) {
      filelist.updateListItemDriveProps(listItem, props);
    });
  }
};

/**
 * Compare by mtime first, then by name.
 * @param {Entry} a First entry.
 * @param {Entry} b Second entry.
 * @return {number} Compare result.
 * @private
 */
FileTable.prototype.compareName_ = function(a, b) {
  return this.collator_.compare(a.name, b.name);
};

/**
 * Compare by mtime first, then by name.
 * @param {Entry} a First entry.
 * @param {Entry} b Second entry.
 * @return {number} Compare result.
 * @private
 */
FileTable.prototype.compareMtime_ = function(a, b) {
  var aCachedFilesystem = this.metadataCache_.getCached(a, 'filesystem');
  var aTime = aCachedFilesystem ? aCachedFilesystem.modificationTime : 0;

  var bCachedFilesystem = this.metadataCache_.getCached(b, 'filesystem');
  var bTime = bCachedFilesystem ? bCachedFilesystem.modificationTime : 0;

  if (aTime > bTime)
    return 1;

  if (aTime < bTime)
    return -1;

  return this.collator_.compare(a.name, b.name);
};

/**
 * Compare by size first, then by name.
 * @param {Entry} a First entry.
 * @param {Entry} b Second entry.
 * @return {number} Compare result.
 * @private
 */
FileTable.prototype.compareSize_ = function(a, b) {
  var aCachedFilesystem = this.metadataCache_.getCached(a, 'filesystem');
  var aSize = aCachedFilesystem ? aCachedFilesystem.size : 0;

  var bCachedFilesystem = this.metadataCache_.getCached(b, 'filesystem');
  var bSize = bCachedFilesystem ? bCachedFilesystem.size : 0;

  if (aSize != bSize) return aSize - bSize;
    return this.collator_.compare(a.name, b.name);
};

/**
 * Compare by type first, then by subtype and then by name.
 * @param {Entry} a First entry.
 * @param {Entry} b Second entry.
 * @return {number} Compare result.
 * @private
 */
FileTable.prototype.compareType_ = function(a, b) {
  // Directories precede files.
  if (a.isDirectory != b.isDirectory)
    return Number(b.isDirectory) - Number(a.isDirectory);

  var aType = FileType.getTypeString(a);
  var bType = FileType.getTypeString(b);

  var result = this.collator_.compare(aType, bType);
  if (result != 0)
    return result;

  return this.collator_.compare(a.name, b.name);
};

/**
 * Renders table row.
 * @param {function(Entry, cr.ui.Table)} baseRenderFunction Base renderer.
 * @param {Entry} entry Corresponding entry.
 * @return {HTMLLiElement} Created element.
 * @private
 */
FileTable.prototype.renderTableRow_ = function(baseRenderFunction, entry) {
  var item = baseRenderFunction(entry, this);
  filelist.decorateListItem(item, entry, this.metadataCache_);
  return item;
};

/**
 * Renders the name column header.
 * @param {string} name Localized column name.
 * @return {HTMLLiElement} Created element.
 * @private
 */
FileTable.prototype.renderNameColumnHeader_ = function(name) {
  if (!this.selectionModel.multiple)
    return this.ownerDocument.createTextNode(name);

  var input = this.ownerDocument.createElement('input');
  input.setAttribute('type', 'checkbox');
  input.setAttribute('tabindex', -1);
  input.id = 'select-all-checkbox';
  input.className = 'common';

  this.updateSelectAllCheckboxState_(input);

  input.addEventListener('click', function(event) {
    if (input.checked)
      this.selectionModel.selectAll();
    else
      this.selectionModel.unselectAll();
    event.stopPropagation();
  }.bind(this));

  var fragment = this.ownerDocument.createDocumentFragment();
  fragment.appendChild(input);
  fragment.appendChild(this.ownerDocument.createTextNode(name));
  return fragment;
};

/**
 * Renders the selection column header.
 * @param {string} name Localized column name.
 * @return {HTMLLiElement} Created element.
 * @private
 */
FileTable.prototype.renderSelectionColumnHeader_ = function(name) {
  if (!this.selectionModel.multiple)
    return this.ownerDocument.createTextNode('');

  var input = this.ownerDocument.createElement('input');
  input.setAttribute('type', 'checkbox');
  input.setAttribute('tabindex', -1);
  input.id = 'select-all-checkbox';
  input.className = 'common';

  this.updateSelectAllCheckboxState_(input);

  input.addEventListener('click', function(event) {
    if (input.checked)
      this.selectionModel.selectAll();
    else
      this.selectionModel.unselectAll();
    event.stopPropagation();
  }.bind(this));

  var fragment = this.ownerDocument.createDocumentFragment();
  fragment.appendChild(input);
  return fragment;
};

/**
 * Render the type column of the detail table.
 *
 * Invoked by cr.ui.Table when a file needs to be rendered.
 *
 * @param {Entry} entry The Entry object to render.
 * @param {string} columnId The id of the column to be rendered.
 * @param {cr.ui.Table} table The table doing the rendering.
 * @return {HTMLDivElement} Created element.
 * @private
 */
FileTable.prototype.renderIconType_ = function(entry, columnId, table) {
  var icon = this.ownerDocument.createElement('div');
  icon.className = 'detail-icon';
  icon.setAttribute('file-type-icon', FileType.getIcon(entry));
  return icon;
};

/**
 * Sets the margin height for the transparent preview panel at the bottom.
 * @param {number} margin Margin to be set in px.
 */
FileTable.prototype.setBottomMarginForPanel = function(margin) {
  this.list_.style.paddingBottom = margin + 'px';
  this.scrollBar_.setBottomMarginForPanel(margin);
};

/**
 * Redraws the UI. Skips multiple consecutive calls.
 */
FileTable.prototype.relayout = function() {
  this.relayoutAggregation_.run();
};

/**
 * Redraws the UI immediately.
 * @private
 */
FileTable.prototype.relayoutImmediately_ = function() {
  if (this.clientWidth > 0)
    this.normalizeColumns();
  this.redraw();
  cr.dispatchSimpleEvent(this.list, 'relayout');
};

/**
 * Decorates (and wire up) a checkbox to be used in either a detail or a
 * thumbnail list item.
 * @param {HTMLInputElement} input Element to decorate.
 */
filelist.decorateCheckbox = function(input) {
  var stopEventPropagation = function(event) {
    if (!event.shiftKey)
      event.stopPropagation();
  };
  input.setAttribute('type', 'checkbox');
  input.setAttribute('tabindex', -1);
  input.classList.add('common');
  input.addEventListener('mousedown', stopEventPropagation);
  input.addEventListener('mouseup', stopEventPropagation);

  input.addEventListener(
      'click',
      /**
       * @this {HTMLInputElement}
       */
      function(event) {
        // Revert default action and swallow the event
        // if this is a multiple click or Shift is pressed.
        if (event.detail > 1 || event.shiftKey) {
          this.checked = !this.checked;
          stopEventPropagation(event);
        }
      });
};

/**
 * Decorates selection checkbox.
 * @param {HTMLInputElement} input Element to decorate.
 * @param {Entry} entry Corresponding entry.
 * @param {cr.ui.List} list Owner list.
 */
filelist.decorateSelectionCheckbox = function(input, entry, list) {
  filelist.decorateCheckbox(input);
  input.classList.add('file-checkbox');
  input.addEventListener('click', function(e) {
    var sm = list.selectionModel;
    var listIndex = list.getListItemAncestor(this).listIndex;
    sm.setIndexSelected(listIndex, this.checked);
    sm.leadIndex = listIndex;
    if (sm.anchorIndex == -1)
      sm.anchorIndex = listIndex;

  });
  // Since we do not want to open the item when tap on checkbox, we need to
  // stop propagation of TAP event dispatched by checkbox ideally. But all
  // touch events from touch_handler are dispatched to the list control. So we
  // have to stop propagation of native touchstart event to prevent list
  // control from generating TAP event here. The synthetic click event will
  // select the touched checkbox/item.
  input.addEventListener('touchstart',
                         function(e) { e.stopPropagation() });

  var index = list.dataModel.indexOf(entry);
  // Our DOM nodes get discarded as soon as we're scrolled out of view,
  // so we have to make sure the check state is correct when we're brought
  // back to life.
  input.checked = list.selectionModel.getIndexSelected(index);
};

/**
 * Common item decoration for table's and grid's items.
 * @param {ListItem} li List item.
 * @param {Entry} entry The entry.
 * @param {MetadataCache} metadataCache Cache to retrieve metadada.
 */
filelist.decorateListItem = function(li, entry, metadataCache) {
  li.classList.add(entry.isDirectory ? 'directory' : 'file');
  if (FileType.isOnDrive(entry)) {
    // The metadata may not yet be ready. In that case, the list item will be
    // updated when the metadata is ready via updateListItemsMetadata.
    var driveProps = metadataCache.getCached(entry, 'drive');
    if (driveProps)
      filelist.updateListItemDriveProps(li, driveProps);
  }

  // Overriding the default role 'list' to 'listbox' for better
  // accessibility on ChromeOS.
  li.setAttribute('role', 'option');

  Object.defineProperty(li, 'selected', {
    /**
     * @this {ListItem}
     * @return {boolean} True if the list item is selected.
     */
    get: function() {
      return this.hasAttribute('selected');
    },

    /**
     * @this {ListItem}
     */
    set: function(v) {
      if (v)
        this.setAttribute('selected');
      else
        this.removeAttribute('selected');
      var checkBox = this.querySelector('input.file-checkbox');
      if (checkBox)
        checkBox.checked = !!v;
    }
  });
};

/**
 * Render filename label for grid and list view.
 * @param {HTMLDocument} doc Owner document.
 * @param {Entry} entry The Entry object to render.
 * @return {HTMLDivElement} The label.
 */
filelist.renderFileNameLabel = function(doc, entry) {
  // Filename need to be in a '.filename-label' container for correct
  // work of inplace renaming.
  var box = doc.createElement('div');
  box.className = 'filename-label';
  var fileName = doc.createElement('span');
  fileName.textContent = entry.name;
  box.appendChild(fileName);

  return box;
};

/**
 * Updates grid item or table row for the driveProps.
 * @param {cr.ui.ListItem} li List item.
 * @param {Object} driveProps Metadata.
 */
filelist.updateListItemDriveProps = function(li, driveProps) {
  if (li.classList.contains('file')) {
    if (driveProps.availableOffline)
      li.classList.remove('dim-offline');
    else
      li.classList.add('dim-offline');
    // TODO(mtomasz): Consider adding some vidual indication for files which
    // are not cached on LTE. Currently we show them as normal files.
    // crbug.com/246611.
  }

  if (driveProps.driveApps.length > 0) {
    var iconDiv = li.querySelector('.detail-icon');
    if (!iconDiv)
      return;
    // Find the default app for this file.  If there is none, then
    // leave it as the base icon for the file type.
    var url;
    for (var i = 0; i < driveProps.driveApps.length; ++i) {
      var app = driveProps.driveApps[i];
      if (app && app.docIcon && app.isPrimary) {
        url = app.docIcon;
        break;
      }
    }
    if (url) {
      iconDiv.style.backgroundImage = 'url(' + url + ')';
    } else {
      iconDiv.style.backgroundImage = null;
    }
  }
};

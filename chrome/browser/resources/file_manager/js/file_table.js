// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Namespace for utility functions.
 */
var filelist = {};

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
  self.collator_ = v8Intl.Collator([], {numeric: true, sensitivity: 'base'});

  var columns = [
        new cr.ui.table.TableColumn('name', str('NAME_COLUMN_LABEL'),
                                    fullPage ? 386 : 324),
        new cr.ui.table.TableColumn('size', str('SIZE_COLUMN_LABEL'),
                                    fullPage ? 100 : 92, true),
        new cr.ui.table.TableColumn('type', str('TYPE_COLUMN_LABEL'),
                                    fullPage ? 160 : 160),
        new cr.ui.table.TableColumn('modificationTime',
                                    str('DATE_COLUMN_LABEL'),
                                    fullPage ? 150 : 210),
        new cr.ui.table.TableColumn('offline',
                                    str('OFFLINE_COLUMN_LABEL'),
                                    130)
    ];

  columns[0].renderFunction = self.renderName_.bind(self);
  columns[1].renderFunction = self.renderSize_.bind(self);
  columns[1].defaultOrder = 'desc';
  columns[2].renderFunction = self.renderType_.bind(self);
  columns[3].renderFunction = self.renderDate_.bind(self);
  columns[3].defaultOrder = 'desc';
  columns[4].renderFunction = self.renderOffline_.bind(self);

  columns[0].headerRenderFunction =
      self.renderNameColumnHeader_.bind(self, columns[0].name);

  var columnModel = Object.create(cr.ui.table.TableColumnModel.prototype, {
    size: {
      get: function() {
        return this.showOfflineColumn ? this.totalSize : this.totalSize - 1;
      }
    },

    totalSize: {
      get: function() {
        return columns.length;
      }
    },

    showOfflineColumn: {
      writable: true,
      value: false
    }
  });
  cr.ui.table.TableColumnModel.call(columnModel, columns);
  self.columnModel = columnModel;
  self.setDateTimeFormat(true);
  self.setRenderFunction(self.renderTableRow_.bind(self,
      self.getRenderFunction()));

  var handleSelectionChange = function() {
    var selectAll = self.querySelector('#select-all-checkbox');
    if (selectAll)
      self.updateSelectAllCheckboxState_(selectAll);
  };

  Object.defineProperty(self.list_, 'selectionModel', {
    get: function() {
      return this.selectionModel_;
    },
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
};

/**
 * Shows or hides 'Avaliable offline' column.
 * @param {boolean} show True to show.
 */
FileTable.prototype.showOfflineColumn = function(show) {
  if (show != this.columnModel.showOfflineColumn) {
    this.columnModel.showOfflineColumn = show;
    this.redraw();
  }
};

/**
 * Sets date and time format.
 * @param {boolean} use12hourClock True if 12 hours clock, False if 24 hours.
 */
FileTable.prototype.setDateTimeFormat = function(use12hourClock) {
  this.timeFormatter_ = v8Intl.DateTimeFormat(
        [] /* default locale */,
        {hour: 'numeric', minute: 'numeric',
         hour12: use12hourClock});
  this.dateFormatter_ = v8Intl.DateTimeFormat(
        [] /* default locale */,
        {year: 'numeric', month: 'short', day: 'numeric',
         hour: 'numeric', minute: 'numeric',
         hour12: use12hourClock});
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
  if (this.selectionModel.multiple) {
    var checkBox = this.ownerDocument.createElement('INPUT');
    filelist.decorateSelectionCheckbox(checkBox, entry, this.list);
    label.appendChild(checkBox);
  }
  label.appendChild(this.renderIconType_(entry, columnId, table));
  label.entry = entry;
  label.className = 'detail-name';
  label.appendChild(filelist.renderFileNameLabel(this.ownerDocument, entry));
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
 * Render the Available offline column of the detail table.
 *
 * @param {Entry} entry The Entry object to render.
 * @param {string} columnId The id of the column to be rendered.
 * @param {cr.ui.Table} table The table doing the rendering.
 * @return {HTMLDivElement} Created element.
 * @private
 */
FileTable.prototype.renderOffline_ = function(entry, columnId, table) {
  var div = this.ownerDocument.createElement('div');
  div.className = 'offline';

  if (entry.isDirectory)
    return div;

  var checkbox = this.ownerDocument.createElement('INPUT');
  filelist.decorateCheckbox(checkbox);
  checkbox.classList.add('pin');

  var command = this.ownerDocument.querySelector('command#toggle-pinned');
  var onPinClick = function(event) {
    command.canExecuteChange(checkbox);
    command.execute(checkbox);
    event.preventDefault();
  };

  checkbox.addEventListener('click', onPinClick);
  checkbox.style.display = 'none';
  checkbox.entry = entry;
  div.appendChild(checkbox);

  this.updateOffline_(
      div, this.metadataCache_.getCached(entry, 'drive'));
  return div;
};

/**
 * Sets up or updates the date cell.
 *
 * @param {HTMLDivElement} div The table cell.
 * @param {Object} drive Metadata.
 * @private
 */
FileTable.prototype.updateOffline_ = function(div, drive) {
  if (!drive) return;
  if (drive.hosted) return;
  var checkbox = div.querySelector('.pin');
  if (!checkbox) return;
  checkbox.style.display = '';
  checkbox.checked = drive.pinned;
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
    forEachCell('.table-row-cell > .offline',
                function(item, entry, props, listItem) {
      this.updateOffline_(item, props);
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
  if (!this.selectionModel.multiple) {
    return this.ownerDocument.createTextNode(name);
  }
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
    var driveProps = metadataCache.getCached(entry, 'drive');
    if (driveProps)
      filelist.updateListItemDriveProps(li, driveProps);
  }

  // Overriding the default role 'list' to 'listbox' for better
  // accessibility on ChromeOS.
  li.setAttribute('role', 'option');

  Object.defineProperty(li, 'selected', {
    get: function() {
      return this.hasAttribute('selected');
    },

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
    if (driveProps.availableWhenMetered)
      li.classList.remove('dim-metered');
    else
      li.classList.add('dim-metered');
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

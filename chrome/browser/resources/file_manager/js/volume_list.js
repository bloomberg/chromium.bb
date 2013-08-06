// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * A volume list model. This model combines the 2 lists.
 * @param {cr.ui.ArrayDataModel} volumesList The first list of the model.
 * @param {cr.ui.ArrayDataModel} pinnedList The second list of the model.
 * @constructor
 * @extends {cr.EventTarget}
 */
function VolumeListModel(volumesList, pinnedList) {
  this.volumesList_ = volumesList;
  this.pinnedList_ = pinnedList;

  // Generates a combined 'permuted' event from an event of either list.
  var permutedHandler = function(listNum, e) {
    var permutedEvent = new Event('permuted');
    var newPermutation = [];
    var newLength;
    if (listNum == 1) {
      newLength = e.newLength + this.pinnedList_.length;
      for (var i = 0; i < e.permutation.length; i++) {
        newPermutation[i] = e.permutation[i];
      }
      for (var i = 0; i < this.pinnedList_.length; i++) {
        newPermutation[i + e.permutation.length] = i + e.newLength;
      }
    } else {
      var volumesLen = this.volumesList_.length;
      newLength = e.newLength + volumesLen;
      for (var i = 0; i < volumesLen; i++) {
        newPermutation[i] = i;
      }
      for (var i = 0; i < e.permutation.length; i++) {
        newPermutation[i + volumesLen] =
            (e.permutation[i] !== -1) ? (e.permutation[i] + volumesLen) : -1;
      }
    }

    permutedEvent.newLength = newLength;
    permutedEvent.permutation = newPermutation;
    this.dispatchEvent(permutedEvent);
  };
  this.volumesList_.addEventListener('permuted', permutedHandler.bind(this, 1));
  this.pinnedList_.addEventListener('permuted', permutedHandler.bind(this, 2));

  // Generates a combined 'change' event from an event of either list.
  var changeHandler = function(listNum, e) {
    var changeEvent = new Event('change');
    changeEvent.index =
        (listNum == 1) ? e.index : (e.index + this.volumesList_.length);
    this.dispatchEvent(changeEvent);
  };
  this.volumesList_.addEventListener('change', changeHandler.bind(this, 1));
  this.pinnedList_.addEventListener('change', changeHandler.bind(this, 2));

  // 'splice' and 'sorted' events are not implemented, since they are not used
  // in list.js.
}

/**
 * VolumeList inherits cr.EventTarget.
 */
VolumeListModel.prototype = {
  __proto__: cr.EventTarget.prototype,
  get length() { return this.length_(); }
};

/**
 * Returns the item at the given index.
 * @param {number} index The index of the entry to get.
 * @return {?string} The path at the given index.
 */
VolumeListModel.prototype.item = function(index) {
  var offset = this.volumesList_.length;
  if (index < offset) {
    var entry = this.volumesList_.item(index);
    return entry ? entry.fullPath : undefined;
  } else {
    return this.pinnedList_.item(index - offset);
  }
};

/**
 * Type of the item on the volume list.
 * @enum {number}
 */
VolumeListModel.ItemType = {
  ROOT: 1,
  PINNED: 2
};

/**
 * Returns the type of the item at the given index.
 * @param {number} index The index of the entry to get.
 * @return {VolumeListModel.ItemType} The type of the item.
 */
VolumeListModel.prototype.getItemType = function(index) {
  var offset = this.volumesList_.length;
  return index < offset ?
      VolumeListModel.ItemType.ROOT : VolumeListModel.ItemType.PINNED;
};

/**
 * Returns the number of items in the model.
 * @return {number} The length of the model.
 * @private
 */
VolumeListModel.prototype.length_ = function() {
  return this.volumesList_.length + this.pinnedList_.length;
};

/**
 * Returns the first matching item.
 * @param {Entry} item The entry to find.
 * @param {number=} opt_fromIndex If provided, then the searching start at
 *     the {@code opt_fromIndex}.
 * @return {number} The index of the first found element or -1 if not found.
 */
VolumeListModel.prototype.indexOf = function(item, opt_fromIndex) {
  for (var i = opt_fromIndex || 0; i < this.length; i++) {
    if (item === this.item(i))
      return i;
  }
  return -1;
};

/**
 * A volume item.
 * @constructor
 * @extends {HTMLLIElement}
 */
var VolumeItem = cr.ui.define('li');

VolumeItem.prototype = {
  __proto__: HTMLLIElement.prototype,
};

/**
 * Decorate the item.
 */
VolumeItem.prototype.decorate = function() {
  // decorate() may be called twice: from the constructor and from
  // List.createItem(). This check prevents double-decorating.
  if (this.className)
    return;

  this.className = 'root-item';
  this.setAttribute('role', 'option');

  this.iconDiv_ = cr.doc.createElement('div');
  this.iconDiv_.className = 'volume-icon';
  this.appendChild(this.iconDiv_);

  this.label_ = cr.doc.createElement('div');
  this.label_.className = 'root-label';
  this.appendChild(this.label_);

  cr.defineProperty(this, 'lead', cr.PropertyKind.BOOL_ATTR);
  cr.defineProperty(this, 'selected', cr.PropertyKind.BOOL_ATTR);
};

/**
 * Associate a path with this item.
 * @param {string} path Path of this item.
 */
VolumeItem.prototype.setPath = function(path) {
  if (this.path_)
    console.warn('VolumeItem.setPath should be called only once.');

  this.path_ = path;

  var rootType = PathUtil.getRootType(path);

  this.iconDiv_.setAttribute('volume-type-icon', rootType);
  if (rootType === RootType.REMOVABLE) {
    this.iconDiv_.setAttribute('volume-subtype',
        VolumeManager.getInstance().getDeviceType(path));
  }

  this.label_.textContent = PathUtil.getFolderLabel(path);

  if (rootType === RootType.ARCHIVE || rootType === RootType.REMOVABLE) {
    this.eject_ = cr.doc.createElement('div');
    // Block other mouse handlers.
    this.eject_.addEventListener(
        'mouseup', function(e) { e.stopPropagation() });
    this.eject_.addEventListener(
        'mousedown', function(e) { e.stopPropagation() });

    this.eject_.className = 'root-eject';
    this.eject_.addEventListener('click', function(event) {
      event.stopPropagation();
      cr.dispatchSimpleEvent(this, 'eject');
    }.bind(this));

    this.appendChild(this.eject_);
  }
};

/**
 * Associate a context menu with this item.
 * @param {cr.ui.Menu} menu Menu this item.
 */
VolumeItem.prototype.maybeSetContextMenu = function(menu) {
  if (!this.path_) {
    console.error(
        'VolumeItem.maybeSetContextMenu must be called after setPath().');
    return;
  }

  var isRoot = PathUtil.isRootPath(this.path_);
  var rootType = PathUtil.getRootType(this.path_);
  // The context menu is shown on the following items:
  // - Removable and Archive volumes
  // - Folder shortcuts
  if (!isRoot ||
      (rootType != RootType.DRIVE && rootType != RootType.DOWNLOADS))
    cr.ui.contextMenuHandler.setContextMenu(this, menu);
};

/**
 * A volume list.
 * @constructor
 * @extends {cr.ui.List}
 */
function VolumeList() {
}

/**
 * VolumeList inherits cr.ui.List.
 */
VolumeList.prototype.__proto__ = cr.ui.List.prototype;

/**
 * @param {HTMLElement} el Element to be DirectoryItem.
 * @param {DirectoryModel} directoryModel Current DirectoryModel.
 * @param {cr.ui.ArrayDataModel} pinnedFolderModel Current model of the pinned
 *     folders.
 */
VolumeList.decorate = function(el, directoryModel, pinnedFolderModel) {
  el.__proto__ = VolumeList.prototype;
  el.decorate(directoryModel, pinnedFolderModel);
};

/**
 * @param {DirectoryModel} directoryModel Current DirectoryModel.
 * @param {cr.ui.ArrayDataModel} pinnedFolderModel Current model of the pinned
 *     folders.
 */
VolumeList.prototype.decorate = function(directoryModel, pinnedFolderModel) {
  cr.ui.List.decorate(this);
  this.__proto__ = VolumeList.prototype;

  this.directoryModel_ = directoryModel;
  this.volumeManager_ = VolumeManager.getInstance();
  this.selectionModel = new cr.ui.ListSingleSelectionModel();

  this.directoryModel_.addEventListener('directory-changed',
      this.onCurrentDirectoryChanged_.bind(this));
  this.selectionModel.addEventListener(
      'change', this.onSelectionChange_.bind(this));
  this.selectionModel.addEventListener(
      'beforeChange', this.onBeforeSelectionChange_.bind(this));

  this.scrollBar_ = new ScrollBar();
  this.scrollBar_.initialize(this.parentNode, this);

  // Overriding default role 'list' set by cr.ui.List.decorate() to 'listbox'
  // role for better accessibility on ChromeOS.
  this.setAttribute('role', 'listbox');

  var self = this;
  this.itemConstructor = function(path) {
    return self.renderRoot_(path);
  };

  this.pinnedItemList_ = pinnedFolderModel;

  this.dataModel =
      new VolumeListModel(this.directoryModel_.getRootsList(),
                          this.pinnedItemList_);
};

/**
 * Creates an element of a volume. This method is called from cr.ui.List
 * internally.
 * @param {string} path Path of the directory to be rendered.
 * @return {VolumeItem} Rendered element.
 * @private
 */
VolumeList.prototype.renderRoot_ = function(path) {
  var item = new VolumeItem();
  item.setPath(path);

  var handleClick = function() {
    if (item.selected && path !== this.directoryModel_.getCurrentDirPath()) {
      this.directoryModel_.changeDirectory(path);
    }
  }.bind(this);
  item.addEventListener('click', handleClick);

  var handleEject = function() {
    var unmountCommand = cr.doc.querySelector('command#unmount');
    // Let's make sure 'canExecute' state of the command is properly set for
    // the root before executing it.
    unmountCommand.canExecuteChange(item);
    unmountCommand.execute(item);
  };
  item.addEventListener('eject', handleEject);
  // TODO(yoshiki): Check if the following touch handler is necessary or not.
  // If unnecessary, remove it.
  item.addEventListener(cr.ui.TouchHandler.EventType.TOUCH_START, handleClick);

  if (this.contextMenu_)
    item.maybeSetContextMenu(this.contextMenu_);

  // If the current directory is already set.
  if (this.currentVolume_ == path) {
    setTimeout(function() {
      this.selectedItem = path;
    }.bind(this), 0);
  }
  return item;
};

/**
 * Sets a context menu. Context menu is enabled only on archive and removable
 * volumes as of now.
 *
 * @param {cr.ui.Menu} menu Context menu.
 */
VolumeList.prototype.setContextMenu = function(menu) {
  this.contextMenu_ = menu;

  for (var i = 0; i < this.dataModel.length; i++) {
    this.getListItemByIndex(i).maybeSetContextMenu(this.contextMenu_);
  }
};

/**
 * Selects the n-th volume from the list.
 * @param {number} index Volume index.
 * @return {boolean} True for success, otherwise false.
 */
VolumeList.prototype.selectByIndex = function(index) {
  if (index < 0 || index > this.dataModel.length - 1)
    return false;

  var newPath = this.dataModel.item(index);
  if (!newPath)
    return false;

  // Prevents double-moving to the current directory.
  if (this.directoryModel_.getCurrentDirEntry().fullPath == newPath)
    return false;

  this.directoryModel_.changeDirectory(newPath);
  return true;
};

/**
 * Handler before root item change.
 * @param {Event} event The event.
 * @private
 */
VolumeList.prototype.onBeforeSelectionChange_ = function(event) {
  if (event.changes.length == 1 && !event.changes[0].selected)
    event.preventDefault();
};

/**
 * Handler for root item being clicked.
 * @param {Event} event The event.
 * @private
 */
VolumeList.prototype.onSelectionChange_ = function(event) {
  // This handler is invoked even when the volume list itself changes the
  // selection. In such case, we shouldn't handle the event.
  if (this.dontHandleSelectionEvent_)
    return;

  this.selectByIndex(this.selectionModel.selectedIndex);
};

/**
 * Invoked when the current directory is changed.
 * @param {Event} event The event.
 * @private
 */
VolumeList.prototype.onCurrentDirectoryChanged_ = function(event) {
  var path = event.newDirEntry.fullPath || this.dataModel.getCurrentDirPath();
  var newRootPath = PathUtil.getRootPath(path);

  // Synchronizes the volume list selection with the current directory, after
  // it is changed outside of the volume list.

  // (1) Select the nearest parent directory (including the pinned directories).
  var bestMatchIndex = -1;
  var bestMatchSubStringLen = 0;
  for (var i = 0; i < this.dataModel.length; i++) {
    var itemPath = this.dataModel.item(i);
    if (path.indexOf(itemPath) == 0) {
      if (bestMatchSubStringLen < itemPath.length) {
        bestMatchIndex = i;
        bestMatchSubStringLen = itemPath.length;
      }
    }
  }
  if (bestMatchIndex != -1) {
    // Not to invoke the handler of this instance, sets the guard.
    this.dontHandleSelectionEvent_ = true;
    this.selectionModel.selectedIndex = bestMatchIndex;
    this.dontHandleSelectionEvent_ = false;
    return;
  }

  // (2) Selects the volume of the current directory.
  for (var i = 0; i < this.dataModel.length; i++) {
    var itemPath = this.dataModel.item(i);
    if (PathUtil.getRootPath(itemPath) == newRootPath) {
      // Not to invoke the handler of this instance, sets the guard.
      this.dontHandleSelectionEvent_ = true;
      this.selectionModel.selectedIndex = i;
      this.dontHandleSelectionEvent_ = false;
      return;
    }
  }
};

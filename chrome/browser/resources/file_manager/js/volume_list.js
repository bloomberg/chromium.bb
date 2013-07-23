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
 * A volume list.
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
  this.currentVolume_ = null;


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
 * @return {HTMLElement} Rendered element.
 * @private
 */
VolumeList.prototype.renderRoot_ = function(path) {
  var li = cr.doc.createElement('li');
  li.className = 'root-item';
  li.setAttribute('role', 'option');
  var dm = this.directoryModel_;
  var handleClick = function() {
    if (li.selected && path !== dm.getCurrentDirPath()) {
      this.directoryModel_.changeDirectory(path);
    }
  }.bind(this);
  li.addEventListener('click', handleClick);
  li.addEventListener(cr.ui.TouchHandler.EventType.TOUCH_START, handleClick);

  var isRoot = PathUtil.isRootPath(path);
  var rootType = PathUtil.getRootType(path);

  var iconDiv = cr.doc.createElement('div');
  iconDiv.className = 'volume-icon';
  iconDiv.setAttribute('volume-type-icon', rootType);
  if (rootType === RootType.REMOVABLE) {
    iconDiv.setAttribute('volume-subtype',
        this.volumeManager_.getDeviceType(path));
  }
  li.appendChild(iconDiv);

  var div = cr.doc.createElement('div');
  div.className = 'root-label';

  div.textContent = PathUtil.getFolderLabel(path);
  li.appendChild(div);

  if (rootType === RootType.ARCHIVE || rootType === RootType.REMOVABLE) {
    var eject = cr.doc.createElement('div');
    eject.className = 'root-eject';
    eject.addEventListener('click', function(event) {
      event.stopPropagation();
      var unmountCommand = cr.doc.querySelector('command#unmount');
      // Let's make sure 'canExecute' state of the command is properly set for
      // the root before executing it.
      unmountCommand.canExecuteChange(li);
      unmountCommand.execute(li);
    }.bind(this));
    // Block other mouse handlers.
    eject.addEventListener('mouseup', function(e) { e.stopPropagation() });
    eject.addEventListener('mousedown', function(e) { e.stopPropagation() });
    li.appendChild(eject);
  }

  if (this.contextMenu_ && (!isRoot ||
      rootType != RootType.DRIVE && rootType != RootType.DOWNLOADS))
    cr.ui.contextMenuHandler.setContextMenu(li, this.contextMenu_);

  cr.defineProperty(li, 'lead', cr.PropertyKind.BOOL_ATTR);
  cr.defineProperty(li, 'selected', cr.PropertyKind.BOOL_ATTR);

  // If the current directory is already set.
  if (this.currentVolume_ == path) {
    setTimeout(function() {
      this.selectedItem = path;
    }.bind(this), 0);
  }

  return li;
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
    var path = this.dataModel.item(i);
    var itemType = this.dataModel.getItemType(i);
    var type = PathUtil.getRootType(path);
    // Context menu is set only to archive and removable volumes.
    if (itemType == VolumeListModel.ItemType.PINNED ||
        type == RootType.ARCHIVE || type == RootType.REMOVABLE) {
      cr.ui.contextMenuHandler.setContextMenu(this.getListItemByIndex(i),
                                              this.contextMenu_);
    }
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
  if (!newPath || this.currentVolume_ == newPath)
    return false;

  this.currentVolume_ = newPath;
  this.directoryModel_.changeDirectory(this.currentVolume_);
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

  // Sets |this.currentVolume_| in advance to prevent |onSelectionChange_()|
  // from calling |DirectoryModel.ChangeDirectory()| again.
  this.currentVolume_ = newRootPath;

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

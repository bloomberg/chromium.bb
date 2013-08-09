// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * A navigation list model. This model combines the 2 lists.
 * @param {cr.ui.ArrayDataModel} volumesList The first list of the model.
 * @param {cr.ui.ArrayDataModel} pinnedList The second list of the model.
 * @constructor
 * @extends {cr.EventTarget}
 */
function NavigationListModel(volumesList, pinnedList) {
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
 * NavigationList inherits cr.EventTarget.
 */
NavigationListModel.prototype = {
  __proto__: cr.EventTarget.prototype,
  get length() { return this.length_(); },
  get folderShortcutList() { return this.pinnedList_; }
};

/**
 * Returns the item at the given index.
 * @param {number} index The index of the entry to get.
 * @return {?string} The path at the given index.
 */
NavigationListModel.prototype.item = function(index) {
  var offset = this.volumesList_.length;
  if (index < offset) {
    var entry = this.volumesList_.item(index);
    return entry ? entry.fullPath : undefined;
  } else {
    return this.pinnedList_.item(index - offset);
  }
};

/**
 * Returns the number of items in the model.
 * @return {number} The length of the model.
 * @private
 */
NavigationListModel.prototype.length_ = function() {
  return this.volumesList_.length + this.pinnedList_.length;
};

/**
 * Returns the first matching item.
 * @param {Entry} item The entry to find.
 * @param {number=} opt_fromIndex If provided, then the searching start at
 *     the {@code opt_fromIndex}.
 * @return {number} The index of the first found element or -1 if not found.
 */
NavigationListModel.prototype.indexOf = function(item, opt_fromIndex) {
  for (var i = opt_fromIndex || 0; i < this.length; i++) {
    if (item === this.item(i))
      return i;
  }
  return -1;
};

/**
 * A navigation list item.
 * @constructor
 * @extends {HTMLLIElement}
 */
var NavigationListItem = cr.ui.define('li');

NavigationListItem.prototype = {
  __proto__: HTMLLIElement.prototype,
};

/**
 * Decorate the item.
 */
NavigationListItem.prototype.decorate = function() {
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
NavigationListItem.prototype.setPath = function(path) {
  if (this.path_)
    console.warn('NavigationListItem.setPath should be called only once.');

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
NavigationListItem.prototype.maybeSetContextMenu = function(menu) {
  if (!this.path_) {
    console.error('NavigationListItem.maybeSetContextMenu must be called ' +
                  'after setPath().');
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
 * A navigation list.
 * @constructor
 * @extends {cr.ui.List}
 */
function NavigationList() {
}

/**
 * NavigationList inherits cr.ui.List.
 */
NavigationList.prototype = {
  __proto__: cr.ui.List.prototype,

  set dataModel(dataModel) {
    if (!this.boundHandleListChanged_)
      this.boundHandleListChanged_ = this.onListContentChanged_.bind(this);

    if (this.dataModel_) {
      dataModel.removeEventListener('change', this.boundHandleListChanged_);
      dataModel.removeEventListener('permuted', this.boundHandleListChanged_);
    }

    dataModel.addEventListener('change', this.boundHandleListChanged_);
    dataModel.addEventListener('permuted', this.boundHandleListChanged_);

    var parentSetter = cr.ui.List.prototype.__lookupSetter__('dataModel');
    return parentSetter.call(this, dataModel);
  },

  get dataModel() {
    return this.dataModel_;
  },

  // TODO(yoshiki): Add a setter of 'directoryModel'.
};

/**
 * @param {HTMLElement} el Element to be DirectoryItem.
 * @param {DirectoryModel} directoryModel Current DirectoryModel.
 *     folders.
 */
NavigationList.decorate = function(el, directoryModel) {
  el.__proto__ = NavigationList.prototype;
  el.decorate(directoryModel);
};

/**
 * @param {DirectoryModel} directoryModel Current DirectoryModel.
 */
NavigationList.prototype.decorate = function(directoryModel) {
  cr.ui.List.decorate(this);
  this.__proto__ = NavigationList.prototype;

  this.fileManager_ = null;
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
};

/**
 * Creates an element of a navigation list. This method is called from
 * cr.ui.List internally.
 * @param {string} path Path of the directory to be rendered.
 * @return {NavigationListItem} Rendered element.
 * @private
 */
NavigationList.prototype.renderRoot_ = function(path) {
  var item = new NavigationListItem();
  item.setPath(path);

  var handleClick = function() {
    if (item.selected && path !== this.directoryModel_.getCurrentDirPath())
      this.changeDirectory_(path);
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

  if (this.contextMenu_)
    item.maybeSetContextMenu(this.contextMenu_);

  return item;
};

/**
 * Changes the current directory to the given path.
 * If the given path is not found, a 'shortcut-target-not-found' event is
 * fired.
 *
 * @param {string} path Path of the directory to be chagned to.
 * @private
 */
NavigationList.prototype.changeDirectory_ = function(path) {
  var onErrorCallback = function() {
    var e = new Event('shortcut-target-not-found');
    e.path = path;
    e.label = PathUtil.getFolderLabel(path);
    this.dispatchEvent(e);
  }.bind(this);

  this.directoryModel_.changeDirectory(path, onErrorCallback);
};

/**
 * Sets a context menu. Context menu is enabled only on archive and removable
 * volumes as of now.
 *
 * @param {cr.ui.Menu} menu Context menu.
 */
NavigationList.prototype.setContextMenu = function(menu) {
  this.contextMenu_ = menu;

  for (var i = 0; i < this.dataModel.length; i++) {
    this.getListItemByIndex(i).maybeSetContextMenu(this.contextMenu_);
  }
};

/**
 * Selects the n-th item from the list.
 * @param {number} index Item index.
 * @return {boolean} True for success, otherwise false.
 */
NavigationList.prototype.selectByIndex = function(index) {
  if (index < 0 || index > this.dataModel.length - 1)
    return false;

  var newPath = this.dataModel.item(index);
  if (!newPath)
    return false;

  // Prevents double-moving to the current directory.
  if (this.directoryModel_.getCurrentDirEntry().fullPath == newPath)
    return false;

  this.changeDirectory_(newPath);
  return true;
};

/**
 * Handler before root item change.
 * @param {Event} event The event.
 * @private
 */
NavigationList.prototype.onBeforeSelectionChange_ = function(event) {
  if (event.changes.length == 1 && !event.changes[0].selected)
    event.preventDefault();
};

/**
 * Handler for root item being clicked.
 * @param {Event} event The event.
 * @private
 */
NavigationList.prototype.onSelectionChange_ = function(event) {
  // This handler is invoked even when the navigation list itself changes the
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
NavigationList.prototype.onCurrentDirectoryChanged_ = function(event) {
  this.selectBestMatchItem_();
};

/**
 * Invoked when the content in the data model is changed.
 * @param {Event} event The event.
 * @private
 */
NavigationList.prototype.onListContentChanged_ = function(event) {
  this.selectBestMatchItem_();
};

/**
 * Synchronizes the volume list selection with the current directory, after
 * it is changed outside of the volume list.
 * @private
 */
NavigationList.prototype.selectBestMatchItem_ = function() {
  var entry = this.directoryModel_.getCurrentDirEntry();
  var path = entry && entry.fullPath;
  if (!path)
    return;

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
  var newRootPath = PathUtil.getRootPath(path);
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

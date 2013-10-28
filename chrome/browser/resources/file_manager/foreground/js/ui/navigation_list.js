// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * A navigation list item.
 * @constructor
 * @extends {HTMLLIElement}
 */
var NavigationListItem = cr.ui.define('li');

NavigationListItem.prototype = {
  __proto__: HTMLLIElement.prototype,
  get modelItem() { return this.modelItem_; }
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
 * @param {NavigationModelItem} modelItem NavigationModelItem of this item.
 * @param {string=} opt_deviceType The type of the device. Available iff the
 *     path represents removable storage.
 */
NavigationListItem.prototype.setModelItem =
    function(modelItem, opt_deviceType) {
  if (this.modelItem_)
    console.warn('NavigationListItem.setModelItem should be called only once.');

  this.modelItem_ = modelItem;

  var rootType = PathUtil.getRootType(modelItem.path);
  this.iconDiv_.setAttribute('volume-type-icon', rootType);
  if (opt_deviceType) {
    this.iconDiv_.setAttribute('volume-subtype', opt_deviceType);
  }

  this.label_.textContent = PathUtil.getFolderLabel(modelItem.path);

  if (rootType === RootType.ARCHIVE || rootType === RootType.REMOVABLE) {
    this.eject_ = cr.doc.createElement('div');
    // Block other mouse handlers.
    this.eject_.addEventListener(
        'mouseup', function(event) { event.stopPropagation() });
    this.eject_.addEventListener(
        'mousedown', function(event) { event.stopPropagation() });

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
  if (!this.modelItem_.path) {
    console.error('NavigationListItem.maybeSetContextMenu must be called ' +
                  'after setModelItem().');
    return;
  }

  var isRoot = PathUtil.isRootPath(this.modelItem_.path);
  var rootType = PathUtil.getRootType(this.modelItem_.path);
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
    if (!this.onListContentChangedBound_)
      this.onListContentChangedBound_ = this.onListContentChanged_.bind(this);

    if (this.dataModel_) {
      this.dataModel_.removeEventListener(
          'change', this.onListContentChangedBound_);
      this.dataModel_.removeEventListener(
          'permuted', this.onListContentChangedBound_);
    }

    var parentSetter = cr.ui.List.prototype.__lookupSetter__('dataModel');
    parentSetter.call(this, dataModel);

    // This must be placed after the parent method is called, in order to make
    // it sure that the list was changed.
    dataModel.addEventListener('change', this.onListContentChangedBound_);
    dataModel.addEventListener('permuted', this.onListContentChangedBound_);
  },

  get dataModel() {
    return this.dataModel_;
  },

  // TODO(yoshiki): Add a setter of 'directoryModel'.
};

/**
 * @param {HTMLElement} el Element to be DirectoryItem.
 * @param {VolumeManagerWrapper} volumeManager The VolumeManager of the system.
 * @param {DirectoryModel} directoryModel Current DirectoryModel.
 *     folders.
 */
NavigationList.decorate = function(el, volumeManager, directoryModel) {
  el.__proto__ = NavigationList.prototype;
  el.decorate(volumeManager, directoryModel);
};

/**
 * @param {VolumeManagerWrapper} volumeManager The VolumeManager of the system.
 * @param {DirectoryModel} directoryModel Current DirectoryModel.
 */
NavigationList.prototype.decorate = function(volumeManager, directoryModel) {
  cr.ui.List.decorate(this);
  this.__proto__ = NavigationList.prototype;

  this.directoryModel_ = directoryModel;
  this.volumeManager_ = volumeManager;
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
  this.itemConstructor = function(modelItem) {
    return self.renderRoot_(modelItem);
  };
};

/**
 * This overrides cr.ui.List.measureItem().
 * In the method, a temporary element is added/removed from the list, and we
 * need to omit animations for such temporary items.
 *
 * @param {ListItem=} opt_item The list item to be measured.
 * @return {{height: number, marginTop: number, marginBottom:number,
 *     width: number, marginLeft: number, marginRight:number}} Size.
 * @override
 */
NavigationList.prototype.measureItem = function(opt_item) {
  this.measuringTemporaryItemNow_ = true;
  var result = cr.ui.List.prototype.measureItem.call(this, opt_item);
  this.measuringTemporaryItemNow_ = false;
  return result;
};

/**
 * Creates an element of a navigation list. This method is called from
 * cr.ui.List internally.
 *
 * @param {NavigationModelItem} modelItem NavigationModelItem to be rendered.
 * @return {NavigationListItem} Rendered element.
 * @private
 */
NavigationList.prototype.renderRoot_ = function(modelItem) {
  var item = new NavigationListItem();
  var volumeInfo =
      PathUtil.isRootPath(modelItem.path) &&
      this.volumeManager_.getVolumeInfo(modelItem.path);
  item.setModelItem(modelItem, volumeInfo && volumeInfo.deviceType);

  var handleClick = function() {
    if (item.selected &&
        modelItem.path !== this.directoryModel_.getCurrentDirPath()) {
      metrics.recordUserAction('FolderShortcut.Navigate');
      this.changeDirectory_(modelItem);
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

  if (this.contextMenu_)
    item.maybeSetContextMenu(this.contextMenu_);

  return item;
};

/**
 * Changes the current directory to the given path.
 * If the given path is not found, a 'shortcut-target-not-found' event is
 * fired.
 *
 * @param {NavigationModelItem} modelItem Directory to be chagned to.
 * @private
 */
NavigationList.prototype.changeDirectory_ = function(modelItem) {
  var onErrorCallback = function(error) {
    if (error.code === FileError.NOT_FOUND_ERR)
      this.dataModel.onItemNotFoundError(modelItem);
  }.bind(this);

  this.directoryModel_.changeDirectory(modelItem.path, onErrorCallback);
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
 *
 * @param {number} index Item index.
 * @return {boolean} True for success, otherwise false.
 */
NavigationList.prototype.selectByIndex = function(index) {
  if (index < 0 || index > this.dataModel.length - 1)
    return false;

  var newModelItem = this.dataModel.item(index);
  var newPath = newModelItem.path;
  if (!newPath)
    return false;

  // Prevents double-moving to the current directory.
  // eg. When user clicks the item, changing directory has already been done in
  //     click handler.
  var entry = this.directoryModel_.getCurrentDirEntry();
  if (entry && entry.fullPath == newPath)
    return false;

  metrics.recordUserAction('FolderShortcut.Navigate');
  this.changeDirectory_(newModelItem);
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

  // (1) Select the nearest parent directory (including the shortcut folders).
  var bestMatchIndex = -1;
  var bestMatchSubStringLen = 0;
  for (var i = 0; i < this.dataModel.length; i++) {
    var itemPath = this.dataModel.item(i).path;
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
    var itemPath = this.dataModel.item(i).path;
    if (PathUtil.getRootPath(itemPath) == newRootPath) {
      // Not to invoke the handler of this instance, sets the guard.
      this.dontHandleSelectionEvent_ = true;
      this.selectionModel.selectedIndex = i;
      this.dontHandleSelectionEvent_ = false;
      return;
    }
  }
};

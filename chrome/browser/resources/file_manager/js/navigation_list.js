// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Entry of NavigationListModel. This construtor should be called only from
 * the helper methods (NavigationModelItem.createWithPath/createWithEntry).
 *
 * @param {FileSystem} filesystem Fielsystem.
 * @param {?string} path Path.
 * @param {DirectoryEntry} entry Entry. Can be a fake.
 * @extends {cr.EventTarget}
 * @constructor
 */
function NavigationModelItem(filesystem, path, entry) {
  this.filesystem_ = filesystem;
  this.path_ = path;
  this.entry_ = entry;
  this.fileError_ = null;

  this.resolvingQueue_ = new AsyncUtil.Queue();

  Object.seal(this);
}

NavigationModelItem.prototype = {
  get path() { return this.path_; },
  get fileError() { return this.fileError_; }
};

/**
 * Returns the cached entry of the item. This may returns NULL if the target is
 * not available on the filesystem, is not resolved or is under resolving the
 * entry.
 *
 * @return {Entry} Cached entry.
 */
NavigationModelItem.prototype.getCachedEntry = function() {
  return this.entry_;
};

/**
 * @param {FileSystem} filesystem FileSystem.
 * @param {string} path Path.
 * @param {function(NavigationModelItem)} callback Called when the resolving is
 *     success/failed with the created NavigationModelItem.
 * @return {NavigationModelItem} Created NavigationModelItem.
 */
NavigationModelItem.createWithPath = function(filesystem, path, callback) {
  var modelItem = new NavigationModelItem(
      filesystem,
      path,
      null);
  modelItem.resolveDirectoryEntry_(function() {
    callback(modelItem);
  });
  return modelItem;
};

/**
 * @param {DirectoryEntry} entry Entry. Can be a fake.
 * @return {NavigationModelItem} Created NavigationModelItem.
 */
NavigationModelItem.createWithEntry = function(entry) {
  if (entry) {
    return new NavigationModelItem(
        entry.filesystem,
        entry.fullPath,
        entry);
  } else {
    return null;
  }
};

/**
 * Retrieves the entry. If the entry is being retrived, wait until it ends. If
 * it is nessesary to resolve entry from the path, resolveDirectoryEntry must be
 * called before using this method.
 *
 * @param {function(Entry)} callback Called with the resolved entry. The entry
 *     may be NULL if resolving is failed.
 */
NavigationModelItem.prototype.getEntryAsync = function(callback) {
  // If resolveDirectoryEntry_() is running, waits until it finishes.
  this.resolvingQueue_.run(function(continueCallback) {
    callback(this.entry_);
    continueCallback();
  }.bind(this));
};

/**
 * Resolves an directory entry.
 * @param {function()} callback Called when the resolving is success/failed.
 * @private
 */
NavigationModelItem.prototype.resolveDirectoryEntry_ = function(callback) {
  this.resolvingQueue_.run(function(continueCallback) {
    this.filesystem_.root.getDirectory(
        this.path_,
        {create: false},
        function(directoryEntry) {
          this.entry_ = directoryEntry;
          callback();
          continueCallback();
        }.bind(this),
        function(error) {
          this.entry_ = null;
          this.fileError_ = error;
          console.error('Error on resolving path: ' + this.path_);
          callback();
          continueCallback();
        }.bind(this));
  }.bind(this));
};

/**
 * Returns if this item is a shortcut or a volume root.
 * @return {boolean} True if a shortcut, false if a volume root.
 */
NavigationModelItem.prototype.isShortcut = function() {
  return !PathUtil.isRootPath(this.path_);
};

/**
 * A navigation list model. This model combines the 2 lists.
 * @param {FileSystem} filesystem FileSystem.
 * @param {cr.ui.ArrayDataModel} volumesList The first list of the model.
 * @param {cr.ui.ArrayDataModel} shortcutList The second list of the model.
 * @constructor
 * @extends {cr.EventTarget}
 */
function NavigationListModel(filesystem, volumesList, shortcutList) {
  this.volumesListModel_ = volumesList;
  this.shortcutListModel_ = shortcutList;

  // On default, shortcut list is disabled. It may be enabled on initializetion.
  this.showShortcuts_ = false;

  var entryToModelItem = function(entry) {
    return NavigationModelItem.createWithEntry(entry);
  };
  var pathToModelItem = function(path) {
    return NavigationModelItem.createWithPath(filesystem, path,
        function(modelItem) {
          if (!modelItem.getCachedEntry() &&
              modelItem.fileError &&
              modelItem.fileError.code === FileError.NOT_FOUND_ERR) {
            this.onItemNotFoundError(modelItem);
          }
        }.bind(this));
  }.bind(this);

  /**
   * Type of updated list.
   * @enum {number}
   * @const
   */
  var ListType = {
    VOLUME_LIST: 1,
    SHORTCUT_LIST: 2
  };
  Object.freeze(ListType);

  // Generates this.volumesList_ and this.shortcutList_ from the models.
  this.volumesList_ = this.volumesListModel_.slice(0).map(entryToModelItem);
  this.shortcutList_ = this.shortcutListModel_.slice(0).map(pathToModelItem);

  // Generates a combined 'permuted' event from an event of either list.
  var permutedHandler = function(listType, event) {
    var permutedEvent = new Event('permuted');
    var newPermutation = [];
    var newLength;
    if (listType == ListType.VOLUME_LIST) {
      // Creates new permutation array.
      newLength = event.newLength;
      for (var i = 0; i < event.permutation.length; i++) {
        newPermutation[i] = event.permutation[i];
      }
      if (this.showShortcuts_) {
        newLength += this.shortcutList_.length;
        for (var i = 0; i < this.shortcutList_.length; i++) {
          newPermutation[i + event.permutation.length] = i + event.newLength;
        }
      }

      // Updates the list.
      var newList = [];
      for (var i = 0; i < event.permutation.length; i++) {
        newList[event.permutation[i]] = this.volumesList_[i];
      }
      for (var i = 0; i < event.newLength; i++) {
        if (!newList[i])
          newList[i] = entryToModelItem(this.volumesListModel_.item(i));
      }
      this.volumesList_ = newList;
    } else {
      // Creates new permutation array.
      var volumesLen = this.volumesList_.length;
      newLength = volumesLen;
      for (var i = 0; i < volumesLen; i++) {
        newPermutation[i] = i;
      }
      if (this.showShortcuts_) {
        newLength += event.newLength;
        for (var i = 0; i < event.permutation.length; i++) {
          newPermutation[i + volumesLen] = (event.permutation[i] !== -1) ?
                                           (event.permutation[i] + volumesLen) :
                                           -1;
        }
      }

      // Updates the list.
      var newList = [];
      for (var i = 0; i < event.permutation.length; i++) {
        newList[event.permutation[i]] = this.shortcutList_[i];
      }
      for (var i = 0; i < event.newLength; i++) {
        if (!newList[i])
          newList[i] = pathToModelItem(this.shortcutListModel_.item(i));
      }
      this.shortcutList_ = newList;
    }

    permutedEvent.newLength = newLength;
    permutedEvent.permutation = newPermutation;
    this.dispatchEvent(permutedEvent);
  };
  this.volumesListModel_.addEventListener(
      'permuted', permutedHandler.bind(this, ListType.VOLUME_LIST));
  this.shortcutListModel_.addEventListener(
      'permuted', permutedHandler.bind(this, ListType.SHORTCUT_LIST));

  // Generates a combined 'change' event from an event of either list.
  var changeHandler = function(listType, event) {
    var i = event.index;

    // Updates the list.
    if (listType == ListType.VOLUME_LIST)
      this.volumesList_[i] = entryToModelItem(this.volumesListModel_.item(i));
    else
      this.shortcutList_[i] = pathToModelItem(this.shortcutListModel_.item(i));

    if (this.showShortcuts_ && listType == ListType.SHORTCUT_LIST)
      return;

    var changeEvent = new Event('change');
    changeEvent.index =
        (listType == ListType.VOLUME_LIST) ? i : (i + this.volumesList_.length);
    this.dispatchEvent(changeEvent);
  };
  this.volumesListModel_.addEventListener(
      'change', changeHandler.bind(this, ListType.VOLUME_LIST));
  this.shortcutListModel_.addEventListener(
      'change', changeHandler.bind(this, ListType.SHORTCUT_LIST));

  // 'splice' and 'sorted' events are not implemented, since they are not used
  // in list.js.
}

/**
 * NavigationList inherits cr.EventTarget.
 */
NavigationListModel.prototype = {
  __proto__: cr.EventTarget.prototype,
  get length() { return this.length_(); },
  get folderShortcutList() { return this.shortcutList_; }
};

/**
 * Returns the item at the given index.
 * @param {number} index The index of the entry to get.
 * @return {?string} The path at the given index.
 */
NavigationListModel.prototype.item = function(index) {
  var offset = this.volumesList_.length;
  if (index < offset)
    return this.volumesList_[index];
  if (this.showShortcuts_)
    return this.shortcutList_[index - offset];
  return null;
};

/**
 * Show/hide the shortcut list.
 * @param {boolean} show True to show, false otherwise.
 */
NavigationListModel.prototype.showShortcuts = function(show) {
  if (this.showShortcuts_ == show)
    return;

  this.showShortcuts_ = show;

  if (show) {
    var permutedEvent = new Event('permuted');
    var permutation = [];
    for (var i = 0; i < this.volumesList_.length; i++) {
      permutation[i] = i;
    }
    permutedEvent.newLength =
        this.volumesList_.length + this.shortcutList_.length;
    permutedEvent.permutation = permutation;
    this.dispatchEvent(permutedEvent);
  } else {
    var permutedEvent = new Event('permuted');
    var permutation = [];
    for (var i = 0; i < this.volumesList_.length; i++) {
      permutation[i] = i;
    }
    var offset = this.volumesList_.length;
    for (var i = 0; i < this.shortcutList_.length; i++) {
      permutation[i + offset] = -1;
    }
    permutedEvent.newLength = this.volumesList_.length;
    permutedEvent.permutation = permutation;
    this.dispatchEvent(permutedEvent);
  }
};

/**
 * Returns the number of items in the model.
 * @return {number} The length of the model.
 * @private
 */
NavigationListModel.prototype.length_ = function() {
  var length = this.volumesList_.length;
  if (this.showShortcuts_)
    length += this.shortcutList_.length;
  return length;
};

/**
 * Returns the first matching item.
 * @param {NavigationModelItem} modelItem The entry to find.
 * @param {number=} opt_fromIndex If provided, then the searching start at
 *     the {@code opt_fromIndex}.
 * @return {number} The index of the first found element or -1 if not found.
 */
NavigationListModel.prototype.indexOf = function(modelItem, opt_fromIndex) {
  for (var i = opt_fromIndex || 0; i < this.length; i++) {
    if (modelItem === this.item(i))
      return i;
  }
  return -1;
};

/**
 * Called when one od the items is not found on the filesystem.
 * @param {NavigationModelItem} modelItem The entry which is not found.
 */
NavigationListModel.prototype.onItemNotFoundError = function(modelItem) {
  var index = this.indexOf(modelItem);
  if (index === -1) {
    // Invalid modelItem.
  } else if (index < this.volumesList_.length) {
    // The item is in the volume list.
    // Not implemented.
    // TODO(yoshiki): Implement it when necessary.
  } else {
    // The item is in the folder shortcut list.
    if (this.isDriveMounted())
      this.shortcutListModel_.remove(modelItem.path);
  }
};

/**
 * Returns if the drive is mounted or not.
 * @return {boolean} True if the drive is mounted, false otherwise.
 */
NavigationListModel.prototype.isDriveMounted = function() {
  for (var i = 0; i < this.volumesList_.length; i++) {
    if (PathUtil.isDriveBasedPath(this.item(i).path))
      return true;
  }
  return false;
};

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
 * Add/remove fade-in animation.
 * @param {boolean} animation True if adding animation, false if removing.
 */
NavigationListItem.prototype.setFadeinAnimation = function(animation) {
  this.classList.toggle('fadein', animation);
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
 * @param {VolumeManager} volumeManager The VolumeManager of the system.
 * @param {DirectoryModel} directoryModel Current DirectoryModel.
 *     folders.
 */
NavigationList.decorate = function(el, volumeManager, directoryModel) {
  el.__proto__ = NavigationList.prototype;
  el.decorate(volumeManager, directoryModel);
};

/**
 * @param {VolumeManager} volumeManager The VolumeManager of the system.
 * @param {DirectoryModel} directoryModel Current DirectoryModel.
 */
NavigationList.prototype.decorate = function(volumeManager, directoryModel) {
  cr.ui.List.decorate(this);
  this.__proto__ = NavigationList.prototype;

  this.fileManager_ = null;
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
 * This overrides Node.removeChild().
 * Instead of just removing the given child, this method adds a fade-out
 * animation and removes the child after animation completes.
 *
 * @param {NavigationListItem} item List item to be removed.
 * @override
 */
NavigationList.prototype.removeChild = function(item) {
  // TODO(yoshiki): Animation is temporary disabled for volumes. Add animation
  // back. crbug.com/276132.
  if (!item.modelItem.isShortcut() || this.measuringTemporaryItemNow_) {
    Node.prototype.removeChild.call(this, item);
    return;
  }

  var removeElement = function(event) {
    // Must keep the animation name 'fadeOut' in sync with the css.
    if (event.animationName == 'fadeOut')
      // Checks if the element is still alive on the DOM tree.
      if (item.parentElement && item.parentElement == this)
        Node.prototype.removeChild.call(this, item);
  }.bind(this);

  item.addEventListener('webkitAnimationEnd', removeElement, false);
  // Must keep the class name 'fadeout' in sync with the css.
  item.classList.add('fadeout');
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

  // TODO(yoshiki): Animation is temporary disabled for volumes. Add animation
  // back. crbug.com/276132.
  item.setFadeinAnimation(modelItem.isShortcut() &&
                          !this.measuringTemporaryItemNow_);

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
  if (this.directoryModel_.getCurrentDirEntry().fullPath == newPath)
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

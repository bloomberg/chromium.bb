// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Utility methods. They are intended for use only in this file.
 */
var DirectoryTreeUtil = {};

/**
 * Updates sub-elements of {@code parentElement} reading {@code DirectoryEntry}
 * with calling {@code iterator}.
 *
 * @param {string} changedDirectryPath The path of the changed directory.
 * @param {DirectoryItem|DirectoryTree} currentDirectoryItem An item to be
 *     started traversal from.
 */
DirectoryTreeUtil.updateChangedDirectoryItem = function(
    changedDirectryPath, currentDirectoryItem) {
  for (var i = 0; i < currentDirectoryItem.items.length; i++) {
    var item = currentDirectoryItem.items[i];
    if (changedDirectryPath === item.entry.fullPath) {
      item.updateSubDirectories();
      break;
    } else if (changedDirectryPath.indexOf(item.entry.fullPath) == 0) {
      DirectoryTreeUtil.updateChangedDirectoryItem(changedDirectryPath, item);
    }
  }
};

/**
 * Updates sub-elements of {@code parentElement} reading {@code DirectoryEntry}
 * with calling {@code iterator}.
 *
 * @param {DirectoryItem|DirectoryTree} parentElement Parent element of newly
 *     created items.
 * @param {function(number): DirectoryEntry} iterator Function which returns
 *     the n-th Entry in the directory.
 * @param {DirectoryModel} directoryModel Current DirectoryModel.
 * @param {boolean} recursive True if the update is recursively.
 */
DirectoryTreeUtil.updateSubElementsFromList = function(
    parentElement, iterator, directoryModel, recursive) {
  var index = 0;
  while (iterator(index)) {
    var currentEntry = iterator(index);
    var currentElement = parentElement.items[index];

    if (index >= parentElement.items.length) {
      var item = new DirectoryItem(currentEntry, parentElement, directoryModel);
      parentElement.add(item);
      index++;
    } else if (currentEntry.fullPath == currentElement.fullPath) {
      if (recursive && parentElement.expanded)
        currentElement.updateSubDirectories(true /* recursive */);

      index++;
    } else if (currentEntry.fullPath < currentElement.fullPath) {
      var item = new DirectoryItem(currentEntry, parentElement, directoryModel);
      parentElement.addAt(item, index);
      index++;
    } else if (currentEntry.fullPath > currentElement.fullPath) {
      parentElement.remove(currentElement);
    }
  }

  var removedChild;
  while (removedChild = parentElement.items[index]) {
    parentElement.remove(removedChild);
  }

  if (index == 0) {
    parentElement.hasChildren = false;
    parentElement.expanded = false;
  } else {
    parentElement.hasChildren = true;
  }
};

/**
 * Returns true if the given directory entry is dummy.
 * @param {DirectoryEntry|Object} dirEntry DirectoryEntry to be checked.
 * @return {boolean} True if the given directory entry is dummy.
 */
DirectoryTreeUtil.isDummyEntry = function(dirEntry) {
  return !('createReader' in dirEntry);
};

/**
 * A directory in the tree. Each element represents one directory.
 *
 * @param {DirectoryEntry} dirEntry DirectoryEntry of this item.
 * @param {DirectoryItem|DirectoryTree} parentDirItem Parent of this item.
 * @param {DirectoryModel} directoryModel Current DirectoryModel.
 * @extends {cr.ui.TreeItem}
 * @constructor
 */
function DirectoryItem(dirEntry, parentDirItem, directoryModel) {
  var item = cr.doc.createElement('div');
  DirectoryItem.decorate(item, dirEntry, parentDirItem, directoryModel);
  return item;
}

/**
 * @param {HTMLElement} el Element to be DirectoryItem.
 * @param {DirectoryEntry} dirEntry DirectoryEntry of this item.
 * @param {DirectoryItem|DirectoryTree} parentDirItem Parent of this item.
 * @param {DirectoryModel} directoryModel Current DirectoryModel.
 */
DirectoryItem.decorate =
    function(el, dirEntry, parentDirItem, directoryModel) {
  el.__proto__ = DirectoryItem.prototype;
  (/** @type {DirectoryItem} */ el).decorate(
      dirEntry, parentDirItem, directoryModel);
};

DirectoryItem.prototype = {
  __proto__: cr.ui.TreeItem.prototype,

  /**
   * The DirectoryEntry corresponding to this DirectoryItem. This may be
   * a dummy DirectoryEntry.
   * @type {DirectoryEntry|Object}
   * @override
   **/
  get entry() {
      return this.dirEntry_;
  },

  /**
   * The element containing the label text and the icon.
   * @type {!HTMLElement}
   * @override
   **/
  get labelElement() {
      return this.firstElementChild.querySelector('.label');
  }
};

/**
 * @param {DirectoryEntry} dirEntry DirectoryEntry of this item.
 * @param {DirectoryItem|DirectoryTree} parentDirItem Parent of this item.
 * @param {DirectoryModel} directoryModel Current DirectoryModel.
 */
DirectoryItem.prototype.decorate = function(
    dirEntry, parentDirItem, directoryModel) {
  var path = dirEntry.fullPath;
  var label = PathUtil.isRootPath(path) ?
              PathUtil.getRootLabel(path) : dirEntry.name;

  this.className = 'tree-item';
  this.innerHTML =
      '<div class="tree-row">' +
      ' <span class="expand-icon"></span>' +
      ' <span class="icon"></span>' +
      ' <span class="label"></span>' +
      ' <div class="root-eject"></div>' +
      '</div>' +
      '<div class="tree-children"></div>';
  this.setAttribute('role', 'treeitem');

  this.directoryModel_ = directoryModel;
  this.parent_ = parentDirItem;
  this.label = label;
  this.fullPath = path;
  this.dirEntry_ = dirEntry;
  this.fileFilter_ = this.directoryModel_.getFileFilter();

  // Sets hasChildren=true tentatively. This will be overridden after
  // scanning sub-directories in DirectoryTreeUtil.updateSubElementsFromList.
  // Special search does not have children.
  this.hasChildren = !PathUtil.isSpecialSearchRoot(path);

  this.addEventListener('expand', this.onExpand_.bind(this), false);
  var volumeManager = VolumeManager.getInstance();
  var icon = this.querySelector('.icon');
  if (PathUtil.isRootPath(path)) {
    icon.classList.add('volume-icon');
    var iconType = PathUtil.getRootType(path);
    icon.setAttribute('volume-type-icon', iconType);

    if (iconType == RootType.REMOVABLE)
      icon.setAttribute('volume-subtype', volumeManager.getDeviceType(path));
  }

  var eject = this.querySelector('.root-eject');
  eject.hidden = !PathUtil.isUnmountableByUser(path);
  eject.addEventListener('click',
      function(event) {
        event.stopPropagation();
        if (!PathUtil.isUnmountableByUser(path))
          return;

        volumeManager.unmount(path, function() {}, function() {});
      }.bind(this));

  if (parentDirItem.expanded)
    this.updateSubDirectories(false /* recursive */);
};

/**
 * Invoked when the item is being expanded.
 * @param {!UIEvent} e Event.
 * @private
 **/
DirectoryItem.prototype.onExpand_ = function(e) {
  this.updateSubDirectories(
      false /* recursive */,
      function() {
        this.expanded = false;
      }.bind(this));

  e.stopPropagation();
};

/**
 * Retrieves the latest subdirectories and update them on the tree.
 * @param {boolean} recursive True if the update is recursively.
 * @param {function()=} opt_errorCallback Callback called on error.
 */
DirectoryItem.prototype.updateSubDirectories = function(
    recursive, opt_errorCallback) {
  // Tries to retrieve new entry if the cached entry is dummy.
  if (DirectoryTreeUtil.isDummyEntry(this.dirEntry_)) {
    // Fake Drive root.
    this.directoryModel_.resolveDirectory(
        this.fullPath,
        function(entry) {
          this.dirEntry_ = entry;

          // If the retrieved entry is dummy again, returns with an error.
          if (DirectoryTreeUtil.isDummyEntry(entry)) {
            if (opt_errorCallback)
              opt_errorCallback();
            return;
          }

          this.updateSubDirectories(recursive, opt_errorCallback);
        }.bind(this),
        opt_errorCallback);
    return;
  }

  var reader = this.dirEntry_.createReader();
  var entries = [];

  var readEntry = function() {
    reader.readEntries(function(results) {
      if (!results.length) {
        this.entries_ = entries.sort();
        this.redrawSubDirectoryList_(recursive);
        return;
      }

      for (var i = 0; i < results.length; i++) {
        var entry = results[i];
        if (entry.isDirectory)
          entries.push(entry);
      }
      readEntry();
    }.bind(this));
  }.bind(this);
  readEntry();
};

/**
 * Redraw subitems with the latest information. The items are sorted in
 * alphabetical order, case insensitive.
 * @param {boolean} recursive True if the update is recursively.
 * @private
 */
DirectoryItem.prototype.redrawSubDirectoryList_ = function(recursive) {
  var entries = this.entries_.
      sort(function(a, b) {
        return a.name.toLowerCase() > b.name.toLowerCase();
      }).
      filter(this.fileFilter_.filter.bind(this.fileFilter_));

  DirectoryTreeUtil.updateSubElementsFromList(
      this,
      function(i) { return entries[i]; },
      this.directoryModel_,
      recursive);
};

/**
 * Executes the assigned action as a drop target.
 */
DirectoryItem.prototype.doDropTargetAction = function() {
  this.expanded = true;
};

/**
 * Executes the assigned action. DirectoryItem performs changeDirectory.
 */
DirectoryItem.prototype.doAction = function() {
  this.directoryModel_.changeDirectory(this.fullPath);
};

/**
 * Tree of directories on the sidebar. This element is also the root of items,
 * in other words, this is the parent of the top-level items.
 *
 * @constructor
 * @extends {cr.ui.Tree}
 */
function DirectoryTree() {}

/**
 * Decorates an element.
 * @param {HTMLElement} el Element to be DirectoryTree.
 * @param {DirectoryModel} directoryModel Current DirectoryModel.
 */
DirectoryTree.decorate = function(el, directoryModel) {
  el.__proto__ = DirectoryTree.prototype;
  (/** @type {DirectoryTree} */ el).decorate(directoryModel);
};

DirectoryTree.prototype = {
  __proto__: cr.ui.Tree.prototype,

  // DirectoryTree is always expanded.
  get expanded() { return true; },
  /**
   * @param {boolean} value Not used.
   */
  set expanded(value) {}
};

/**
 * Decorates an element.
 * @param {DirectoryModel} directoryModel Current DirectoryModel.
 */
DirectoryTree.prototype.decorate = function(directoryModel) {
  cr.ui.Tree.prototype.decorate.call(this);

  this.directoryModel_ = directoryModel;

  this.fileFilter_ = this.directoryModel_.getFileFilter();
  this.fileFilter_.addEventListener('changed',
                                    this.onFilterChanged_.bind(this));

  this.rootsList_ = this.directoryModel_.getRootsList();
  this.rootsList_.addEventListener('change',
                                   this.onRootsListChanged_.bind(this));
  this.rootsList_.addEventListener('permuted',
                                   this.onRootsListChanged_.bind(this));
  this.onRootsListChanged_();

  // Add a handler for directory change.
  this.addEventListener('change', function() {
    if (this.selectedItem) {
      this.selectedItem.doAction();
      return;
    }

    // Fallback to the default directory.
    this.directoryModel_.changeDirectory(
        this.directoryModel_.getDefaultDirectory());
  }.bind(this));

  this.privateOnDirectoryChangedBound_ = this.onDirectoryChanged_.bind(this);
  chrome.fileBrowserPrivate.onDirectoryChanged.addListener(
      this.privateOnDirectoryChangedBound_);
};

/**
 * Sets a context menu. Context menu is enabled only on archive and removable
 * volumes as of now.
 *
 * @param {cr.ui.Menu} menu Context menu.
 */
DirectoryTree.prototype.setContextMenu = function(menu) {
  this.contextMenu_ = menu;

  for (var i = 0; i < this.rootsList_.length; i++) {
    var item = this.rootsList_.item(i);
    var type = PathUtil.getRootType(item.fullPath);
    // Context menu is set only to archive and removable volumes.
    if (type == RootType.ARCHIVE || type == RootType.REMOVABLE) {
      cr.ui.contextMenuHandler.setContextMenu(this.items[i].rowElement,
                                              this.contextMenu_);
    }
  }
};

/**
 * Invoked when the root list is changed.
 * @private
 */
DirectoryTree.prototype.onRootsListChanged_ = function() {
  this.redraw(false /* recursive */);
};

/**
 * Redraw the list.
 * @param {boolean} recursive True if the update is recursively. False if the
 *     only root items are updated.
 */
DirectoryTree.prototype.redraw = function(recursive) {
  var rootsList = this.rootsList_;
  DirectoryTreeUtil.updateSubElementsFromList(this,
                                              rootsList.item.bind(rootsList),
                                              this.directoryModel_,
                                              recursive);
  this.setContextMenu(this.contextMenu_);
};

/**
 * Invoked when the filter is changed.
 * @private
 */
DirectoryTree.prototype.onFilterChanged_ = function() {
  this.redraw(true /* recursive */);
};

/**
 * Invoked when a directory is changed.
 * @param {!UIEvent} event Event.
 * @private
 */
DirectoryTree.prototype.onDirectoryChanged_ = function(event) {
  if (event.eventType == 'changed') {
    var path = util.extractFilePath(event.directoryUrl);
    DirectoryTreeUtil.updateChangedDirectoryItem(path, this);
  }
};

/**
 * Returns the path of the selected item.
 * @return {string} The current path.
 */
DirectoryTree.prototype.getCurrentPath = function() {
  return this.selectedItem ? this.selectedItem.fullPath : null;
};

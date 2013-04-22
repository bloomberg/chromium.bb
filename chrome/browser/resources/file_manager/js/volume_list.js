// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

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
 */
VolumeList.decorate = function(el, directoryModel) {
  el.__proto__ = VolumeList.prototype;
  el.decorate(directoryModel);
};

/**
 * @param {DirectoryModel} directoryModel Current DirectoryModel.
 */
VolumeList.prototype.decorate = function(directoryModel) {
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

  // Overriding default role 'list' set by cr.ui.List.decorate() to 'listbox'
  // role for better accessibility on ChromeOS.
  this.setAttribute('role', 'listbox');

  var self = this;
  this.itemConstructor = function(entry) {
    return self.renderRoot_(entry);
  };

  //this.rootsList_.selectionModel =
  //    this.directoryModel_.getRootsListSelectionModel();
  this.dataModel = this.directoryModel_.getRootsList();
};

/**
 * Creates an element of a volume. This method is called from cr.ui.List
 * internally.
 * @param {DirectoryEntry} entry Entry of the directory to be rendered.
 * @return {HTMLElement} Rendered element.
 * @private
 */
VolumeList.prototype.renderRoot_ = function(entry) {
  var path = entry.fullPath;
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

  div.textContent = PathUtil.getRootLabel(path);
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

  if (this.contextMenu_ &&
      rootType != RootType.DRIVE && rootType != RootType.DOWNLOADS)
    cr.ui.contextMenuHandler.setContextMenu(li, this.contextMenu_);

  cr.defineProperty(li, 'lead', cr.PropertyKind.BOOL_ATTR);
  cr.defineProperty(li, 'selected', cr.PropertyKind.BOOL_ATTR);

  // If the current directory is already set.
  if (this.currentVolume_ == path) {
    setTimeout(function() {
      this.selectedItem = entry;
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
    var item = this.rootsList_.item(i);
    var type = PathUtil.getRootType(item.fullPath);
    // Context menu is set only to archive and removable volumes.
    if (type == RootType.ARCHIVE || type == RootType.REMOVABLE) {
      cr.ui.contextMenuHandler.setContextMenu(this.getListItemByIndex(i),
                                              this.contextMenu_);
    }
  }
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
  var newRootDir = this.dataModel.item(this.selectionModel.selectedIndex);
  if (newRootDir && this.currentVolume_ != newRootDir.fullPath) {
    this.currentVolume_ = newRootDir.fullPath;
    this.directoryModel_.changeDirectory(this.currentVolume_);
  }
};

/**
 * Invoked when the current directory is changed.
 * @param {Event} event The event.
 * @private
 */
VolumeList.prototype.onCurrentDirectoryChanged_ = function(event) {
  var path = event.newDirEntry.fullPath || dm.getCurrentDirPath();
  var newRootPath = PathUtil.getRootPath(path);

  // Sets |this.currentVolume_| in advance to prevent |onSelectionChange_()|
  // from calling |DirectoryModel.ChangeDirectory()| again.
  this.currentVolume_ = newRootPath;

  for (var i = 0; i < this.dataModel.length; i++) {
    var item = this.dataModel.item(i);
    if (PathUtil.getRootPath(item.fullPath) == newRootPath) {

      this.selectionModel.selectedIndex = i;
      return;
    }
  }
};

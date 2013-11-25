// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Entry of NavigationListModel. This constructor should be called only from
 * the helper methods (NavigationModelItem.create).
 *
 * @param {string} path Path.
 * @param {DirectoryEntry} entry Entry. Can be null.
 * @constructor
 */
function NavigationModelItem(path, entry) {
  this.path_ = path;
  this.entry_ = entry;
  this.resolvingQueue_ = new AsyncUtil.Queue();

  Object.seal(this);
}

NavigationModelItem.prototype = {
  get path() { return this.path_; },
};

/**
 * Returns the cached entry of the item. This may return NULL if the target is
 * not available on the filesystem, is not resolved or is under resolving the
 * entry.
 *
 * @return {Entry} Cached entry.
 */
NavigationModelItem.prototype.getCachedEntry = function() {
  return this.entry_;
};

/**
 * @param {VolumeManagerWrapper} volumeManager VolumeManagerWrapper instance.
 * @param {string} path Path.
 * @param {DirectoryEntry} entry Entry. Can be null.
 * @param {function(FileError)} errorCallback Called when the resolving is
 *     failed with the error.
 * @return {NavigationModelItem} Created NavigationModelItem.
 */
NavigationModelItem.create = function(
    volumeManager, path, entry, errorCallback) {
  var item = new NavigationModelItem(path, entry);

  // If the given entry is null, try to resolve path to get an entry.
  if (!entry) {
    item.resolvingQueue_.run(function(continueCallback) {
      volumeManager.resolvePath(
          path,
          function(entry) {
            if (entry.isDirectory)
              item.entry_ = entry;
            else
              errorCallback(util.createFileError(FileError.TYPE_MISMATCH_ERR));
            continueCallback();
          },
          function(error) {
            errorCallback(error);
            continueCallback();
          });
    });
  }
  return item;
};

/**
 * Retrieves the entry. If the entry is being retrieved, waits until it
 * finishes.
 * @param {function(Entry)} callback Called with the resolved entry. The entry
 *     may be NULL if resolving is failed.
 */
NavigationModelItem.prototype.getEntryAsync = function(callback) {
  // If resolving the entry is running, wait until it finishes.
  this.resolvingQueue_.run(function(continueCallback) {
    callback(this.entry_);
    continueCallback();
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
 * @param {VolumeManagerWrapper} volumeManager VolumeManagerWrapper instance.
 * @param {cr.ui.ArrayDataModel} shortcutListModel The list of folder shortcut.
 * @constructor
 * @extends {cr.EventTarget}
 */
function NavigationListModel(volumeManager, shortcutListModel) {
  cr.EventTarget.call(this);

  this.volumeManager_ = volumeManager;
  this.shortcutListModel_ = shortcutListModel;

  var volumeInfoToModelItem = function(volumeInfo) {
    if (volumeInfo.volumeType == util.VolumeType.DRIVE) {
      // For drive volume, we assign the path to "My Drive".
      return NavigationModelItem.create(
          this.volumeManager_,
          volumeInfo.mountPath + '/root',
          null,
          function() {});
    } else {
      return NavigationModelItem.create(
          this.volumeManager_,
          volumeInfo.mountPath,
          volumeInfo.root,
          function() {});
    }
  }.bind(this);

  var pathToModelItem = function(path) {
    var item = NavigationModelItem.create(
        this.volumeManager_,
        path,
        null,  // Entry will be resolved.
        function(error) {
          if (error.code == FileError.NOT_FOUND_ERR)
            this.onItemNotFoundError(item);
         }.bind(this));
    return item;
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

  // Generates this.volumeList_ and this.shortcutList_ from the models.
  this.volumeList_ =
      this.volumeManager_.volumeInfoList.slice().map(volumeInfoToModelItem);

  this.shortcutList_ = [];
  for (var i = 0; i < this.shortcutListModel_.length; i++) {
    var shortcutPath = this.shortcutListModel_.item(i);
    var mountPath = PathUtil.isDriveBasedPath(shortcutPath) ?
        RootDirectory.DRIVE :
        PathUtil.getRootPath(shortcutPath);
    var volumeInfo = this.volumeManager_.getVolumeInfo(mountPath);
    var isMounted = volumeInfo && !volumeInfo.error;
    if (isMounted)
      this.shortcutList_.push(pathToModelItem(shortcutPath));
  }

  // Generates a combined 'permuted' event from an event of either list.
  var permutedHandler = function(listType, event) {
    var permutation;

    // Build the volumeList.
    if (listType == ListType.VOLUME_LIST) {
      // The volume is mounted or unmounted.
      var newList = [];

      // Use the old instances if they just move.
      for (var i = 0; i < event.permutation.length; i++) {
        if (event.permutation[i] >= 0)
          newList[event.permutation[i]] = this.volumeList_[i];
      }

      // Create missing instances.
      for (var i = 0; i < event.newLength; i++) {
        if (!newList[i]) {
          newList[i] = volumeInfoToModelItem(
              this.volumeManager_.volumeInfoList.item(i));
        }
      }
      this.volumeList_ = newList;

      permutation = event.permutation.slice();
    } else {
      // volumeList part has not been changed, so the permutation should be
      // idenetity mapping.
      permutation = [];
      for (var i = 0; i < this.volumeList_.length; i++)
        permutation[i] = i;
    }

    // Build the shortcutList. Even if the event is for the volumeInfoList
    // update, the short cut path may be unmounted or newly mounted. So, here
    // shortcutList will always be re-built.
    // Currently this code may be redundant, as shortcut folder is supported
    // only on Drive File System and we can assume single-profile, but
    // multi-profile will be supported later.
    // The shortcut list is sorted in case-insensitive lexicographical order.
    // So we just can traverse the two list linearly.
    var modelIndex = 0;
    var oldListIndex = 0;
    var newList = [];
    while (modelIndex < this.shortcutListModel_.length &&
           oldListIndex < this.shortcutList_.length) {
      var shortcutPath = this.shortcutListModel_.item(modelIndex);
      var cmp = this.shortcutListModel_.compare(
          shortcutPath, this.shortcutList_[oldListIndex].path);
      if (cmp > 0) {
        // The shortcut at shortcutList_[oldListIndex] is removed.
        permutation.push(-1);
        oldListIndex++;
        continue;
      }

      // Check if the volume where the shortcutPath is is mounted or not.
      var mountPath = PathUtil.isDriveBasedPath(shortcutPath) ?
          RootDirectory.DRIVE :
          PathUtil.getRootPath(shortcutPath);
      var volumeInfo = this.volumeManager_.getVolumeInfo(mountPath);
      var isMounted = volumeInfo && !volumeInfo.error;
      if (cmp == 0) {
        // There exists an old NavigationModelItem instance.
        if (isMounted) {
          // Reuse the old instance.
          permutation.push(newList.length + this.volumeList_.length);
          newList.push(this.shortcutList_[oldListIndex]);
        } else {
          permutation.push(-1);
        }
        oldListIndex++;
      } else {
        // We needs to create a new instance for the shortcut path.
        if (isMounted)
          newList.push(pathToModelItem(shortcutPath));
      }
      modelIndex++;
    }

    // Add remaining (new) shortcuts if necessary.
    for (; modelIndex < this.shortcutListModel_.length; modelIndex++) {
      var shortcutPath = this.shortcutListModel_.item(modelIndex);
      var mountPath = PathUtil.isDriveBasedPath(shortcutPath) ?
          RootDirectory.DRIVE :
          PathUtil.getRootPath(shortcutPath);
      var volumeInfo = this.volumeManager_.getVolumeInfo(mountPath);
      var isMounted = volumeInfo && !volumeInfo.error;
      if (isMounted)
        newList.push(pathToModelItem(shortcutPath));
    }

    // Fill remaining permutation if necessary.
    for (; oldListIndex < this.shortcutList_.length; oldListIndex++)
      permutation.push(-1);

    this.shortcutList_ = newList;

    // Dispatch permuted event.
    var permutedEvent = new Event('permuted');
    permutedEvent.newLength =
        this.volumeList_.length + this.shortcutList_.length;
    permutedEvent.permutation = permutation;
    this.dispatchEvent(permutedEvent);
  };

  this.volumeManager_.volumeInfoList.addEventListener(
      'permuted', permutedHandler.bind(this, ListType.VOLUME_LIST));
  this.shortcutListModel_.addEventListener(
      'permuted', permutedHandler.bind(this, ListType.SHORTCUT_LIST));

  // 'change' event is just ignored, because it is not fired neither in
  // the folder shortcut list nor in the volume info list.
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
  var offset = this.volumeList_.length;
  if (index < offset)
    return this.volumeList_[index];
  return this.shortcutList_[index - offset];
};

/**
 * Returns the number of items in the model.
 * @return {number} The length of the model.
 * @private
 */
NavigationListModel.prototype.length_ = function() {
  return this.volumeList_.length + this.shortcutList_.length;
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
  } else if (index < this.volumeList_.length) {
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
  return !!this.volumeManager_.getVolumeInfo(RootDirectory.DRIVE);
};

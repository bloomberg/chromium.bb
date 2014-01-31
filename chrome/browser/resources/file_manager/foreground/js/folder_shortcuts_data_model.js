// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The drive mount path used in the storage. It must be '/drive'.
 * @type {string}
 */
var STORED_DRIVE_MOUNT_PATH = '/drive';

/**
 * Model for the folder shortcuts. This object is cr.ui.ArrayDataModel-like
 * object with additional methods for the folder shortcut feature.
 * This uses chrome.storage as backend. Items are always sorted by URL.
 *
 * @param {VolumeManagerWrapper} volumeManager Volume manager instance.
 * @constructor
 * @extends {cr.EventTarget}
 */
function FolderShortcutsDataModel(volumeManager) {
  this.volumeManager_ = volumeManager;
  this.array_ = [];
  this.pendingPaths_ = {};  // Hash map for easier deleting.
  this.unresolvablePaths_ = {};
  this.driveVolumeInfo_ = null;

  // Process changes only after initial loading is finished.
  var queue = new AsyncUtil.Queue();

  // Processes list of paths and converts them to Entry, then adds to the model.
  var processPaths = function(list, completionCallback) {
    for (var index = 0; index < list.length; index++) {
      this.pendingPaths_[list[index]] = true;
    }
    // Resolve the items all at once, in parallel.
    var group = new AsyncUtil.Group();
    for (var index = 0; index < list.length; index++) {
      var path = list[index];
      group.add(function(path, callback) {
        var url = this.convertStoredPathToUrl_(path);
        webkitResolveLocalFileSystemURL(url, function(entry) {
          if (entry.fullPath in this.pendingPaths_)
            delete this.pendingPaths_[entry.fullPath];
          this.addInternal_(entry);
          callback();
        }.bind(this), function() {
          if (path in this.pendingPaths_)
            delete this.pendingPaths_[path];
          // Remove the shortcut on error, only if Drive is fully online.
          // Only then we can be sure, that the error means that the directory
          // does not exist anymore.
          if (volumeManager.getDriveConnectionState().type !==
              util.DriveConnectionType.ONLINE) {
            this.unresolvablePaths_[path] = true;
          }
          // Not adding to the model nor to the |unresolvablePaths_| means
          // that it will be removed from the storage permanently after the
          // next call to save_().
          callback();
        }.bind(this));
      }.bind(this, path));
    }
    // Save the model after finishing.
    group.run(function() {
      this.save_();
      completionCallback();
    }.bind(this));
  }.bind(this);

  // Loads the contents from the storage to initialize the array.
  queue.run(function(queueCallback) {
    var shortcutPaths = null;

    // Process both (1) and (2) before processing paths.
    var group = new AsyncUtil.Group();

    // (1) Get the stored shortcuts from chrome.storage.
    group.add(function(callback) {
      chrome.storage.sync.get(FolderShortcutsDataModel.NAME, function(value) {
        if (!(FolderShortcutsDataModel.NAME in value)) {
          // If no shortcuts are stored, shortcutPaths is kept as null.
          callback();
          return;
        }

        // Since the value comes from outer resource, we have to check it just
        // in case.
        shortcutPaths = value[FolderShortcutsDataModel.NAME];

        // Record metrics.
        metrics.recordSmallCount('FolderShortcut.Count', shortcutPaths.length);

        callback();
      }.bind(this));
    }.bind(this));

    // (2) Get the volume info of the 'drive' volume.
    group.add(function(callback) {
      this.volumeManager_.ensureInitialized(function() {
        this.driveVolumeInfo_ = this.volumeManager_.getCurrentProfileVolumeInfo(
            util.VolumeType.DRIVE);
        callback();
      }.bind(this));
    }.bind(this));

    // Process shortcuts' paths after both (1) and (2) are finished.
    group.run(function() {
      // No shortcuts are stored.
      if (!shortcutPaths) {
        queueCallback();
        return;
      }

      if (!this.driveVolumeInfo_) {
        // Drive is unavailable.
        shortcutPaths.forEach(function(path) {
          this.unresolvablePaths_[path] = true;
       }, this);

        // TODO(mtomasz): Reload shortcuts when Drive goes back available.
        // http://crbug.com/333090

        queueCallback();
        return;
      }

      // Convert the paths to Entries and add to the model.
      processPaths(shortcutPaths, queueCallback);
    }.bind(this));
  }.bind(this));

  // Listening for changes in the storage. Process only after initial load is
  // finished.
  chrome.storage.onChanged.addListener(function(changes, namespace) {
    queue.run(function(queueCallback) {
      if (!(FolderShortcutsDataModel.NAME in changes) || namespace != 'sync') {
        queueCallback();
        return;
      }

      var list = changes[FolderShortcutsDataModel.NAME].newValue;

      if (!this.driveVolumeInfo_) {
        // Drive is unavailable.
        list.forEach(function(path) {
          this.unresolvablePaths_[path] = true;
        }, this);

        // TODO(mtomasz): Reload shortcuts when Drive goes back available.
        // http://crbug.com/333090

        queueCallback();
        return;
      }

      processPaths(list, queueCallback);
    }.bind(this));
  }.bind(this));
}

/**
 * Key name in chrome.storage. The array are stored with this name.
 * @type {string}
 * @const
 */
FolderShortcutsDataModel.NAME = 'folder-shortcuts-list';

FolderShortcutsDataModel.prototype = {
  __proto__: cr.EventTarget.prototype,

  /**
   * @return {number} Number of elements in the array.
   */
  get length() {
    return this.array_.length;
  },

  /**
   * Returns the entries in the given range as a new array instance. The
   * arguments and return value are compatible with Array.slice().
   *
   * @param {number} start Where to start the selection.
   * @param {number=} opt_end Where to end the selection.
   * @return {Array.<Entry>} Entries in the selected range.
   */
  slice: function(begin, opt_end) {
    return this.array_.slice(begin, opt_end);
  },

  /**
   * @param {number} index Index of the element to be retrieved.
   * @return {Entry} The value of the |index|-th element.
   */
  item: function(index) {
    return this.array_[index];
  },

  /**
   * @param {Entry} value Value of the element to be retrieved.
   * @return {number} Index of the element with the specified |value|.
   */
  getIndex: function(value) {
    for (var i = 0; i < this.length; i++) {
      // Same item check: must be exact match.
      if (util.isSameEntry(this.array_[i], value))
        return i;
    }
    return -1;
  },

  /**
   * Compares 2 entries and returns a number indicating one entry comes before
   * or after or is the same as the other entry in sort order.
   *
   * @param {Entry} a First entry.
   * @param {Entry} b Second entry.
   * @return {boolean} Returns -1, if |a| < |b|. Returns 0, if |a| === |b|.
   *     Otherwise, returns 1.
   */
  compare: function(a, b) {
    return a.toURL().localeCompare(
        b.toURL(),
        undefined,  // locale parameter, use default locale.
        {usage: 'sort', numeric: true});
  },

  /**
   * Adds the given item to the array. If there were already same item in the
   * list, return the index of the existing item without adding a duplicate
   * item.
   *
   * @param {Entry} value Value to be added into the array.
   * @return {number} Index in the list which the element added to.
   */
  add: function(value) {
    var result = this.addInternal_(value);
    metrics.recordUserAction('FolderShortcut.Add');
    return result;
  },

  /**
   * Adds the given item to the array. If there were already same item in the
   * list, return the index of the existing item without adding a duplicate
   * item.
   *
   * @param {Entry} value Value to be added into the array.
   * @return {number} Index in the list which the element added to.
   * @private
   */
  addInternal_: function(value) {
    var oldArray = this.array_.slice(0);  // Shallow copy.
    var addedIndex = -1;
    for (var i = 0; i < this.length; i++) {
      // Same item check: must be exact match.
      if (util.isSameEntry(this.array_[i], value))
        return i;

      // Since the array is sorted, new item will be added just before the first
      // larger item.
      if (this.compare(this.array_[i], value) >= 0) {
        this.array_.splice(i, 0, value);
        addedIndex = i;
        break;
      }
    }
    // If value is not added yet, add it at the last.
    if (addedIndex == -1) {
      this.array_.push(value);
      addedIndex = this.length;
    }

    this.firePermutedEvent_(
        this.calculatePermutation_(oldArray, this.array_));
    this.save_();
    metrics.recordUserAction('FolderShortcut.Add');
    return addedIndex;
  },

  /**
   * Removes the given item from the array.
   * @param {Entry} value Value to be removed from the array.
   * @return {number} Index in the list which the element removed from.
   */
  remove: function(value) {
    var removedIndex = -1;
    var oldArray = this.array_.slice(0);  // Shallow copy.
    for (var i = 0; i < this.length; i++) {
      // Same item check: must be exact match.
      if (util.isSameEntry(this.array_[i], value)) {
        this.array_.splice(i, 1);
        removedIndex = i;
        break;
      }
    }

    if (removedIndex != -1) {
      this.firePermutedEvent_(
          this.calculatePermutation_(oldArray, this.array_));
      this.save_();
      metrics.recordUserAction('FolderShortcut.Remove');
      return removedIndex;
    }

    // No item is removed.
    return -1;
  },

  /**
   * @param {Entry} entry Entry to be checked.
   * @return {boolean} True if the given |entry| exists in the array. False
   *     otherwise.
   */
  exists: function(entry) {
    var index = this.getIndex(entry);
    return (index >= 0);
  },

  /**
   * Saves the current array to chrome.storage.
   * @private
   */
  save_: function() {
    // The current implementation doesn't rely on sort order in prefs, however
    // older versions do. Therefore, we need to sort the paths before saving.
    // TODO(mtomasz): Remove sorting prefs after M-34 is stable.
    // crbug.com/333148
    var compareByPath = function(a, b) {
      return a.localeCompare(
          b,
          undefined,  // locale parameter, use default locale.
          {usage: 'sort', numeric: true});
    };

    // TODO(mtomasz): remove it once the shortcut reload logic is implemented.
    // http://crbug.com/333090
    if (!this.driveVolumeInfo_) {
      this.driveVolumeInfo_ = this.volumeManager_.getCurrentProfileVolumeInfo(
          util.VolumeType.DRIVE);

      // Returns without saving, because the drive has been unavailable since
      // Files.app opens and we can't retrieve the drive info.
      // It doesn't cause problem since the user can't change the shortcuts
      // when the Drive is unavailable.
      if (!this.driveVolumeInfo_)
        return;
    }

    // TODO(mtomasz): Migrate to URL.
    var paths = this.array_.
                map(function(entry) { return entry.toURL(); }).
                map(this.convertUrlToStoredPath_.bind(this)).
                concat(Object.keys(this.pendingPaths_)).
                concat(Object.keys(this.unresolvablePaths_)).
                sort(compareByPath);

    var prefs = {};
    prefs[FolderShortcutsDataModel.NAME] = paths;
    chrome.storage.sync.set(prefs, function() {});
  },

  /**
   * Creates a permutation array for 'permuted' event, which is compatible with
   * a permutation array used in cr/ui/array_data_model.js.
   *
   * @param {array} oldArray Previous array before changing.
   * @param {array} newArray New array after changing.
   * @return {Array.<number>} Created permutation array.
   * @private
   */
  calculatePermutation_: function(oldArray, newArray) {
    var oldIndex = 0;  // Index of oldArray.
    var newIndex = 0;  // Index of newArray.

    // Note that both new and old arrays are sorted.
    var permutation = [];
    for (; oldIndex < oldArray.length; oldIndex++) {
      if (newIndex >= newArray.length) {
        // oldArray[oldIndex] is deleted, which is not in the new array.
        permutation[oldIndex] = -1;
        continue;
      }

      while (newIndex < newArray.length) {
        // Unchanged item, which exists in both new and old array. But the
        // index may be changed.
        if (util.isSameEntry(oldArray[oldIndex], newArray[newIndex])) {
          permutation[oldIndex] = newIndex;
          newIndex++;
          break;
        }

        // oldArray[oldIndex] is deleted, which is not in the new array.
        if (this.compare(oldArray[oldIndex], newArray[newIndex]) < 0) {
          permutation[oldIndex] = -1;
          break;
        }

        // In the case of this.compare(oldArray[oldIndex]) > 0:
        // newArray[newIndex] is added, which is not in the old array.
        newIndex++;
      }
    }
    return permutation;
  },

  /**
   * Fires a 'permuted' event, which is compatible with cr.ui.ArrayDataModel.
   * @param {Array.<number>} Permutation array.
   */
  firePermutedEvent_: function(permutation) {
    var permutedEvent = new Event('permuted');
    permutedEvent.newLength = this.length;
    permutedEvent.permutation = permutation;
    this.dispatchEvent(permutedEvent);

    // Note: This model only fires 'permuted' event, because:
    // 1) 'change' event is not necessary to fire since it is covered by
    //    'permuted' event.
    // 2) 'splice' and 'sorted' events are not implemented. These events are
    //    not used in NavigationListModel. We have to implement them when
    //    necessary.
  },

  /**
   * Called externally when one of the items is not found on the filesystem.
   * @param {Entry} entry The entry which is not found.
   */
  onItemNotFoundError: function(entry) {
    // If Drive is online, then delete the shortcut permanently. Otherwise,
    // delete from model and add to |unresolvablePaths_|.
    if (this.volumeManager_.getDriveConnectionState().type !==
        util.DriveConnectionType.ONLINE) {
      var path = this.convertUrlToStoredPath_(entry.toURL());
      // TODO(mtomasz): Add support for multi-profile.
      this.unresolvablePaths_[path] = true;
    }
    this.remove(entry);
  },

  /**
   * Converts the given "stored path" to the URL
   *
   * This conversion is necessary because the shortcuts are not stored with
   * stored-formatted mount paths for compatibility. See http://crbug.com/336155
   * for detail.
   *
   * @param {string} path Path in Drive with the stored drive mount path.
   * @return {string} URL of the given path.
   * @private
   */
  convertStoredPathToUrl_: function(path) {
    if (path.indexOf(STORED_DRIVE_MOUNT_PATH + '/') !== 0) {
      console.warn(path + ' is neither a drive mount path nor a stored path.');
      return null;
    }

    var url = this.driveVolumeInfo_.root.toURL() +
        encodeURIComponent(path.substr(STORED_DRIVE_MOUNT_PATH.length));
    return url;
  },

  /**
   * Converts the URL to the stored-formatted path.
   *
   * See the comment of convertStoredPathToUrl_() for further information.
   *
   * @param {string} url URL of the directory in Drive.
   * @return {string} Path with the stored drive mount path.
   * @private
   */
  convertUrlToStoredPath_: function(url) {
    var rootUrl = this.driveVolumeInfo_.root.toURL();
    if (url.indexOf(rootUrl + '/') !== 0) {
      console.warn(url + ' is not a drive URL.');
      return null;
    }

    var storedPath = decodeURIComponent(
        STORED_DRIVE_MOUNT_PATH + url.substr(rootUrl.length));
    return storedPath;
  },
};

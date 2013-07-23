// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Basic sorted array model using chrome.storage as a backend.
 *
 * @param {string} type Type of backend. Should be "sync" or "local".
 * @param {string} name Name of the model.
 * @constructor
 * @extends {cr.EventTarget}
 */
function StoredSortedArray(type, name) {
  this.type_ = type;
  this.name_ = name;
  this.array_ = [];
  this.storage_ = (type == 'sync') ? chrome.storage.sync : chrome.storage.local;

  // Loads the contents from the storage to initialize the array.
  this.storage_.get(name, function(value) {
    if (!(name in value))
      return;

    // Since the value comes from outer resource, we have to check it just in
    // case.
    var list = value[name];
    if (list instanceof Array) {
      var changedEvent = new Event('changed');
      changedEvent.permutation = this.createPermutation_(this.array_, list);

      this.array_ = list;
      this.dispatchEvent(changedEvent);
    }
  }.bind(this));

  // Listening for changes in the storage.
  chrome.storage.onChanged.addListener(function(changes, namespace) {
    if (!(name in changes) || namespace != type)
      return;

    var list = changes[name].newValue;
    // Since the value comes from outer resource, we have to check it just in
    // case.
    if (list instanceof Array) {
      var changedEvent = new Event('changed');
      changedEvent.permutation = this.createPermutation_(this.array_, list);

      this.array_ = list;
      this.dispatchEvent(changedEvent);
    }
  }.bind(this));
}

StoredSortedArray.prototype = {
  __proto__: cr.EventTarget.prototype,

  /**
   * @return {number} Number of elements in the array.
   */
  get length() {
    return this.array_.length;
  },

  /**
   * @param {number} index Index of the element to be retrieved.
   * @return {string} The value of the |index|-th element.
   */
  getItem: function(index) {
    return this.array_[index];
  },

  /**
   * @param {string} value Value of the element to be retrieved.
   * @return {number} Index of the element with the specified |value|.
   */
  getIndex: function(value) {
    for (var i = 0; i < this.length; i++) {
      if (this.array_[i].localeCompare(value) == 0) {
        return i;
      }
    }
    return -1;
  },

  /**
   * Adds the given item to the array.
   * @param {string} value Value to be added into the array.
   * @return {number} Index in the list which the element added to.
   */
  add: function(value) {
    var i = 0;
    for (; i < this.length; i++) {
      // Since the array is sorted, new item will be added just before the first
      // larger item. If the same item exists, do nothing.
      if (this.array_[i].localeCompare(value) == 0) {
        return i;
      } else if (this.array_[i].localeCompare(value) >= 0) {
        this.array_.splice(i, 0, value);
        break;
      }
    }
    // If value is not added yet, add it at the last.
    if (i == this.length)
      this.array_.push(value);
    this.save_();
    return i;
  },

  /**
   * Removes the given item from the array.
   * @param {string} value Value to be removed from the array.
   * @return {number} Index in the list which the element removed from.
   */
  remove: function(value) {
    var i = 0;
    for (; i < this.length; i++) {
      if (this.array_[i] == value) {
        this.array_.splice(i, 1);
        break;
      }
    }
    this.save_();
    return i;
  },

  /**
   * Saves the current array to chrome.storage.
   * @private
   */
  save_: function() {
    var obj = {};
    obj[this.name_] = this.array_;
    this.storage_.set(obj, function() {});
  },

  /**
   * Creates a permutation array for 'changed' event, which is compatible with
   * a parmutation array used in cr/ui/array_data_model.js.
   *
   * @param {array} oldArray Previous array before changing.
   * @param {array} newArray New array after changing.
   * @return {Array.<number>} Created permutation array.
   * @private
   */
  createPermutation_: function(oldArray, newArray) {
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
        if (oldArray[oldIndex] == newArray[newIndex]) {
          // Unchanged item, which exists in both new and old array. But the
          // index may be changed.
          permutation[oldIndex] = newIndex;
          newIndex++;
          break;
        } else if (oldArray[oldIndex] < newArray[newIndex]) {
          // oldArray[oldIndex] is deleted, which is not in the new array.
          permutation[oldIndex] = -1;
          break;
        } else {  // oldArray[oldIndex] > newArray[newIndex]
          // newArray[newIndex] is added, which is not in the old array.
          newIndex++;
        }
      }
    }
    return permutation;
  }
};

/**
 * Model for the folder shortcuts.
 * This list is the combined array of the following arrays.
 *  1) syncable list of the folder shortcuts on drive
 *  2) non-syncable list of the folder shortcuts on the other volumes.
 *  Each array uses StoredSortedArray as implementation.
 *
 * @constructor
 * @extends {cr.EventTarget}
 */
function FolderShortcutsDataModel() {
  // Syncable array for Drive.
  this.remoteList_ = new StoredSortedArray('sync', 'folder-shortcuts-list');
  this.remoteList_.addEventListener(
      'changed',
      this.onArrayChanged_.bind(this, 'sync'));

  // Syncable array for other volumes.
  this.localList_ = new StoredSortedArray('local', 'folder-shortcuts-list');
  this.localList_.addEventListener(
      'changed',
      this.onArrayChanged_.bind(this, 'local'));
}

/**
 * Type of an event.
 * @enum {number}
 */
FolderShortcutsDataModel.EventType = {
  ADDED: 0,
  REMOVED: 1
};

FolderShortcutsDataModel.prototype = {
  __proto__: cr.EventTarget.prototype,

  /**
   * @return {number} The number of elements in the array.
   */
  get length() {
    return this.remoteList_.length + this.localList_.length;
  },

  /**
   * @param {string} path Path to be added.
   */
  add: function(path) {
    if (PathUtil.isDriveBasedPath(path)) {
      var index = this.remoteList_.add(path);
      this.fireChangeEvents_('sync', index,
                             FolderShortcutsDataModel.EventType.ADDED);
    } else {
      var index = this.localList_.add(path);
      this.fireChangeEvents_('local', index,
                             FolderShortcutsDataModel.EventType.ADDED);
    }
  },

  /**
   * @param {string} path Path to be removed.
   */
  remove: function(path) {
    if (PathUtil.isDriveBasedPath(path)) {
      var index = this.remoteList_.remove(path);
      this.fireChangeEvents_('sync', index,
                             FolderShortcutsDataModel.EventType.REMOVED);
    } else {
      var index = this.localList_.remove(path);
      this.fireChangeEvents_('local', index,
                             FolderShortcutsDataModel.EventType.REMOVED);
    }
  },

  /**
   * @param {string} path Path to be checked.
   * @return {boolean} True if the given |path| exists in the array. False
   *     otherwise.
   */
  exists: function(path) {
    if (PathUtil.isDriveBasedPath(path)) {
      var index = this.remoteList_.getIndex(path);
      return (index >= 0);
    } else {
      var index = this.localList_.getIndex(path);
      return (index >= 0);
    }
  },

  /**
   * @param {number} index Index of the element to be retrieved.
   * @return {string=} The value of the |index|-th element.
   */
  item: function(index) {
    if (0 <= index && index < this.remoteList_.length)
      return this.remoteList_.getItem(index);
    if (this.remoteList_.length <= index && index < this.length)
      return this.localList_.getItem(index - this.remoteList_.length);
    return undefined;
  },

  /**
   * Invoked when any of the subarray is changed. This method propagates
   * 'change' and 'permuted' events.
   *
   * @param {string} type Type of the array from which the change event comes.
   * @param {Event} event The 'changed' event from the array.
   */
  onArrayChanged_: function(type, event) {
    var changeEvent = new Event('change');
    changeEvent.index = (type == 'sync') ? 0 : this.remoteList_.length;
    this.dispatchEvent(changeEvent);

    var permutedEvent = new Event('permuted');
    permutedEvent.newLength = this.length;
    permutedEvent.permutation = event.permutation;
    this.dispatchEvent(permutedEvent);

    // 'splice' and 'sorted' events are not implemented.
  },

  /**
   * Fires 'change' and 'permuted' event.
   *
   * @param {string} type Type of the array from which the change event comes.
   * @param {number} index Changed index in the array.
   * @param {FolderShortcutsDataModel.EventType} type Type of the event.
   */
  fireChangeEvents_: function(type, index, eventType) {
    var changedIndex = ((type == 'sync') ? 0 : this.remoteList_.length) + index;

    var changeEvent = new Event('change');
    changeEvent.index = changedIndex;
    this.dispatchEvent(changeEvent);

    var permutation = [];
    if (eventType == FolderShortcutsDataModel.EventType.ADDED) {
      // Old length should be (current length) - 1, since an item is added.
      var oldLength = this.length - 1;
      for (var i = 0; i < oldLength; i++) {
        if (i < changedIndex)
          permutation[i] = i;
        else if (i == changedIndex)
          permutation[i] = i + 1;
        else
          permutation[i] = i + 1;
      }
    } else {  // eventType == FolderShortcutsDataModel.EventType.REMOVED
      // Old length should be (current length) + 1, since an item is removed.
      var oldLength = this.length + 1;
      for (var i = 0; i < oldLength; i++) {
        if (i < changedIndex)
          permutation[i] = i;
        else if (i == changedIndex)
          permutation[i] = -1;  // Item is removed.
        else
          permutation[i] = i - 1;
      }
    }

    var permutedEvent = new Event('permuted');
    permutedEvent.newLength = this.length;
    permutedEvent.permutation = permutation;
    this.dispatchEvent(permutedEvent);

    // 'splice' and 'sorted' events are not implemented.
  }
};

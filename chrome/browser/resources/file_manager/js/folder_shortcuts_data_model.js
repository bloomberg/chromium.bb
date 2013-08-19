// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Model for the folder shortcuts. This object is cr.ui.ArrayDataModel-like
 * object with additional methods for the folder shortcut feature.
 * This uses chrome.storage as backend. Items are always sorted by file path.
 *
 * @constructor
 * @extends {cr.EventTarget}
 */
function FolderShortcutsDataModel() {
  this.array_ = [];

  /**
   * Eliminate unsupported folders from the list.
   *
   * @param {Array.<string>} array Folder array which may contain the
   *     unsupported folders.
   * @return {Array.<string>} Folder list without unsupported folder.
   */
  var filter = function(array) {
    return array.filter(PathUtil.isEligibleForFolderShortcut);
  };

  // Loads the contents from the storage to initialize the array.
  chrome.storage.sync.get(FolderShortcutsDataModel.NAME, function(value) {
    if (!(FolderShortcutsDataModel.NAME in value))
      return;

    // Since the value comes from outer resource, we have to check it just in
    // case.
    var list = value[FolderShortcutsDataModel.NAME];
    if (list instanceof Array) {
      list = filter(list);

      // Record metrics.
      metrics.recordSmallCount('FolderShortcut.Count', list.length);

      var permutation = this.calculatePermutation_(this.array_, list);
      this.array_ = list;
      this.firePermutedEvent_(permutation);
    }
  }.bind(this));

  // Listening for changes in the storage.
  chrome.storage.onChanged.addListener(function(changes, namespace) {
    if (!(FolderShortcutsDataModel.NAME in changes) || namespace != 'sync')
      return;

    var list = changes[FolderShortcutsDataModel.NAME].newValue;
    // Since the value comes from outer resource, we have to check it just in
    // case.
    if (list instanceof Array) {
      list = filter(list);

      // If the list is not changed, do nothing and just return.
      if (this.array_.length == list.length) {
        var changed = false;
        for (var i = 0; i < this.array_.length; i++) {
          // Same item check: must be exact match.
          if (this.array_[i] != list[i]) {
            changed = true;
            break;
          }
        }
        if (!changed)
          return;
      }

      var permutation = this.calculatePermutation_(this.array_, list);
      this.array_ = list;
      this.firePermutedEvent_(permutation);
    }
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
   * Returns the paths in the given range as a new array instance. The
   * arguments and return value are compatible with Array.slice().
   *
   * @param {number} start Where to start the selection.
   * @param {number=} opt_end Where to end the selection.
   * @return {Array.<string>} Paths in the selected range.
   */
  slice: function(begin, opt_end) {
    return this.array_.slice(begin, opt_end);
  },

  /**
   * @param {number} index Index of the element to be retrieved.
   * @return {string} The value of the |index|-th element.
   */
  item: function(index) {
    return this.array_[index];
  },

  /**
   * @param {string} value Value of the element to be retrieved.
   * @return {number} Index of the element with the specified |value|.
   */
  getIndex: function(value) {
    for (var i = 0; i < this.length; i++) {
      // Same item check: must be exact match.
      if (this.array_[i] == value) {
        return i;
      }
    }
    return -1;
  },

  /**
   * Compares 2 strings and returns a number indicating one string comes before
   * or after or is the same as the other string in sort order.
   *
   * @param {string} a String1.
   * @param {string} b String2.
   * @return {boolean} Return -1, if String1 < String2. Return 0, if String1 ==
   *     String2. Otherwise, return 1.
   */
  compare: function(a, b) {
    return a.localeCompare(b,
                           undefined,  // locale parameter, use default locale.
                           {usage: 'sort', numeric: true});
  },

  /**
   * Adds the given item to the array. If there were already same item in the
   * list, return the index of the existing item without adding a duplicate
   * item.
   *
   * @param {string} value Value to be added into the array.
   * @return {number} Index in the list which the element added to.
   */
  add: function(value) {
    var oldArray = this.array_.slice(0);  // Shallow copy.
    var addedIndex = -1;
    for (var i = 0; i < this.length; i++) {
      // Same item check: must be exact match.
      if (this.array_[i] == value)
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
   * @param {string} value Value to be removed from the array.
   * @return {number} Index in the list which the element removed from.
   */
  remove: function(value) {
    var removedIndex = -1;
    var oldArray = this.array_.slice(0);  // Shallow copy.
    for (var i = 0; i < this.length; i++) {
      // Same item check: must be exact match.
      if (this.array_[i] == value) {
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
   * @param {string} path Path to be checked.
   * @return {boolean} True if the given |path| exists in the array. False
   *     otherwise.
   */
  exists: function(path) {
    var index = this.getIndex(path);
    return (index >= 0);
  },

  /**
   * Saves the current array to chrome.storage.
   * @private
   */
  save_: function() {
    var obj = {};
    obj[FolderShortcutsDataModel.NAME] = this.array_;
    chrome.storage.sync.set(obj, function() {});
  },

  /**
   * Creates a permutation array for 'permuted' event, which is compatible with
   * a parmutation array used in cr/ui/array_data_model.js.
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
        if (oldArray[oldIndex] == newArray[newIndex]) {
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
  }
};

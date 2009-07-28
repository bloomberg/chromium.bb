/**
 * @fileoverview This file contains js create a helper Map object
 * @author lzheng@chromium.com (Lei Zheng)
 */

/**
 * A Map class that implemeted using two arrays.
 */
Map = function() {
  /**
   * Array that hold all Keys.
   * @private
   */
  this.keysArray = []; // Keys
  /**
   * Array that hold all values.
   * @private
   */
  this.valsArray = []; // Values

  /**
   * Index of where to insert new entries in key and value array.
   * @private
   */
  this.mapIndex_ = 0;
};

/**
 * Insert a key, value pair to map.
 * @param {object} key An object that supports "==".
 * @param {object} value The entry in map.
 */
Map.prototype.insert = function(key, value) {
  var index = this.findIndex(key);
  if (index == -1)  {
    this.keysArray[this.mapIndex_] = key;
    this.valsArray[this.mapIndex_] = value;
    ++this.mapIndex_;
  }
  else {
    this.valsArray[index] = value;
  }
};

/**
 * Get the entry associated with a key.
 * @param {object} key for the entry.
 * @return {object} The entry in map.
 */
Map.prototype.keys = function() {
  return this.keysArray;
};

/**
 * Get the entry associated with a key.
 * @param {object} key for the entry.
 * @return {object} The entry in map.
 */
Map.prototype.get = function(key) {
  var index = this.findIndex(key);
  if (index != -1) {
    return this.valsArray[index];
  }
  return null;
};



/**
 * Remove an entry associated with a key.
 * @param {object} key for the entry.
 */
Map.prototype.remove = function(key) {
  var index = this.findIndex(key);
  if (index != -1) {
    this.keysArray = this.removeFromArray(keysArray, index);
    this.valsArray = this.removeFromAarry(valsArray, index);
    --this.mapIndex_;
  }
  return;
};

/**
 * Get the total entries in the map.
 * @return {int} The total number of entries.
 */
Map.prototype.size = function() {
  return this.mapIndex_;
};

/**
 * Clear up everything in the map.
 */
Map.clear = function() {
  this.mapIndex_ = 0;
  this.keysArray = [];
  this.valsArray = [];
};

/**
 * Find the index associated with a key.
 * @private
 * @param {object} key for the entry.
 * @return {int} The index of this entry.
 */
Map.prototype.findIndex = function(key) {
  var result = -1;
  for (var i = 0; i < this.keysArray.length; i++) {
    if (this.keysArray[i] == key) {
      result = i;
      break;
    }
  }
  return result;
};

/**
 * @private
 * Remove an entry from the map.
 * @param {int} index The index of the entry.
 */
Map.prototype.removeAt = function(array, index) {
  var first_half = array.slice(0, index);
  var second_half = array.slice(index + 1);
  return first_half.concat(second_half);
}

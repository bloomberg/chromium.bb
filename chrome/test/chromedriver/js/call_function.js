// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Enum for WebDriver status codes.
 * @enum {number}
 */
var StatusCode = {
  STALE_ELEMENT_REFERENCE: 10,
  UNKNOWN_ERROR: 13,
};

/**
 * Enum for node types.
 * @enum {number}
 */
var NodeType = {
  ELEMENT: 1,
  DOCUMENT: 9,
};

/**
 * Dictionary key to use for holding an element ID.
 * @const
 * @type {string}
 */
var ELEMENT_KEY = 'ELEMENT';

/**
 * A cache which maps IDs <-> cached objects for the purpose of identifying
 * a script object remotely.
 * @constructor
 */
function Cache() {
  this.cache_ = {};
  this.nextId_ = 1;
}

Cache.prototype = {

  /**
   * Stores a given item in the cache and returns a unique ID.
   *
   * @param {!Object} item The item to store in the cache.
   * @return {number} The ID for the cached item.
   */
  storeItem_: function(item) {
    for (var i in this.cache_) {
      if (item == this.cache_[i])
        return i;
    }
    var id = this.nextId_.toString();
    this.cache_[id] = item;
    this.nextId_++;
    return id;
  },

  /**
   * Retrieves the cached object for the given ID.
   *
   * @param {number} id The ID for the cached item to retrieve.
   * @return {!Object} The retrieved item.
   */
  retrieveItem_: function(id) {
    var item = this.cache_[id];
    if (item)
      return item;
    var error = new Error('not in cache');
    error.code = StatusCode.STALE_ELEMENT_REFERENCE;
    error.message = 'element is not attached to the page document';
    throw error;
  },

  /**
   * Clears stale items from the cache.
   */
  clearStale: function() {
    for (var id in this.cache_) {
      var node = this.cache_[id];
      while (node) {
        if (node == document)
          break;
        node = node.parentNode;
      }
      if (!node)
        delete this.cache_[id];
    }
  },

  /**
   * Wraps the given value to be transmitted remotely by converting
   * appropriate objects to cached object IDs.
   *
   * @param {*} value The value to wrap.
   * @return {*} The wrapped value.
   */
  wrap: function(value) {
    if (typeof(value) == 'object' && value != null) {
      var nodeType = value['nodeType'];
      if (nodeType == NodeType.ELEMENT || nodeType == NodeType.DOCUMENT) {
        var wrapped = {};
        wrapped[ELEMENT_KEY] = this.storeItem_(value);
        return wrapped;
      }

      var obj = (typeof(value.length) == 'number') ? [] : {};
      for (var prop in value)
        obj[prop] = this.wrap(value[prop]);
      return obj;
    }
    return value;
  },

  /**
   * Unwraps the given value by converting from object IDs to the cached
   * objects.
   *
   * @param {*} value The value to unwrap.
   * @return {*} The unwrapped value.
   */
  unwrap: function(value) {
    if (typeof(value) == 'object' && value != null) {
      if (ELEMENT_KEY in value)
        return this.retrieveItem_(value[ELEMENT_KEY]);

      var obj = (typeof(value.length) == 'number') ? [] : {};
      for (var prop in value)
        obj[prop] = this.unwrap(value[prop]);
      return obj;
    }
    return value;
  }
};

/**
 * Returns the global object cache for the page.
 * @return {!Cache} The page's object cache.
 */
function getPageCache() {
  // We use the same key as selenium's javascript/atoms/inject.js.
  var key = '$wdc_';
  if (!(key in document))
    document[key] = new Cache();
  return document[key];
}

/**
 * Calls a given function and returns its value.
 *
 * The inputs to and outputs of the function will be unwrapped and wrapped
 * respectively, unless otherwise specified. This wrapping involves converting
 * between cached object reference IDs and actual JS objects. The cache will
 * automatically be pruned each call to remove stale references.
 *
 * @param {function(...[*]) : *} func The function to invoke.
 * @param {!Array.<*>} args The array of arguments to supply to the function,
 *     which will be unwrapped before invoking the function.
 * @param {boolean=} opt_unwrappedReturn Whether the function's return value
 *     should be left unwrapped.
 * @return {*} An object containing a status and value property, where status
 *     is a WebDriver status code and value is the wrapped value. If an
 *     unwrapped return was specified, this will be the function's pure return
 *     value.
 */
function callFunction(func, args, opt_unwrappedReturn) {
  var cache = getPageCache();
  cache.clearStale();

  if (opt_unwrappedReturn)
    return func.apply(null, cache.unwrap(args));

  var status = 0;
  try {
    var returnValue = cache.wrap(func.apply(null, cache.unwrap(args)));
  } catch (error) {
    status = error.code || StatusCode.UNKNOWN_ERROR;
    var returnValue = error.message;
  }
  return {
      status: status,
      value: returnValue
  }
}

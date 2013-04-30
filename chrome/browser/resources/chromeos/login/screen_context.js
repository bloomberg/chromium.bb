// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Implementation of ScreenContext class: key-value storage for
 * values that are shared between C++ and JS sides.
 */
cr.define('login', function() {
  function require(condition, message) {
    if (!condition) {
      throw Error(message);
    }
  }

  function checkKeyIsValid(key) {
    var keyType = typeof key;
    require(keyType === 'string', 'Invalid type of key: "' + keyType + '".');
  }

  function checkValueIsValid(value) {
    var valueType = typeof value;
    require(['string', 'boolean', 'number'].indexOf(valueType) != -1,
        'Invalid type of value: "' + valueType + '".');
  }

  function ScreenContext() {
    this.storage_ = {};
    this.changes_ = {};
  }

  ScreenContext.prototype = {
    /**
     * Returns stored value for |key| or |defaultValue| if key not found in
     * storage. Throws Error if key not found and |defaultValue| omitted.
     */
    get: function(key, defaultValue) {
      checkKeyIsValid(key);
      if (this.hasKey(key)) {
        return this.storage_[key];
      } else if (typeof defaultValue !== 'undefined') {
        return defaultValue;
      } else {
        throw Error('Key "' + key + '" not found.');
      }
    },

    /**
     * Sets |value| for |key|. Returns true if call changes state of context,
     * false otherwise.
     */
    set: function(key, value) {
      checkKeyIsValid(key);
      checkValueIsValid(value);
      if (this.hasKey(key) && this.storage_[key] === value)
        return false;
      this.changes_[key] = value;
      this.storage_[key] = value;
      return true;
    },

    hasKey: function(key) {
      checkKeyIsValid(key);
      return this.storage_.hasOwnProperty(key);
    },

    hasChanges: function() {
      return Object.keys(this.changes_).length > 0;
    },

    /**
     * Applies |changes| to context. Returns Array of changed keys' names.
     */
    applyChanges: function(changes) {
      require(!this.hasChanges(), 'Context has changes.');
      Object.keys(changes).forEach(function(key) {
        checkKeyIsValid(key);
        checkValueIsValid(changes[key]);
        this.storage_[key] = changes[key];
      }, this);
      return Object.keys(changes);
    },

    /**
     * Returns changes made on context since previous call.
     */
    getChangesAndReset: function() {
      var result = this.changes_;
      this.changes_ = {};
      return result;
    }
  };

  return {
    ScreenContext: ScreenContext
  };
});

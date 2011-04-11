// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Namespace for utility functions.
 */
var util = {
  /**
   * Returns a function that console.log's its arguments, prefixed by |msg|.
   *
   * @param {string} msg The message prefix to use in the log.
   */
  flog: function(msg) {
    return function() {
      var ary = Array.apply(null, arguments);
      console.log(msg + ': ' + ary.join(', '));
    };
  },

  /**
   * Returns a function that throws an exception that includes its arguments
   * prefixed by |msg|.
   *
   * @param {string} msg The message prefix to use in the exception.
   */
  ferr: function(msg) {
    return function() {
      var ary = Array.apply(null, arguments);
      throw new Error(msg + ': ' + ary.join(', '));
    };
  },

  /**
   * Install a sensible toString() on the FileError object.
   *
   * FileError.prototype.code is a numeric code describing the cause of the
   * error.  The FileError constructor has a named property for each possible
   * error code, but provides no way to map the code to the named property.
   * This toString() implementation fixes that.
   */
  installFileErrorToString: function() {
    FileError.prototype.toString = function() {
      return '[object FileError: ' + util.getFileErrorMnemonic(this.code) + ']';
    }
  },

  getFileErrorMnemonic: function(code) {
    for (var key in FileError) {
      if (key.search(/_ERR$/) != -1 && FileError[key] == code)
        return key;
    }

    return code;
  }
};

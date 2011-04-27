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
   * @param {function(*)} opt_callback A function to invoke after logging.
   */
  flog: function(msg, opt_callback) {
    return function() {
      var ary = Array.apply(null, arguments);
      console.log(msg + ': ' + ary.join(', '));
      if (opt_callback)
        opt_callback.call(arguments);
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
  },

  /**
   * Utility function to invoke callback once for each entry in dirEntry.
   *
   * @param {DirectoryEntry} dirEntry The directory entry to enumerate.
   * @param {function(Entry)} callback The function to invoke for each entry in
   *   dirEntry.
   */
  forEachDirEntry: function(dirEntry, callback) {
    var reader;

    function onReadSome(results) {
      if (results.length == 0)
        return callback(null);

      for (var i = 0; i < results.length; i++)
        callback(results[i]);

      reader.readEntries(onReadSome);
    };

    reader = dirEntry.createReader();
    reader.readEntries(onReadSome);
  },

  /**
   * Utility function to resolve multiple directories with a single call.
   *
   * The successCallback will be invoked once for each directory object
   * found.  The errorCallback will be invoked once for each
   * path that could not be resolved.
   *
   * The successCallback is invoked with a null entry when all paths have
   * been processed.
   *
   * @param {DirEntry} dirEntry The base directory.
   * @param {Object} params The parameters to pass to the underlying
   *     getDirectory calls.
   * @param {Array<string>} paths The list of directories to resolve.
   * @param {function(!DirEntry)} successCallback The function to invoke for
   *     each DirEntry found.  Also invoked once with null at the end of the
   *     process.
   * @param {function(string, FileError)} errorCallback The function to invoke
   *     for each path that cannot be resolved.
   */
  getDirectories: function(dirEntry, params, paths, successCallback,
                           errorCallback) {

    // Copy the params array, since we're going to destroy it.
    params = [].slice.call(params);

    function onComplete() {
      successCallback(null);
    }

    function getNextDirectory() {
      var path = paths.shift();
      if (!path)
        return onComplete();

      dirEntry.getDirectory(
        path, params,
        function(entry) {
          successCallback(entry);
          getNextDirectory();
        },
        function(err) {
          errorCallback(path, err);
          getNextDirectory();
        });
    }

    getNextDirectory();
  },

};

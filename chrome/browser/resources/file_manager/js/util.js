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

  htmlUnescape: function(str) {
    return str.replace(/&(lt|gt|amp);/g, function(entity) {
      switch (entity) {
        case '&lt;': return '<';
        case '&gt;': return '>';
        case '&amp;': return '&';
      }
    });
  },

  /**
   * Given a list of Entries, recurse any DirectoryEntries, and call back
   * with a list of all file and directory entries encountered (including the
   * original set).
   */
  recurseAndResolveEntries: function(entries, successCallback, errorCallback) {
    var pendingSubdirectories = 0;
    var pendingFiles = 0;

    var dirEntries = [];
    var fileEntries = [];
    var fileBytes = 0;

    function pathCompare(a, b) {
      if (a.fullPath > b.fullPath)
        return 1;

      if (a.fullPath < b.fullPath)
        return -1;

      return 0;
    }

    // We invoke this after each async callback to see if we've received all
    // the expected callbacks.  If so, we're done.
    function areWeThereYet() {
      if (pendingSubdirectories == 0 && pendingFiles == 0) {
        var result = {
          dirEntries: dirEntries.sort(pathCompare),
          fileEntries: fileEntries.sort(pathCompare),
          fileBytes: fileBytes
        };

        if (successCallback) {
          successCallback(result);
        }
      }
    }

    function tallyEntry(entry) {
      if (entry.isDirectory) {
        dirEntries.push(entry);
        recurseDirectory(entry);
      } else {
        fileEntries.push(entry);
        pendingFiles++;
        entry.file(function(file) {
          fileBytes += file.size;
          pendingFiles--;
          areWeThereYet();
        });
      }
    }

    function recurseDirectory(dirEntry) {
      pendingSubdirectories++;

      util.forEachDirEntry(dirEntry, function(entry) {
          if (entry == null) {
            // Null entry indicates we're done scanning this directory.
            pendingSubdirectories--;
            areWeThereYet();
          } else {
            tallyEntry(entry);
          }
      });
    }

    for (var i = 0; i < entries.length; i++) {
      tallyEntry(entries[i]);
    }

    areWeThereYet();
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

    function onError(err) {
      console.error('Failed to read  dir entries at ' + dirEntry.fullPath);
    }

    function onReadSome(results) {
      if (results.length == 0)
        return callback(null);

      for (var i = 0; i < results.length; i++)
        callback(results[i]);

      reader.readEntries(onReadSome, onError);
    };

    reader = dirEntry.createReader();
    reader.readEntries(onReadSome, onError);
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

  /**
   * Resolve a path to either a DirectoryEntry or a FileEntry, regardless of
   * whether the path is a directory or file.
   *
   * @param root {DirectoryEntry} The root of the filesystem to search.
   * @param path {string} The path to be resolved.
   * @param resultCallback{function(Entry)} Called back when a path is
   *     successfully resolved.  Entry will be either a DirectoryEntry or
   *     a FileEntry.
   * @param errorCallback{function(FileError)} Called back if an unexpected
   *     error occurs while resolving the path.
   */
  resolvePath: function(root, path, resultCallback, errorCallback) {
    if (path == '' || path == '/') {
      resultCallback(root);
      return;
    }

    root.getFile(
        path, {create: false},
        resultCallback,
        function (err) {
          if (err.code == FileError.TYPE_MISMATCH_ERR) {
            // Bah.  It's a directory, ask again.
            root.getDirectory(
                path, {create: false},
                resultCallback,
                errorCallback);
          } else  {
            errorCallback(err);
          }
        });
  },

  /**
   * Locate the file referred to by path, creating directories or the file
   * itself if necessary.
   */
  getOrCreateFile: function(root, path, successCallback, errorCallback) {
    var dirname = null;
    var basename = null;

    function onDirFound(dirEntry) {
      dirEntry.getFile(basename, { create: true },
                       successCallback, errorCallback);
    }

    var i = path.lastIndexOf('/');
    if (i > -1) {
      dirname = path.substr(0, i);
      basename = path.substr(i + 1);
    } else {
      basename = path;
    }

    if (!dirname)
      return onDirFound(root);

    util.getOrCreateDirectory(root, dirname, onDirFound, errorCallback);
  },

  /**
   * Locate the directory referred to by path, creating directories along the
   * way.
   */
  getOrCreateDirectory: function(root, path, successCallback, errorCallback) {
    var names = path.split('/');

    function getOrCreateNextName(dir) {
      if (!names.length)
        return successCallback(dir);

      var name;
      do {
        name = names.shift();
      } while (!name || name == '.');

      dir.getDirectory(name, { create: true }, getOrCreateNextName,
                       errorCallback);
    }

    getOrCreateNextName(root);
  },

  /**
   * Lookup tables used by bytesToSi.
   */
  units_: ['B', 'KB', 'MB', 'GB', 'TB', 'PB'],
  scale_: [1, 1e3, 1e6, 1e9, 1e12, 1e15],

  /**
   * Convert a number of bytes into an appropriate International System of
   * Units (SI) representation, using the correct number separators.
   *
   * The first time this function is called it computes a lookup table which
   * is cached for subsequent calls.
   *
   * @param {number} bytes The number of bytes.
   */
  bytesToSi: function(bytes) {
    function fmt(s, u) {
      var rounded = Math.round(bytes / s * 10) / 10;
      // TODO(rginda): Switch to v8Locale's number formatter when it's
      // available.
      return rounded.toLocaleString() + u;
    }

    // This loop index is used outside the loop if it turns out |bytes|
    // requires the largest unit.
    var i;

    for (i = 0; i < this.units_.length - 1; i++) {
      if (bytes < this.scale_[i + 1])
        return fmt(this.scale_[i], this.units_[i]);
    }

    return fmt(this.scale_[i], this.units_[i]);
  },

  /**
   * Utility function to read specified range of bytes from file
   * @param file {File} file to read
   * @param begin {int} starting byte(included)
   * @param end {int} last byte(excluded)
   * @param callback {function(File, Uint8Array)} callback to invoke
   * @param onError {function(err)} error handler
   */
  readFileBytes: function(file, begin, end, callback, onError) {
    var fileReader = new FileReader();
    fileReader.onerror = onError;
    fileReader.onloadend = function() {
      callback(file, new ByteReader(fileReader.result))
    };
    fileReader.readAsArrayBuffer(file.webkitSlice(begin, end));
  },

  /**
   * Write a blob to a file.
   * Truncates the file first, so the previous content is fully overwritten.
   * @param {FileEntry} entry
   * @param {Blob} blob
   * @param {Function} onSuccess completion callback
   * @param {Function} onError error handler
   */
  writeBlobToFile: function(entry, blob, onSuccess, onError) {
    function truncate(writer) {
      writer.onerror = onError;
      writer.onwriteend = write.bind(null, writer);
      writer.truncate(0);
    }

    function write(writer) {
      writer.onwriteend = onSuccess;
      writer.write(blob);
    }

    entry.createWriter(truncate, onError);
  },

  createElement: function (document, elementName, var_arg) { // children
    var logger = console;
    var element = document.createElement(elementName);

    for (var i = 2; i < arguments.length; i++) {
      var arg = arguments[i];
      if (typeof(arg) == 'string') {
        element.appendChild(document.createTextNode(arg));
      } else {
        element.appendChild(arg);
      }
    }

    return element;
  }
};

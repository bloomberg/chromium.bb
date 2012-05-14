// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Namespace for utility functions.
 */
var util = {};

/**
 * Returns a function that console.log's its arguments, prefixed by |msg|.
 *
 * @param {string} msg The message prefix to use in the log.
 * @param {function} opt_callback A function to invoke after logging.
 * @return {function} Function that logs.
 */
util.flog = function(msg, opt_callback) {
  return function() {
    var ary = Array.apply(null, arguments);
    console.log(msg + ': ' + ary.join(', '));
    if (opt_callback)
      opt_callback.apply(null, arguments);
  };
};

/**
 * Returns a function that throws an exception that includes its arguments
 * prefixed by |msg|.
 *
 * @param {string} msg The message prefix to use in the exception.
 * @return {function} Function that throws.
 */
util.ferr = function(msg) {
  return function() {
    var ary = Array.apply(null, arguments);
    throw new Error(msg + ': ' + ary.join(', '));
  };
};

/**
 * Install a sensible toString() on the FileError object.
 *
 * FileError.prototype.code is a numeric code describing the cause of the
 * error.  The FileError constructor has a named property for each possible
 * error code, but provides no way to map the code to the named property.
 * This toString() implementation fixes that.
 */
util.installFileErrorToString = function() {
  FileError.prototype.toString = function() {
    return '[object FileError: ' + util.getFileErrorMnemonic(this.code) + ']';
  }
};

/**
 * @param {number} code The file error code.
 * @return {string} The file error mnemonic.
 */
util.getFileErrorMnemonic = function(code) {
  for (var key in FileError) {
    if (key.search(/_ERR$/) != -1 && FileError[key] == code)
      return key;
  }

  return code;
};

/**
 * @param {string} str String to unescape.
 * @return {string} Unescaped string.
 */
util.htmlUnescape = function(str) {
  return str.replace(/&(lt|gt|amp);/g, function(entity) {
    switch (entity) {
      case '&lt;': return '<';
      case '&gt;': return '>';
      case '&amp;': return '&';
    }
  });
};

/**
 * Given a list of Entries, recurse any DirectoryEntries if |recurse| is true,
 * and call back with a list of all file and directory entries encountered
 * (including the original set).
 * @param {Array.<Entry>} entries List of entries.
 * @param {boolean} recurse Whether to recurse.
 * @param {function(object)} successCallback Object has the fields dirEntries,
 *     fileEntries and fileBytes.
 */
util.recurseAndResolveEntries = function(entries, recurse, successCallback) {
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
      if (recurse) {
        recurseDirectory(entry);
      }
    } else {
      fileEntries.push(entry);
      pendingFiles++;
      entry.getMetadata(function(metadata) {
        fileBytes += metadata.size;
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
};

/**
 * Utility function to invoke callback once for each entry in dirEntry.
 *
 * @param {DirectoryEntry} dirEntry The directory entry to enumerate.
 * @param {function(Entry)} callback The function to invoke for each entry in
 *     dirEntry.
 */
util.forEachDirEntry = function(dirEntry, callback) {
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
};

/**
 * Reads contents of directory.
 * @param {DirectoryEntry} root Root entry.
 * @param {string} path Directory path.
 * @param {function(Array.<Entry>)} callback List of entries passed to callback.
 */
util.readDirectory = function(root, path, callback) {
  function onError(e) {
    callback([], e);
  }
  root.getDirectory(path, {create: false}, function(entry) {
    var reader = entry.createReader();
    var r = [];
    function readNext() {
      reader.readEntries(function(results) {
        if (results.length == 0) {
          callback(r, null);
          return;
        }
        r.push.apply(r, results);
        readNext();
      }, onError);
    }
    readNext();
  }, onError);
};

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
 * @param {function(FileError)} errorCallback The function to invoke
 *     for each path that cannot be resolved.
 */
util.getDirectories = function(dirEntry, params, paths, successCallback,
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
        errorCallback(err);
        getNextDirectory();
      });
  }

  getNextDirectory();
};

/**
 * Utility function to resolve multiple files with a single call.
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
 *     getFile calls.
 * @param {Array<string>} paths The list of files to resolve.
 * @param {function(!FileEntry)} successCallback The function to invoke for
 *     each FileEntry found.  Also invoked once with null at the end of the
 *     process.
 * @param {function(FileError)} errorCallback The function to invoke
 *     for each path that cannot be resolved.
 */
util.getFiles = function(dirEntry, params, paths, successCallback,
                         errorCallback) {
  // Copy the params array, since we're going to destroy it.
  params = [].slice.call(params);

  function onComplete() {
    successCallback(null);
  }

  function getNextFile() {
    var path = paths.shift();
    if (!path)
      return onComplete();

    dirEntry.getFile(
      path, params,
      function(entry) {
        successCallback(entry);
        getNextFile();
      },
      function(err) {
        errorCallback(err);
        getNextFile();
      });
  }

  getNextFile();
};

/**
 * Resolve a path to either a DirectoryEntry or a FileEntry, regardless of
 * whether the path is a directory or file.
 *
 * @param {DirectoryEntry} root The root of the filesystem to search.
 * @param {string} path The path to be resolved.
 * @param {function(Entry)} resultCallback Called back when a path is
 *     successfully resolved. Entry will be either a DirectoryEntry or
 *     a FileEntry.
 * @param {function(FileError)} errorCallback Called back if an unexpected
 *     error occurs while resolving the path.
 */
util.resolvePath = function(root, path, resultCallback, errorCallback) {
  if (path == '' || path == '/') {
    resultCallback(root);
    return;
  }

  root.getFile(
      path, {create: false},
      resultCallback,
      function(err) {
        if (err.code == FileError.TYPE_MISMATCH_ERR) {
          // Bah.  It's a directory, ask again.
          root.getDirectory(
              path, {create: false},
              resultCallback,
              errorCallback);
        } else {
          errorCallback(err);
        }
      });
};

/**
 * Locate the file referred to by path, creating directories or the file
 * itself if necessary.
 * @param {DirEntry} root The root entry.
 * @param {string} path The file path.
 * @param {function} successCallback The callback.
 * @param {function} errorCallback The callback.
 */
util.getOrCreateFile = function(root, path, successCallback, errorCallback) {
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

  if (!dirname) {
    onDirFound(root);
    return;
  }

  util.getOrCreateDirectory(root, dirname, onDirFound, errorCallback);
};

/**
 * Locate the directory referred to by path, creating directories along the
 * way.
 * @param {DirEntry} root The root entry.
 * @param {string} path The directory path.
 * @param {function} successCallback The callback.
 * @param {function} errorCallback The callback.
 */
util.getOrCreateDirectory = function(root, path, successCallback,
                                     errorCallback) {
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
};

/**
 * Remove a file or a directory.
 * @param {Entry} entry The entry to remove.
 * @param {function} onSuccess The success callback.
 * @param {function} onError The error callback.
 */
util.removeFileOrDirectory = function(entry, onSuccess, onError) {
  if (entry.isDirectory)
    entry.removeRecursively(onSuccess, onError);
  else
    entry.remove(onSuccess, onError);
};

/**
 * Units table for bytesToSi.
 * Note: changing this requires some code change in bytesToSi.
 * Note: these values are localized in file_manager.js.
 */
util.UNITS = ['KB', 'MB', 'GB', 'TB', 'PB'];

/**
 * Scale table for bytesToSi.
 */
util.SCALE = [1e3, 1e6, 1e9, 1e12, 1e15];

/**
 * Convert a number of bytes into an appropriate International System of
 * Units (SI) representation, using the correct number separators.
 *
 * @param {number} bytes The number of bytes.
 * @return {string} Localized string.
 */
util.bytesToSi = function(bytes) {
  function str(n, u) {
    // TODO(rginda): Switch to v8Locale's number formatter when it's
    // available.
    return n.toLocaleString() + ' ' + u;
  }

  function fmt(s, u) {
    var rounded = Math.round(bytes / s * 10) / 10;
    return str(rounded, u);
  }

  // Less than 1KB is displayed like '0.8 KB'.
  if (bytes < util.SCALE[0]) {
    return fmt(util.SCALE[0], util.UNITS[0]);
  }

  // Up to 1MB is displayed as rounded up number of KBs.
  if (bytes < util.SCALE[1]) {
    var rounded = Math.ceil(bytes / util.SCALE[0]);
    return str(rounded, util.UNITS[0]);
  }

  // This loop index is used outside the loop if it turns out |bytes|
  // requires the largest unit.
  var i;

  for (i = 1; i < util.UNITS.length - 1; i++) {
    if (bytes < util.SCALE[i + 1])
      return fmt(util.SCALE[i], util.UNITS[i]);
  }

  return fmt(util.SCALE[i], util.UNITS[i]);
};

/**
 * Utility function to read specified range of bytes from file
 * @param {File} file The file to read.
 * @param {number} begin Starting byte(included).
 * @param {number} end Last byte(excluded).
 * @param {function(File, Uint8Array)} callback Callback to invoke.
 * @param {function(FileError)} onError Error handler.
 */
util.readFileBytes = function(file, begin, end, callback, onError) {
  var fileReader = new FileReader();
  fileReader.onerror = onError;
  fileReader.onloadend = function() {
    callback(file, new ByteReader(fileReader.result));
  };
  fileReader.readAsArrayBuffer(file.webkitSlice(begin, end));
};

/**
 * Write a blob to a file.
 * Truncates the file first, so the previous content is fully overwritten.
 * @param {FileEntry} entry File entry.
 * @param {Blob} blob The blob to write.
 * @param {function} onSuccess Completion callback.
 * @param {function(FileError)} onError Error handler.
 */
util.writeBlobToFile = function(entry, blob, onSuccess, onError) {
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
};

/**
 * Returns a string '[Ctrl-][Alt-][Shift-][Meta-]' depending on the event
 * modifiers. Convenient for writing out conditions in keyboard handlers.
 *
 * @param {Event} event The keyboard event.
 * @return {string} Modifiers.
 */
util.getKeyModifiers = function(event) {
  return (event.ctrlKey ? 'Ctrl-' : '') +
         (event.altKey ? 'Alt-' : '') +
         (event.shiftKey ? 'Shift-' : '') +
         (event.metaKey ? 'Meta-' : '');
};

/**
 * @param {HTMLElement} element Element to transform.
 * @param {Object} transform Transform object,
 *                           contains scaleX, scaleY and rotate90 properties.
 */
util.applyTransform = function(element, transform) {
  element.style.webkitTransform =
      transform ? 'scaleX(' + transform.scaleX + ') ' +
                  'scaleY(' + transform.scaleY + ') ' +
                  'rotate(' + transform.rotate90 * 90 + 'deg)' :
      '';
};

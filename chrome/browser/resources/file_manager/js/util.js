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
  };
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
 * @param {number} code File error code (from FileError object).
 * @return {string} Translated file error string.
 */
util.getFileErrorString = function(code) {
  for (var key in FileError) {
    var match = /(.*)_ERR$/.exec(key);
    if (match && FileError[key] == code) {
      // This would convert 1 to 'NOT_FOUND'.
      code = match[1];
      break;
    }
  }
  console.warn('File error: ' + code);
  return loadTimeData.getString('FILE_ERROR_' + code) ||
      loadTimeData.getString('FILE_ERROR_GENERIC');
};

/**
 * @param {string} str String to escape.
 * @return {string} Escaped string.
 */
util.htmlEscape = function(str) {
  return str.replace(/[<>&]/g, function(entity) {
    switch (entity) {
      case '<': return '&lt;';
      case '>': return '&gt;';
      case '&': return '&amp;';
    }
  });
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

  var pathCompare = function(a, b) {
    if (a.fullPath > b.fullPath)
      return 1;

    if (a.fullPath < b.fullPath)
      return -1;

    return 0;
  };

  var parentPath = function(path) {
    return path.substring(0, path.lastIndexOf('/'));
  };

  // We invoke this after each async callback to see if we've received all
  // the expected callbacks.  If so, we're done.
  var areWeThereYet = function() {
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
  };

  var tallyEntry = function(entry, originalSourcePath) {
    entry.originalSourcePath = originalSourcePath;
    if (entry.isDirectory) {
      dirEntries.push(entry);
      if (recurse) {
        recurseDirectory(entry, originalSourcePath);
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
  };

  var recurseDirectory = function(dirEntry, originalSourcePath) {
    pendingSubdirectories++;

    util.forEachDirEntry(dirEntry, function(entry) {
        if (entry == null) {
          // Null entry indicates we're done scanning this directory.
          pendingSubdirectories--;
          areWeThereYet();
        } else {
          tallyEntry(entry, originalSourcePath);
        }
    });
  };

  for (var i = 0; i < entries.length; i++) {
    tallyEntry(entries[i], parentPath(entries[i].fullPath));
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

  var onError = function(err) {
    console.error('Failed to read  dir entries at ' + dirEntry.fullPath);
  };

  var onReadSome = function(results) {
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
  var onError = function(e) {
    callback([], e);
  };
  root.getDirectory(path, {create: false}, function(entry) {
    var reader = entry.createReader();
    var r = [];
    var readNext = function() {
      reader.readEntries(function(results) {
        if (results.length == 0) {
          callback(r, null);
          return;
        }
        r.push.apply(r, results);
        readNext();
      }, onError);
    };
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

  var onComplete = function() {
    successCallback(null);
  };

  var getNextDirectory = function() {
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
  };

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

  var onComplete = function() {
    successCallback(null);
  };

  var getNextFile = function() {
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
  };

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

  var onDirFound = function(dirEntry) {
    dirEntry.getFile(basename, { create: true },
                     successCallback, errorCallback);
  };

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

  var getOrCreateNextName = function(dir) {
    if (!names.length)
      return successCallback(dir);

    var name;
    do {
      name = names.shift();
    } while (!name || name == '.');

    dir.getDirectory(name, { create: true }, getOrCreateNextName,
                     errorCallback);
  };

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
 * Convert a number of bytes into a human friendly format, using the correct
 * number separators.
 *
 * @param {number} bytes The number of bytes.
 * @return {string} Localized string.
 */
util.bytesToString = function(bytes) {
  // Translation identifiers for size units.
  var UNITS = ['SIZE_BYTES',
               'SIZE_KB',
               'SIZE_MB',
               'SIZE_GB',
               'SIZE_TB',
               'SIZE_PB'];

  // Minimum values for the units above.
  var STEPS = [0,
               Math.pow(2, 10),
               Math.pow(2, 20),
               Math.pow(2, 30),
               Math.pow(2, 40),
               Math.pow(2, 50)];

  var str = function(n, u) {
    // TODO(rginda): Switch to v8Locale's number formatter when it's
    // available.
    return strf(u, n.toLocaleString());
  };

  var fmt = function(s, u) {
    var rounded = Math.round(bytes / s * 10) / 10;
    return str(rounded, u);
  };

  // Less than 1KB is displayed like '80 bytes'.
  if (bytes < STEPS[1]) {
    return str(bytes, UNITS[0]);
  }

  // Up to 1MB is displayed as rounded up number of KBs.
  if (bytes < STEPS[2]) {
    var rounded = Math.ceil(bytes / STEPS[1]);
    return str(rounded, UNITS[1]);
  }

  // This loop index is used outside the loop if it turns out |bytes|
  // requires the largest unit.
  var i;

  for (i = 2 /* MB */; i < UNITS.length - 1; i++) {
    if (bytes < STEPS[i + 1])
      return fmt(STEPS[i], UNITS[i]);
  }

  return fmt(STEPS[i], UNITS[i]);
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
  fileReader.readAsArrayBuffer(file.slice(begin, end));
};

if (!Blob.prototype.slice) {
  /**
   * This code might run in the test harness on older versions of Chrome where
   * Blob.slice is still called Blob.webkitSlice.
   */
  Blob.prototype.slice = Blob.prototype.webkitSlice;
}

/**
 * Write a blob to a file.
 * Truncates the file first, so the previous content is fully overwritten.
 * @param {FileEntry} entry File entry.
 * @param {Blob} blob The blob to write.
 * @param {function} onSuccess Completion callback.
 * @param {function(FileError)} onError Error handler.
 */
util.writeBlobToFile = function(entry, blob, onSuccess, onError) {
  var truncate = function(writer) {
    writer.onerror = onError;
    writer.onwriteend = write.bind(null, writer);
    writer.truncate(0);
  };

  var write = function(writer) {
    writer.onwriteend = onSuccess;
    writer.write(blob);
  };

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

/**
 * Makes filesystem: URL from the path.
 * @param {string} path File or directory path.
 * @return {string} URL.
 */
util.makeFilesystemUrl = function(path) {
  path = path.split('/').map(encodeURIComponent).join('/');
  var prefix = 'external';
  if (chrome.fileBrowserPrivate.mocked) {
    prefix = (chrome.fileBrowserPrivate.FS_TYPE == window.TEMPORARY) ?
        'temporary' : 'persistent';
  }
  return 'filesystem:' + document.location.origin + '/' + prefix + path;
};

/**
 * Extracts path from filesystem: URL.
 * @param {string} url Filesystem URL.
 * @return {string} The path.
 */
util.extractFilePath = function(url) {
  var match =
      /^filesystem:[\w-]*:\/\/[\w]*\/(external|persistent|temporary)(\/.*)$/.
      exec(url);
  var path = match && match[2];
  if (!path) return null;
  return decodeURIComponent(path);
};

/**
 * Traverses a tree up to a certain depth.
 * @param {FileEntry} root Root entry.
 * @param {function(Array.<Entry>)} callback The callback is called at the very
 *     end with a list of entries found.
 * @param {number?} max_depth Maximum depth. Pass zero to traverse everything.
 * @param {function(entry):boolean=} opt_filter Optional filter to skip some
 *     files/directories.
 */
util.traverseTree = function(root, callback, max_depth, opt_filter) {
  var list = [];
  util.forEachEntryInTree(root, function(entry) {
    if (entry) {
      list.push(entry);
    } else {
      callback(list);
    }
    return true;
  }, max_depth, opt_filter);
};

/**
 * Traverses a tree up to a certain depth, and calls a callback for each entry.
 * @param {FileEntry} root Root entry.
 * @param {function(Entry):boolean} callback The callback is called for each
 *     entry, and then once with null passed. If callback returns false once,
 *     the whole traversal is stopped.
 * @param {number?} max_depth Maximum depth. Pass zero to traverse everything.
 * @param {function(entry):boolean=} opt_filter Optional filter to skip some
 *     files/directories.
 */
util.forEachEntryInTree = function(root, callback, max_depth, opt_filter) {
  if (root.isFile) {
    if (opt_filter && !opt_filter(root)) {
      callback(null);
      return;
    }
    if (callback(root))
      callback(null);
    return;
  }

  var pending = 0;
  var cancelled = false;

  var maybeDone = function() {
    if (pending == 0 && !cancelled)
      callback(null);
  };

  var readEntry = function(entry, depth) {
    if (cancelled) return;
    if (opt_filter && !opt_filter(entry)) return;

    if (!callback(entry)) {
      cancelled = true;
      return;
    }

    // Do not recurse too deep and into files.
    if (entry.isFile || (max_depth != 0 && depth >= max_depth))
      return;

    pending++;
    util.forEachDirEntry(entry, function(childEntry) {
      if (childEntry == null) {
        // Null entry indicates we're done scanning this directory.
        pending--;
        maybeDone();
      } else {
        readEntry(childEntry, depth + 1);
      }
    });
  };

  readEntry(root, 0);
};

/**
 * A shortcut function to create a child element with given tag and class.
 *
 * @param {HTMLElement} parent Parent element.
 * @param {string} opt_className Class name.
 * @param {string} opt_tag Element tag, DIV is omitted.
 * @return {Element} Newly created element.
 */
util.createChild = function(parent, opt_className, opt_tag) {
  var child = parent.ownerDocument.createElement(opt_tag || 'div');
  if (opt_className)
    child.className = opt_className;
  parent.appendChild(child);
  return child;
};

/**
 * Update the app state.
 * For app v1 use the top window location search query and hash.
 * For app v2 use the top window appState variable.
 *
 * @param {boolean} replace True if the history state should be replaced,
 *                          false if pushed.
 * @param {string} path Path to be put in the address bar after the hash.
 *   If null the hash is left unchanged.
 * @param {string|object} opt_param Search parameter. Used directly if string,
 *   stringified if object. If omitted the search query is left unchanged.
 */
util.updateAppState = function(replace, path, opt_param) {
  if (window.appState) {
    // |replace| parameter is ignored. There is no stack, so saving/restoring
    // the state is the apps responsibility.
    if (typeof opt_param == 'string')
      window.appState.params = {};
    else if (typeof opt_param == 'object')
      window.appState.params = opt_param;
    if (path)
      window.appState.defaultPath = path;
    util.saveAppState();
    return;
  }

  var location = document.location;

  var search;
  if (typeof opt_param == 'string')
    search = opt_param;
  else if (typeof opt_param == 'object')
    search = '?' + JSON.stringify(opt_param);
  else
    search = location.search;

  var hash;
  if (path)
    hash = '#' + encodeURI(path);
  else
    hash = location.hash;

  var newLocation = location.origin + location.pathname + search + hash;
  //TODO(kaznacheev): Fix replaceState for component extensions. Currently it
  //does not replace the content of the address bar.
  if (replace)
    window.history.replaceState(undefined, path, newLocation);
  else
    window.history.pushState(undefined, path, newLocation);
};

/**
 * Return a translated string.
 *
 * Wrapper function to make dealing with translated strings more concise.
 * Equivalent to loadTimeData.getString(id).
 *
 * @param {string} id The id of the string to return.
 * @return {string} The translated string.
 */
function str(id) {
  return loadTimeData.getString(id);
}

/**
 * Return a translated string with arguments replaced.
 *
 * Wrapper function to make dealing with translated strings more concise.
 * Equivilant to loadTimeData.getStringF(id, ...).
 *
 * @param {string} id The id of the string to return.
 * @param {...string} var_args The values to replace into the string.
 * @return {string} The translated string with replaced values.
 */
function strf(id, var_args) {
  return loadTimeData.getStringF.apply(loadTimeData, arguments);
}

/**
 * Adapter object that abstracts away the the difference between Chrome app APIs
 * v1 and v2. Is only necessary while the migration to v2 APIs is in progress.
 */
util.platform = {
  /**
   * @return {boolean} True for v2.
   */
  v2: function() {
    try {
      return !!(chrome.app && chrome.app.runtime);
    } catch (e) {
      return false;
    }
  },

  /**
   * @param {function(Object)} callback Function accepting a preference map.
   */
  getPreferences: function(callback) {
    try {
      callback(window.localStorage);
    } catch (ignore) {
      chrome.storage.local.get(callback);
    }
  },

  /**
   * @param {string} key Preference name.
   * @param {function(string)} callback Function accepting the preference value.
   */
  getPreference: function(key, callback) {
    try {
      callback(window.localStorage[key]);
    } catch (ignore) {
      chrome.storage.local.get(key, function(items) {
        callback(items[key]);
      });
    }
  },

  /**
   * @param {string} key Preference name.
   * @param {string|object} value Preference value.
   * @param {function} opt_callback Completion callback.
   */
  setPreference: function(key, value, opt_callback) {
    if (typeof value != 'string')
      value = JSON.stringify(value);

    try {
      window.localStorage[key] = value;
      if (opt_callback) opt_callback();
    } catch (ignore) {
      var items = {};
      items[key] = value;
      chrome.storage.local.set(items, opt_callback);
    }
  },

  /**
   * @param {function(Object)} callback Function accepting a status object.
   */
  getWindowStatus: function(callback) {
    try {
      chrome.windows.getCurrent(callback);
    } catch (ignore) {
      // TODO: fill the status object once the API is available.
      callback({});
    }
  },

  /**
   * Close current window.
   */
  closeWindow: function() {
    if (util.platform.v2()) {
      window.close();
    } else {
      chrome.tabs.getCurrent(function(tab) {
        chrome.tabs.remove(tab.id);
      });
    }
  },

  /**
   * @return {string} Applicaton id.
   */
  getAppId: function() {
    if (util.platform.v2()) {
      return chrome.runtime.id;
    } else {
      return chrome.extension.getURL('').split('/')[2];
    }
  },

  /**
   * @param {string} path Path relative to the extension root.
   * @return {string} Extension-based URL.
   */
  getURL: function(path) {
    if (util.platform.v2()) {
      return chrome.runtime.getURL(path);
    } else {
      return chrome.extension.getURL(path);
    }
  },

  /**
   * Suppress default context menu in a current window.
   */
  suppressContextMenu: function() {
    // For packed v2 apps the default context menu would not show until
    // --debug-packed-apps is added to the command line.
    // For unpacked v2 apps (used for debugging) it is ok to show the menu.
    if (util.platform.v2())
      return;

    // For the old style app we show the menu only in the test harness mode.
    if (!util.TEST_HARNESS)
      document.addEventListener('contextmenu',
          function(e) { e.preventDefault() });
  },

  /**
   * Creates a new window.
   * @param {string} url Window url.
   * @param {Object} options Window options.
   */
  createWindow: function(url, options) {
    if (util.platform.v2()) {
      chrome.app.window.create(url, options);
    } else {
      var params = {};
      for (var key in options) {
        if (options.hasOwnProperty(key)) {
          params[key] = options[key];
        }
      }
      params.url = url;
      params.type = 'popup';
      chrome.windows.create(params);
    }
  }
};

/**
 * Load Javascript resources dynamically.
 * @param {Array.<string>} urls Array of script urls.
 * @param {function} onload Completion callback.
 */
util.loadScripts = function(urls, onload) {
  var countdown = urls.length;
  if (!countdown) {
    onload();
    return;
  }
  var done = function() {
    if (--countdown == 0)
      onload();
  };
  while (urls.length) {
    var script = document.createElement('script');
    script.src = urls.shift();
    document.head.appendChild(script);
    script.onload = done;
    script.onerror = done;
  }
};

// TODO(serya): remove it when have migrated to AppsV2.
util.__defineGetter__('storage', function() {
  delete util.storage;
  if (chrome.storage) {
    util.storage = chrome.storage;
    return util.storage;
  }

  var listeners = [];

  function StorageArea(type) {
    this.type_ = type;
  }

  StorageArea.prototype.set = function(items, opt_callback) {
    var changes = {};
    for (var i in items) {
      changes[i] = {oldValue: window.localStorage[i], newValue: items[i]};
      window.localStorage[i] = items[i];
    }
    if (opt_callback)
      opt_callback();
    for (var i = 0; i < listeners.length; i++) {
      listeners[i](changes, this.type_);
    }
  };

  StorageArea.prototype.get = function(keys, callback) {
    if (!callback) {
      // Since key is optionsl it's the callback.
      keys(window.localStorage);
      return;
    }
    if (typeof(keys) == 'string')
      keys = [keys];
    var result = {};
    for (var i = 0; i < keys.length; i++) {
      var key = keys[i];
      result[key] = window.localStorage[key];
    }
    callback(result);
  };

  /**
   * Simulation of the AppsV2 storage interface.
   * @type {object}
   */
  util.storage = {
    local: new StorageArea('local'),
    sync: new StorageArea('sync'),
    onChanged: {
      addListener: function(l) {
        listeners.push(l);
      },
      removeListener: function(l) {
        for (var i = 0; i < listeners.length; i++) {
          listeners.splice(i, 1);
        }
      }
    }
  };
  return util.storage;
});

/**
 * Attach page load handler.
 * Loads mock chrome.* APIs is the real ones are not present.
 * @param {function} handler Application-specific load handler.
 */
util.addPageLoadHandler = function(handler) {
  document.addEventListener('DOMContentLoaded', function() {
    if (chrome.fileBrowserPrivate) {
      handler();
    } else {
      util.TEST_HARNESS = true;
      util.loadScripts(['js/mock_chrome.js', 'js/file_copy_manager.js'],
          handler);
    }
    util.platform.suppressContextMenu();
  });
};

/**
 * Save app v2 launch data to the local storage.
 */
util.saveAppState = function() {
  if (window.appState)
    util.platform.setPreference(window.appID, window.appState);
};


/**
 *  AppCache is a persistent timestamped key-value storage backed by
 *  HTML5 local storage.
 *
 *  It is not designed for frequent access. In order to avoid costly
 *  localStorage iteration all data is kept in a single localStorage item.
 *  There is no in-memory caching, so concurrent access is _almost_ safe.
 *
 *  TODO(kaznacheev) Reimplement this based on Indexed DB.
 */
util.AppCache = function() {};

/**
 * Local storage key.
 */
util.AppCache.KEY = 'AppCache';

/**
 * Max number of items.
 */
util.AppCache.CAPACITY = 100;

/**
 * Default lifetime.
 */
util.AppCache.LIFETIME = 30 * 24 * 60 * 60 * 1000;  // 30 days.

/**
 * @param {string} key Key.
 * @param {function(number)} callback Callback accepting a value.
 */
util.AppCache.getValue = function(key, callback) {
  util.AppCache.read_(function(map) {
    var entry = map[key];
    callback(entry && entry.value);
  });
};

/**
 * Update the cache.
 *
 * @param {string} key Key.
 * @param {string} value Value. Remove the key if value is null.
 * @param {number} opt_lifetime Maximim time to keep an item (in milliseconds).
 */
util.AppCache.update = function(key, value, opt_lifetime) {
  util.AppCache.read_(function(map) {
    if (value != null) {
      map[key] = {
        value: value,
        expire: Date.now() + (opt_lifetime || util.AppCache.LIFETIME)
      };
    } else if (key in map) {
      delete map[key];
    } else {
      return;  // Nothing to do.
    }
    util.AppCache.cleanup_(map);
    util.AppCache.write_(map);
  });
};

/**
 * @param {function(Object)} callback Callback accepting a map of timestamped
 *   key-value pairs.
 * @private
 */
util.AppCache.read_ = function(callback) {
  util.platform.getPreference(util.AppCache.KEY, function(json) {
    if (json) {
      try {
        callback(JSON.parse(json));
      } catch (e) {
        // The local storage item somehow got messed up, start fresh.
      }
    }
    callback({});
  });
};

/**
 * @param {Object} map A map of timestamped key-value pairs.
 * @private
 */
util.AppCache.write_ = function(map) {
  util.platform.setPreference(util.AppCache.KEY, JSON.stringify(map));
};

/**
 * Remove over-capacity and obsolete items.
 *
 * @param {Object} map A map of timestamped key-value pairs.
 * @private
 */
util.AppCache.cleanup_ = function(map) {
  // Sort keys by ascending timestamps.
  var keys = [];
  for (var key in map) {
    if (map.hasOwnProperty(key))
      keys.push(key);
  }
  keys.sort(function(a, b) { return map[a].expire > map[b].expire });

  var cutoff = Date.now();

  var obsolete = 0;
  while (obsolete < keys.length &&
         map[keys[obsolete]].expire < cutoff) {
    obsolete++;
  }

  var overCapacity = Math.max(0, keys.length - util.AppCache.CAPACITY);

  var itemsToDelete = Math.max(obsolete, overCapacity);
  for (var i = 0; i != itemsToDelete; i++) {
    delete map[keys[i]];
  }
};

/**
 * RemoteImageLoader loads an image from a remote url.
 *
 * Fetches a blob via XHR, converts it to a data: url and assigns to img.src.
 * @constructor
 */
util.RemoteImageLoader = function() {};

/**
 * @param {Image} image Image element.
 * @param {string} url Remote url to load into the image.
 */
util.RemoteImageLoader.prototype.load = function(image, url) {
  this.onSuccess_ = function(dataURL) { image.src = dataURL };
  this.onError_ = function() { image.onerror() };

  var xhr = new XMLHttpRequest();
  xhr.responseType = 'blob';
  xhr.onload = function() {
    if (xhr.status == 200) {
      var reader = new FileReader;
      reader.onload = function(e) {
        this.onSuccess_(e.target.result);
      }.bind(this);
      reader.onerror = this.onError_;
      reader.readAsDataURL(xhr.response);
    } else {
      this.onError_();
    }
  }.bind(this);
  xhr.onerror = this.onError_;

  try {
    xhr.open('GET', url, true);
    xhr.send();
  } catch (e) {
    console.log(e);
    this.onError_();
  }
};

/**
 * Cancels the loading.
 */
util.RemoteImageLoader.prototype.cancel = function() {
  // We cannot really cancel the XHR.send and FileReader.readAsDataURL,
  // silencing the callbacks instead.
  this.onSuccess_ = this.onError_ = function() {};
};

/**
 * Load an image.
 *
 * In packaged apps img.src is not allowed to point to http(s)://.
 * For such urls util.RemoteImageLoader is used.
 *
 * @param {Image} image Image element.
 * @param {string} url Source url.
 * @return {util.RemoteImageLoader?} RemoteImageLoader object reference, use it
 *   to cancel the loading.
 */
util.loadImage = function(image, url) {
  if (util.platform.v2() && url.match(/^http(s):/)) {
    var imageLoader = new util.RemoteImageLoader();
    imageLoader.load(image, url);
    return imageLoader;
  }

  // OK to load directly.
  image.src = url;
  return null;
};

/**
 * Finds proerty descriptor in the object prototype chain.
 * @param {Object} object The object.
 * @param {string} propertyName The property name.
 * @return {Object} Property descriptor.
 */
util.findPropertyDescriptor = function(object, propertyName) {
  for (var p = object; p; p = Object.getPrototypeOf(p)) {
    var d = Object.getOwnPropertyDescriptor(p, propertyName);
    if (d)
      return d;
  }
  return null;
};

/**
 * Calls inherited property setter (useful when property is
 * overriden).
 * @param {Object} object The object.
 * @param {string} propertyName The property name.
 * @param {*} value Value to set.
 */
util.callInheritedSetter = function(object, propertyName, value) {
  var d = util.findPropertyDescriptor(Object.getPrototypeOf(object),
                                      propertyName);
  d.set.call(object, value);
};

/**
 * Returns true if the board of the device matches the given prefix.
 * @param {string} boardPrefix The board prefix to match against.
 *     (ex. "x86-mario". Prefix is used as the actual board name comes with
 *     suffix like "x86-mario-something".
 * @return {boolean} True if the board of the device matches the given prefix.
 */
util.boardIs = function(boardPrefix) {
  // The board name should be lower-cased, but making it case-insensitive for
  // backward compatibility just in case.
  var board = str('CHROMEOS_RELEASE_BOARD');
  var pattern = new RegExp('^' + boardPrefix, 'i');
  return board.match(pattern) != null;
};

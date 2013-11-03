// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Utilities for FileOperationManager.
 */
var fileOperationUtil = {};

/**
 * Simple wrapper for util.deduplicatePath. On error, this method translates
 * the FileError to FileOperationManager.Error object.
 *
 * @param {DirectoryEntry} dirEntry The target directory entry.
 * @param {string} relativePath The path to be deduplicated.
 * @param {function(string)} successCallback Callback run with the deduplicated
 *     path on success.
 * @param {function(FileOperationManager.Error)} errorCallback Callback run on
 *     error.
 */
fileOperationUtil.deduplicatePath = function(
    dirEntry, relativePath, successCallback, errorCallback) {
  util.deduplicatePath(
      dirEntry, relativePath, successCallback,
      function(err) {
        var onFileSystemError = function(error) {
          errorCallback(new FileOperationManager.Error(
              util.FileOperationErrorType.FILESYSTEM_ERROR, error));
        };

        if (err.code == FileError.PATH_EXISTS_ERR) {
          // Failed to uniquify the file path. There should be an existing
          // entry, so return the error with it.
          util.resolvePath(
              dirEntry, relativePath,
              function(entry) {
                errorCallback(new FileOperationManager.Error(
                    util.FileOperationErrorType.TARGET_EXISTS, entry));
              },
              onFileSystemError);
          return;
        }
        onFileSystemError(err);
      });
};

/**
 * Traverses files/subdirectories of the given entry, and returns them.
 * In addition, this method annotate the size of each entry. The result will
 * include the entry itself.
 *
 * @param {Entry} entry The root Entry for traversing.
 * @param {function(Array.<Entry>)} successCallback Called when the traverse
 *     is successfully done with the array of the entries.
 * @param {function(FileError)} errorCallback Called on error with the first
 *     occurred error (i.e. following errors will just be discarded).
 */
fileOperationUtil.resolveRecursively = function(
    entry, successCallback, errorCallback) {
  var result = [];
  var error = null;
  var numRunningTasks = 0;

  var maybeInvokeCallback = function() {
    // If there still remain some running tasks, wait their finishing.
    if (numRunningTasks > 0)
      return;

    if (error)
      errorCallback(error);
    else
      successCallback(result);
  };

  // The error handling can be shared.
  var onError = function(fileError) {
    // If this is the first error, remember it.
    if (!error)
      error = fileError;
    --numRunningTasks;
    maybeInvokeCallback();
  };

  var process = function(entry) {
    numRunningTasks++;
    result.push(entry);
    if (entry.isDirectory) {
      // The size of a directory is 1 bytes here, so that the progress bar
      // will work smoother.
      // TODO(hidehiko): Remove this hack.
      entry.size = 1;

      // Recursively traverse children.
      var reader = entry.createReader();
      reader.readEntries(
          function processSubEntries(subEntries) {
            if (error || subEntries.length == 0) {
              // If an error is found already, or this is the completion
              // callback, then finish the process.
              --numRunningTasks;
              maybeInvokeCallback();
              return;
            }

            for (var i = 0; i < subEntries.length; i++)
              process(subEntries[i]);

            // Continue to read remaining children.
            reader.readEntries(processSubEntries, onError);
          },
          onError);
    } else {
      // For a file, annotate the file size.
      entry.getMetadata(function(metadata) {
        entry.size = metadata.size;
        --numRunningTasks;
        maybeInvokeCallback();
      }, onError);
    }
  };

  process(entry);
};

/**
 * Copies source to parent with the name newName recursively.
 * This should work very similar to FileSystem API's copyTo. The difference is;
 * - The progress callback is supported.
 * - The cancellation is supported.
 *
 * @param {Entry} source The entry to be copied.
 * @param {DirectoryEntry} parent The entry of the destination directory.
 * @param {string} newName The name of copied file.
 * @param {function(string, string)} entryChangedCallback
 *     Callback invoked when an entry is created with the source url and
 *     the destination url.
 * @param {function(string, number)} progressCallback Callback invoked
 *     periodically during the copying. It takes the source url and the
 *     processed bytes of it.
 * @param {function(string)} successCallback Callback invoked when the copy
 *     is successfully done with the url of the created entry.
 * @param {function(FileError)} errorCallback Callback invoked when an error
 *     is found.
 * @return {function()} Callback to cancel the current file copy operation.
 *     When the cancel is done, errorCallback will be called. The returned
 *     callback must not be called more than once.
 */
fileOperationUtil.copyTo = function(
    source, parent, newName, entryChangedCallback, progressCallback,
    successCallback, errorCallback) {
  var copyId = null;
  var pendingCallbacks = [];

  var onCopyProgress = function(progressCopyId, status) {
    if (copyId == null) {
      // If the copyId is not yet available, wait for it.
      pendingCallbacks.push(
          onCopyProgress.bind(null, progressCopyId, status));
      return;
    }

    // This is not what we're interested in.
    if (progressCopyId != copyId)
      return;

    switch (status.type) {
      case 'begin_copy_entry':
        break;

      case 'end_copy_entry':
        entryChangedCallback(status.sourceUrl, status.destinationUrl);
        break;

      case 'progress':
        progressCallback(status.sourceUrl, status.size);
        break;

      case 'success':
        chrome.fileBrowserPrivate.onCopyProgress.removeListener(onCopyProgress);
        successCallback(status.destinationUrl);
        break;

      case 'error':
        chrome.fileBrowserPrivate.onCopyProgress.removeListener(onCopyProgress);
        errorCallback(util.createFileError(status.error));
        break;

      default:
        // Found unknown state. Cancel the task, and return an error.
        console.error('Unknown progress type: ' + status.type);
        chrome.fileBrowserPrivate.onCopyProgress.removeListener(onCopyProgress);
        chrome.fileBrowserPrivate.cancelCopy(copyId);
        errorCallback(util.createFileError(FileError.INVALID_STATE_ERR));
    }
  };

  // Register the listener before calling startCopy. Otherwise some events
  // would be lost.
  chrome.fileBrowserPrivate.onCopyProgress.addListener(onCopyProgress);

  // Then starts the copy.
  chrome.fileBrowserPrivate.startCopy(
      source.toURL(), parent.toURL(), newName, function(startCopyId) {
        // last error contains the FileError code on error.
        if (chrome.runtime.lastError) {
          // Unsubscribe the progress listener.
          chrome.fileBrowserPrivate.onCopyProgress.removeListener(
              onCopyProgress);
          errorCallback(util.createFileError(
              Integer.parseInt(chrome.runtime.lastError, 10)));
          return;
        }

        copyId = startCopyId;
        for (var i = 0; i < pendingCallbacks.length; i++) {
          pendingCallbacks[i]();
        }
      });

  return function() {
    // If copyId is not yet available, wait for it.
    if (copyId == null) {
      pendingCallbacks.push(function() {
        chrome.fileBrowserPrivate.cancelCopy(copyId);
      });
      return;
    }

    chrome.fileBrowserPrivate.cancelCopy(copyId);
  };
};

/**
 * Thin wrapper of chrome.fileBrowserPrivate.zipSelection to adapt its
 * interface similar to copyTo().
 *
 * @param {Array.<Entry>} sources The array of entries to be archived.
 * @param {DirectoryEntry} parent The entry of the destination directory.
 * @param {string} newName The name of the archive to be created.
 * @param {function(FileEntry)} successCallback Callback invoked when the
 *     operation is successfully done with the entry of the created archive.
 * @param {function(FileError)} errorCallback Callback invoked when an error
 *     is found.
 */
fileOperationUtil.zipSelection = function(
    sources, parent, newName, successCallback, errorCallback) {
  chrome.fileBrowserPrivate.zipSelection(
      parent.toURL(),
      sources.map(function(e) { return e.toURL(); }),
      newName, function(success) {
        if (!success) {
          // Failed to create a zip archive.
          errorCallback(
              util.createFileError(FileError.INVALID_MODIFICATION_ERR));
          return;
        }

        // Returns the created entry via callback.
        parent.getFile(
            newName, {create: false}, successCallback, errorCallback);
      });
};

/**
 * @constructor
 */
function FileOperationManager() {
  this.copyTasks_ = [];
  this.deleteTasks_ = [];
  this.unloadTimeout_ = null;
  this.taskIdCounter_ = 0;

  this.eventRouter_ = new FileOperationManager.EventRouter();

  Object.seal(this);
}

/**
 * Get FileOperationManager instance. In case is hasn't been initialized, a new
 * instance is created.
 *
 * @return {FileOperationManager} A FileOperationManager instance.
 */
FileOperationManager.getInstance = function() {
  if (!FileOperationManager.instance_)
    FileOperationManager.instance_ = new FileOperationManager();

  return FileOperationManager.instance_;
};

/**
 * Manages Event dispatching.
 * Currently this can send three types of events: "copy-progress",
 * "copy-operation-completed" and "delete".
 *
 * TODO(hidehiko): Reorganize the event dispatching mechanism.
 * @constructor
 * @extends {cr.EventTarget}
 */
FileOperationManager.EventRouter = function() {
};

/**
 * Extends cr.EventTarget.
 */
FileOperationManager.EventRouter.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Dispatches a simple "copy-progress" event with reason and current
 * FileOperationManager status. If it is an ERROR event, error should be set.
 *
 * @param {string} reason Event type. One of "BEGIN", "PROGRESS", "SUCCESS",
 *     "ERROR" or "CANCELLED". TODO(hidehiko): Use enum.
 * @param {Object} status Current FileOperationManager's status. See also
 *     FileOperationManager.getStatus().
 * @param {string} taskId ID of task related with the event.
 * @param {FileOperationManager.Error=} opt_error The info for the error. This
 *     should be set iff the reason is "ERROR".
 */
FileOperationManager.EventRouter.prototype.sendProgressEvent = function(
    reason, status, taskId, opt_error) {
  var event = new Event('copy-progress');
  event.reason = reason;
  event.status = status;
  event.taskId = taskId;
  if (opt_error)
    event.error = opt_error;
  this.dispatchEvent(event);
};

/**
 * Dispatches an event to notify that an entry is changed (created or deleted).
 * @param {util.EntryChangedKind} kind The enum to represent if the entry is
 *     created or deleted.
 * @param {Entry} entry The changed entry.
 */
FileOperationManager.EventRouter.prototype.sendEntryChangedEvent = function(
    kind, entry) {
  var event = new Event('entry-changed');
  event.kind = kind;
  event.entry = entry;
  this.dispatchEvent(event);
};

/**
 * Dispatches an event to notify entries are changed for delete task.
 *
 * @param {string} reason Event type. One of "BEGIN", "PROGRESS", "SUCCESS",
 *     or "ERROR". TODO(hidehiko): Use enum.
 * @param {Array.<string>} urls An array of URLs which are affected by delete
 *     operation.
 * @param {string} taskId ID of task related with the event.
 */
FileOperationManager.EventRouter.prototype.sendDeleteEvent = function(
    reason, urls, taskId) {
  var event = new Event('delete');
  event.reason = reason;
  event.urls = urls;
  this.dispatchEvent(event);
};

/**
 * A record of a queued copy operation.
 *
 * Multiple copy operations may be queued at any given time.  Additional
 * Tasks may be added while the queue is being serviced.  Though a
 * cancel operation cancels everything in the queue.
 *
 * @param {util.FileOperationType} operationType The type of this operation.
 * @param {Array.<Entry>} sourceEntries Array of source entries.
 * @param {DirectoryEntry} targetDirEntry Target directory.
 * @constructor
 */
FileOperationManager.Task = function(
    operationType, sourceEntries, targetDirEntry) {
  this.operationType = operationType;
  this.sourceEntries = sourceEntries;
  this.targetDirEntry = targetDirEntry;

  /**
   * An array of map from url to Entry being processed.
   * @type {Array.<Object<string, Entry>>}
   */
  this.processingEntries = null;

  /**
   * Total number of bytes to be processed. Filled in initialize().
   * @type {number}
   */
  this.totalBytes = 0;

  /**
   * Total number of already processed bytes. Updated periodically.
   * @type {number}
   */
  this.processedBytes = 0;

  this.deleteAfterCopy = false;

  /**
   * Set to true when cancel is requested.
   * @private {boolean}
   */
  this.cancelRequested_ = false;

  /**
   * Callback to cancel the running process.
   * @private {function()}
   */
  this.cancelCallback_ = null;

  // TODO(hidehiko): After we support recursive copy, we don't need this.
  // If directory already exists, we try to make a copy named 'dir (X)',
  // where X is a number. When we do this, all subsequent copies from
  // inside the subtree should be mapped to the new directory name.
  // For example, if 'dir' was copied as 'dir (1)', then 'dir\file.txt' should
  // become 'dir (1)\file.txt'.
  this.renamedDirectories_ = [];
};

/**
 * @param {function()} callback When entries resolved.
 */
FileOperationManager.Task.prototype.initialize = function(callback) {
};

/**
 * Updates copy progress status for the entry.
 *
 * @param {number} size Number of bytes that has been copied since last update.
 */
FileOperationManager.Task.prototype.updateFileCopyProgress = function(size) {
  this.completedBytes += size;
};

/**
 * Requests cancellation of this task.
 * When the cancellation is done, it is notified via callbacks of run().
 */
FileOperationManager.Task.prototype.requestCancel = function() {
  this.cancelRequested_ = true;
  if (this.cancelCallback_) {
    this.cancelCallback_();
    this.cancelCallback_ = null;
  }
};

/**
 * Runs the task. Sub classes must implement this method.
 *
 * @param {function(util.EntryChangedKind, Entry)} entryChangedCallback
 *     Callback invoked when an entry is changed.
 * @param {function()} progressCallback Callback invoked periodically during
 *     the operation.
 * @param {function()} successCallback Callback run on success.
 * @param {function(FileOperationManager.Error)} errorCallback Callback run on
 *     error.
 */
FileOperationManager.Task.prototype.run = function(
    entryChangedCallback, progressCallback, successCallback, errorCallback) {
};

/**
 * Get states of the task.
 * TOOD(hirono): Removes this method and sets a task to progress events.
 * @return {object} Status object.
 */
FileOperationManager.Task.prototype.getStatus = function() {
  var numRemainingItems = this.countRemainingItems();
  return {
    operationType: this.operationType,
    numRemainingItems: numRemainingItems,
    totalBytes: this.totalBytes,
    processedBytes: this.processedBytes,
    processingEntry: this.getSingleEntry()
  };
};

/**
 * Counts the number of remaining items.
 * @return {number} Number of remaining items.
 */
FileOperationManager.Task.prototype.countRemainingItems = function() {
  var count = 0;
  for (var i = 0; i < this.processingEntries.length; i++) {
    for (var url in this.processingEntries[i]) {
      count++;
    }
  }
  return count;
};

/**
 * Obtains the single processing entry. If there are multiple processing
 * entries, it returns null.
 * @return {Entry} First entry.
 */
FileOperationManager.Task.prototype.getSingleEntry = function() {
  if (this.countRemainingItems() !== 1)
    return null;
  for (var i = 0; i < this.processingEntries.length; i++) {
    var entryMap = this.processingEntries[i];
    for (var name in entryMap)
      return entryMap[name];
  }
  return null;
};

/**
 * Task to copy entries.
 *
 * @param {Array.<Entry>} sourceEntries Array of source entries.
 * @param {DirectoryEntry} targetDirEntry Target directory.
 * @constructor
 * @extends {FileOperationManager.Task}
 */
FileOperationManager.CopyTask = function(sourceEntries, targetDirEntry) {
  FileOperationManager.Task.call(
      this, util.FileOperationType.COPY, sourceEntries, targetDirEntry);
};

/**
 * Extends FileOperationManager.Task.
 */
FileOperationManager.CopyTask.prototype.__proto__ =
    FileOperationManager.Task.prototype;

/**
 * Initializes the CopyTask.
 * @param {function()} callback Called when the initialize is completed.
 */
FileOperationManager.CopyTask.prototype.initialize = function(callback) {
  var group = new AsyncUtil.Group();
  // Correct all entries to be copied for status update.
  this.processingEntries = [];
  for (var i = 0; i < this.sourceEntries.length; i++) {
    group.add(function(index, callback) {
      fileOperationUtil.resolveRecursively(
          this.sourceEntries[index],
          function(resolvedEntries) {
            var resolvedEntryMap = {};
            for (var j = 0; j < resolvedEntries.length; ++j) {
              var entry = resolvedEntries[j];
              entry.processedBytes = 0;
              resolvedEntryMap[entry.toURL()] = entry;
            }
            this.processingEntries[index] = resolvedEntryMap;
            callback();
          }.bind(this),
          function(error) {
            console.error(
                'Failed to resolve for copy: %s',
                util.getFileErrorMnemonic(error.code));
          });
    }.bind(this, i));
  }

  group.run(function() {
    // Fill totalBytes.
    this.totalBytes = 0;
    for (var i = 0; i < this.processingEntries.length; i++) {
      for (var url in this.processingEntries[i])
        this.totalBytes += this.processingEntries[i][url].size;
    }

    callback();
  }.bind(this));
};

/**
 * Copies all entries to the target directory.
 * Note: this method contains also the operation of "Move" due to historical
 * reason.
 *
 * @param {function(util.EntryChangedKind, Entry)} entryChangedCallback
 *     Callback invoked when an entry is changed.
 * @param {function()} progressCallback Callback invoked periodically during
 *     the copying.
 * @param {function()} successCallback On success.
 * @param {function(FileOperationManager.Error)} errorCallback On error.
 * @override
 */
FileOperationManager.CopyTask.prototype.run = function(
    entryChangedCallback, progressCallback, successCallback, errorCallback) {
  // TODO(hidehiko): We should be able to share the code to iterate on entries
  // with serviceMoveTask_().
  if (this.sourceEntries.length == 0) {
    successCallback();
    return;
  }

  // TODO(hidehiko): Delete after copy is the implementation of Move.
  // Migrate the part into MoveTask.run().
  var deleteOriginals = function() {
    var count = this.sourceEntries.length;

    var onEntryDeleted = function(entry) {
      entryChangedCallback(util.EntryChangedKind.DELETED, entry);
      count--;
      if (!count)
        successCallback();
    };

    var onFilesystemError = function(err) {
      errorCallback(new FileOperationManager.Error(
          util.FileOperationErrorType.FILESYSTEM_ERROR, err));
    };

    for (var i = 0; i < this.sourceEntries.length; i++) {
      var entry = this.sourceEntries[i];
      util.removeFileOrDirectory(
          entry, onEntryDeleted.bind(null, entry), onFilesystemError);
    }
  }.bind(this);

  AsyncUtil.forEach(
      this.sourceEntries,
      function(callback, entry, index) {
        if (this.cancelRequested_) {
          errorCallback(new FileOperationManager.Error(
              util.FileOperationErrorType.FILESYSTEM_ERROR,
              util.createFileError(FileError.ABORT_ERR)));
          return;
        }
        progressCallback();
        this.processEntry_(
            entry, this.targetDirEntry,
            function(sourceUrl, destinationUrl) {
              // Finalize the entry's progress state.
              var entry = this.processingEntries[index][sourceUrl];
              if (entry) {
                this.processedBytes += entry.size - entry.processedBytes;
                progressCallback();
                delete this.processingEntries[index][sourceUrl];
              }

              webkitResolveLocalFileSystemURL(
                  destinationUrl, function(destinationEntry) {
                    entryChangedCallback(
                        util.EntryChangedKind.CREATED, destinationEntry);
                  });
            }.bind(this),
            function(source_url, size) {
              var entry = this.processingEntries[index][source_url];
              if (entry) {
                this.processedBytes += size - entry.processedBytes;
                entry.processedBytes = size;
                progressCallback();
              }
            }.bind(this),
            callback,
            errorCallback);
      },
      function() {
        if (this.deleteAfterCopy) {
          deleteOriginals();
        } else {
          successCallback();
        }
      }.bind(this),
      this);
};

/**
 * Copies the source entry to the target directory.
 *
 * @param {Entry} sourceEntry An entry to be copied.
 * @param {DirectoryEntry} destinationEntry The entry which will contain the
 *     copied entry.
 * @param {function(string, string)} entryChangedCallback
 *     Callback invoked when an entry is created with the source url and
 *     the destination url.
 * @param {function(string, number)} progressCallback Callback invoked
 *     periodically during the copying.
 * @param {function()} successCallback On success.
 * @param {function(FileOperationManager.Error)} errorCallback On error.
 * @private
 */
FileOperationManager.CopyTask.prototype.processEntry_ = function(
    sourceEntry, destinationEntry, entryChangedCallback, progressCallback,
    successCallback, errorCallback) {
  fileOperationUtil.deduplicatePath(
      destinationEntry, sourceEntry.name,
      function(destinationName) {
        if (this.cancelRequested_) {
          errorCallback(new FileOperationManager.Error(
              util.FileOperationErrorType.FILESYSTEM_ERROR,
              util.createFileError(FileError.ABORT_ERR)));
          return;
        }
        this.cancelCallback_ = fileOperationUtil.copyTo(
            sourceEntry, destinationEntry, destinationName,
            entryChangedCallback, progressCallback,
            function(entry) {
              this.cancelCallback_ = null;
              successCallback();
            }.bind(this),
            function(error) {
              this.cancelCallback_ = null;
              errorCallback(new FileOperationManager.Error(
                  util.FileOperationErrorType.FILESYSTEM_ERROR, error));
            }.bind(this));
      }.bind(this),
      errorCallback);
};

/**
 * Task to move entries.
 *
 * @param {Array.<Entry>} sourceEntries Array of source entries.
 * @param {DirectoryEntry} targetDirEntry Target directory.
 * @constructor
 * @extends {FileOperationManager.Task}
 */
FileOperationManager.MoveTask = function(sourceEntries, targetDirEntry) {
  FileOperationManager.Task.call(
      this, util.FileOperationType.MOVE, sourceEntries, targetDirEntry);
};

/**
 * Extends FileOperationManager.Task.
 */
FileOperationManager.MoveTask.prototype.__proto__ =
    FileOperationManager.Task.prototype;

/**
 * Initializes the MoveTask.
 * @param {function()} callback Called when the initialize is completed.
 */
FileOperationManager.MoveTask.prototype.initialize = function(callback) {
  // This may be moving from search results, where it fails if we
  // move parent entries earlier than child entries. We should
  // process the deepest entry first. Since move of each entry is
  // done by a single moveTo() call, we don't need to care about the
  // recursive traversal order.
  this.sourceEntries.sort(function(entry1, entry2) {
    return entry2.fullPath.length - entry1.fullPath.length;
  });

  this.processingEntries = [];
  for (var i = 0; i < this.sourceEntries.length; i++) {
    var processingEntryMap = {};
    var entry = this.sourceEntries[i];

    // The move should be done with updating the metadata. So here we assume
    // all the file size is 1 byte. (Avoiding 0, so that progress bar can
    // move smoothly).
    // TODO(hidehiko): Remove this hack.
    entry.size = 1;
    processingEntryMap[entry.toURL()] = entry;
    this.processingEntries[i] = processingEntryMap;
  }

  callback();
};

/**
 * Moves all entries in the task.
 *
 * @param {function(util.EntryChangedKind, Entry)} entryChangedCallback
 *     Callback invoked when an entry is changed.
 * @param {function()} progressCallback Callback invoked periodically during
 *     the moving.
 * @param {function()} successCallback On success.
 * @param {function(FileOperationManager.Error)} errorCallback On error.
 * @override
 */
FileOperationManager.MoveTask.prototype.run = function(
    entryChangedCallback, progressCallback, successCallback, errorCallback) {
  if (this.sourceEntries.length == 0) {
    successCallback();
    return;
  }

  AsyncUtil.forEach(
      this.sourceEntries,
      function(callback, entry, index) {
        if (this.cancelRequested_) {
          errorCallback(new FileOperationManager.Error(
              util.FileOperationErrorType.FILESYSTEM_ERROR,
              util.createFileError(FileError.ABORT_ERR)));
          return;
        }
        progressCallback();
        FileOperationManager.MoveTask.processEntry_(
            entry, this.targetDirEntry, entryChangedCallback,
            function() {
              // Erase the processing entry.
              this.processingEntries[index] = {};
              this.processedBytes++;
              callback();
            }.bind(this),
            errorCallback);
      },
      function() {
        successCallback();
      }.bind(this),
      this);
};

/**
 * Moves the sourceEntry to the targetDirEntry in this task.
 *
 * @param {Entry} sourceEntry An entry to be moved.
 * @param {DirectoryEntry} destinationEntry The entry of the destination
 *     directory.
 * @param {function(util.EntryChangedKind, Entry)} entryChangedCallback
 *     Callback invoked when an entry is changed.
 * @param {function()} successCallback On success.
 * @param {function(FileOperationManager.Error)} errorCallback On error.
 * @private
 */
FileOperationManager.MoveTask.processEntry_ = function(
    sourceEntry, destinationEntry, entryChangedCallback, successCallback,
    errorCallback) {
  fileOperationUtil.deduplicatePath(
      destinationEntry,
      sourceEntry.name,
      function(destinationName) {
        sourceEntry.moveTo(
            destinationEntry, destinationName,
            function(movedEntry) {
              entryChangedCallback(util.EntryChangedKind.CREATED, movedEntry);
              entryChangedCallback(util.EntryChangedKind.DELETED, sourceEntry);
              successCallback();
            },
            function(error) {
              errorCallback(new FileOperationManager.Error(
                  util.FileOperationErrorType.FILESYSTEM_ERROR, error));
            });
      },
      errorCallback);
};

/**
 * Task to create a zip archive.
 *
 * @param {Array.<Entry>} sourceEntries Array of source entries.
 * @param {DirectoryEntry} targetDirEntry Target directory.
 * @param {DirectoryEntry} zipBaseDirEntry Base directory dealt as a root
 *     in ZIP archive.
 * @constructor
 * @extends {FileOperationManager.Task}
 */
FileOperationManager.ZipTask = function(
    sourceEntries, targetDirEntry, zipBaseDirEntry) {
  FileOperationManager.Task.call(
      this, util.FileOperationType.ZIP, sourceEntries, targetDirEntry);
  this.zipBaseDirEntry = zipBaseDirEntry;
};

/**
 * Extends FileOperationManager.Task.
 */
FileOperationManager.ZipTask.prototype.__proto__ =
    FileOperationManager.Task.prototype;


/**
 * Initializes the ZipTask.
 * @param {function()} callback Called when the initialize is completed.
 */
FileOperationManager.ZipTask.prototype.initialize = function(callback) {
  var resolvedEntryMap = {};
  var group = new AsyncUtil.Group();
  for (var i = 0; i < this.sourceEntries.length; i++) {
    group.add(function(index, callback) {
      fileOperationUtil.resolveRecursively(
          this.sourceEntries[index],
          function(entries) {
            for (var j = 0; j < entries.length; j++)
              resolvedEntryMap[entries[j].toURL()] = entries[j];
            callback();
          },
          function(error) {});
    }.bind(this, i));
  }

  group.run(function() {
    // For zip archiving, all the entries are processed at once.
    this.processingEntries = [resolvedEntryMap];

    this.totalBytes = 0;
    for (var url in resolvedEntryMap)
      this.totalBytes += resolvedEntryMap[url].size;

    callback();
  }.bind(this));
};

/**
 * Runs a zip file creation task.
 *
 * @param {function(util.EntryChangedKind, Entry)} entryChangedCallback
 *     Callback invoked when an entry is changed.
 * @param {function()} progressCallback Callback invoked periodically during
 *     the moving.
 * @param {function()} successCallback On complete.
 * @param {function(FileOperationManager.Error)} errorCallback On error.
 * @override
 */
FileOperationManager.ZipTask.prototype.run = function(
    entryChangedCallback, progressCallback, successCallback, errorCallback) {
  // TODO(hidehiko): we should localize the name.
  var destName = 'Archive';
  if (this.sourceEntries.length == 1) {
    var entryPath = this.sourceEntries[0].fullPath;
    var i = entryPath.lastIndexOf('/');
    var basename = (i < 0) ? entryPath : entryPath.substr(i + 1);
    i = basename.lastIndexOf('.');
    destName = ((i < 0) ? basename : basename.substr(0, i));
  }

  fileOperationUtil.deduplicatePath(
      this.targetDirEntry, destName + '.zip',
      function(destPath) {
        // TODO: per-entry zip progress update with accurate byte count.
        // For now just set completedBytes to same value as totalBytes so
        // that the progress bar is full.
        this.processedBytes = this.totalBytes;
        progressCallback();

        // The number of elements in processingEntries is 1. See also
        // initialize().
        var entries = [];
        for (var url in this.processingEntries[0])
          entries.push(this.processingEntries[0][url]);

        fileOperationUtil.zipSelection(
            entries,
            this.zipBaseDirEntry,
            destPath,
            function(entry) {
              entryChangedCallback(util.EntryChangedKind.CREATE, entry);
              successCallback();
            },
            function(error) {
              errorCallback(new FileOperationManager.Error(
                  util.FileOperationErrorType.FILESYSTEM_ERROR, error));
            });
      }.bind(this),
      errorCallback);
};

/**
 * Error class used to report problems with a copy operation.
 * If the code is UNEXPECTED_SOURCE_FILE, data should be a path of the file.
 * If the code is TARGET_EXISTS, data should be the existing Entry.
 * If the code is FILESYSTEM_ERROR, data should be the FileError.
 *
 * @param {util.FileOperationErrorType} code Error type.
 * @param {string|Entry|FileError} data Additional data.
 * @constructor
 */
FileOperationManager.Error = function(code, data) {
  this.code = code;
  this.data = data;
};

// FileOperationManager methods.

/**
 * Called before a new method is run in the manager. Prepares the manager's
 * state for running a new method.
 */
FileOperationManager.prototype.willRunNewMethod = function() {
  // Cancel any pending close actions so the file copy manager doesn't go away.
  if (this.unloadTimeout_)
    clearTimeout(this.unloadTimeout_);
  this.unloadTimeout_ = null;
};

/**
 * @return {Object} Status object.
 */
FileOperationManager.prototype.getStatus = function() {
  // TODO(hidehiko): Reorganize the structure when delete queue is merged
  // into copy task queue.
  var result = {
    // Set to util.FileOperationType if all the running/pending tasks is
    // the same kind of task.
    operationType: null,

    // The number of entries to be processed.
    numRemainingItems: 0,

    // The total number of bytes to be processed.
    totalBytes: 0,

    // The number of bytes.
    processedBytes: 0,

    // Available if numRemainingItems == 1. Pointing to an Entry which is
    // begin processed.
    processingEntry: task.getSingleEntry()
  };

  var operationType =
      this.copyTasks_.length > 0 ? this.copyTasks_[0].operationType : null;
  var task = null;
  for (var i = 0; i < this.copyTasks_.length; i++) {
    task = this.copyTasks_[i];
    if (task.operationType != operationType)
      operationType = null;

    // Assuming the number of entries is small enough, count every time.
    result.numRemainingItems += task.countRemainingItems();
    result.totalBytes += task.totalBytes;
    result.processedBytes += task.processedBytes;
  }

  result.operationType = operationType;
  return result;
};

/**
 * Adds an event listener for the tasks.
 * @param {string} type The name of the event.
 * @param {function(Event)} handler The handler for the event.
 *     This is called when the event is dispatched.
 */
FileOperationManager.prototype.addEventListener = function(type, handler) {
  this.eventRouter_.addEventListener(type, handler);
};

/**
 * Removes an event listener for the tasks.
 * @param {string} type The name of the event.
 * @param {function(Event)} handler The handler to be removed.
 */
FileOperationManager.prototype.removeEventListener = function(type, handler) {
  this.eventRouter_.removeEventListener(type, handler);
};

/**
 * Says if there are any tasks in the queue.
 * @return {boolean} True, if there are any tasks.
 */
FileOperationManager.prototype.hasQueuedTasks = function() {
  return this.copyTasks_.length > 0 || this.deleteTasks_.length > 0;
};

/**
 * Unloads the host page in 5 secs of idling. Need to be called
 * each time this.copyTasks_.length or this.deleteTasks_.length
 * changed.
 *
 * @private
 */
FileOperationManager.prototype.maybeScheduleCloseBackgroundPage_ = function() {
  if (!this.hasQueuedTasks()) {
    if (this.unloadTimeout_ === null)
      this.unloadTimeout_ = setTimeout(maybeCloseBackgroundPage, 5000);
  } else if (this.unloadTimeout_) {
    clearTimeout(this.unloadTimeout_);
    this.unloadTimeout_ = null;
  }
};

/**
 * Completely clear out the copy queue, either because we encountered an error
 * or completed successfully.
 *
 * @private
 */
FileOperationManager.prototype.resetQueue_ = function() {
  this.copyTasks_ = [];
  this.maybeScheduleCloseBackgroundPage_();
};

/**
 * Requests the specified task to be canceled.
 * @param {string} taskId ID of task to be canceled.
 */
FileOperationManager.prototype.requestTaskCancel = function(taskId) {
  var task = null;
  for (var i = 0; i < this.copyTasks_.length; i++) {
    task = this.copyTasks_[i];
    if (task.taskId !== taskId)
      continue;
    task.requestCancel();
    // If the task is not on progress, remove it immediately.
    if (i !== 0) {
      this.eventRouter_.sendProgressEvent('CANCELED',
                                          task.getStatus(),
                                          task.taskId);
      this.copyTasks_.splice(i, 1);
    }
  }
  for (var i = 0; i < this.deleteTasks_.length; i++) {
    task = this.deleteTasks_[i];
    if (task.taskId !== taskId)
      continue;
    task.requestCancel();
    // If the task is not on progress, remove it immediately.
    if (i != 0) {
      this.eventRouter_.sendDeleteEvent(
          'CANCELED',
          task.entries.map(function(entry) {
                             return util.makeFilesystemUrl(entry.fullPath);
                           }),
          task.taskId);
      this.deleteTasks_.splice(i, 1);
    }
  }
};

/**
 * Kick off pasting.
 *
 * @param {Array.<string>} sourcePaths Path of the source files.
 * @param {string} targetPath The destination path of the target directory.
 * @param {boolean} isMove True if the operation is "move", otherwise (i.e.
 *     if the operation is "copy") false.
 */
FileOperationManager.prototype.paste = function(
    sourcePaths, targetPath, isMove) {
  // Do nothing if sourcePaths is empty.
  if (sourcePaths.length == 0)
    return;

  var errorCallback = function(error) {
    this.eventRouter_.sendProgressEvent(
        'ERROR',
        this.getStatus(),
        this.generateTaskId_(null),
        new FileOperationManager.Error(
            util.FileOperationErrorType.FILESYSTEM_ERROR, error));
  }.bind(this);

  var targetEntry = null;
  var entries = [];

  // Resolve paths to entries.
  var resolveGroup = new AsyncUtil.Group();
  resolveGroup.add(function(callback) {
    webkitResolveLocalFileSystemURL(
        util.makeFilesystemUrl(targetPath),
        function(entry) {
          if (!entry.isDirectory) {
            // Found a non directory entry.
            errorCallback(util.createFileError(FileError.TYPE_MISMATCH_ERR));
            return;
          }

          targetEntry = entry;
          callback();
        },
        errorCallback);
  });

  for (var i = 0; i < sourcePaths.length; i++) {
    resolveGroup.add(function(sourcePath, callback) {
      webkitResolveLocalFileSystemURL(
          util.makeFilesystemUrl(sourcePath),
          function(entry) {
            entries.push(entry);
            callback();
          },
          errorCallback);
    }.bind(this, sourcePaths[i]));
  }

  resolveGroup.run(function() {
    if (isMove) {
      // Moving to the same directory is a redundant operation.
      entries = entries.filter(function(entry) {
        return targetEntry.fullPath + '/' + entry.name != entry.fullPath;
      });

      // Do nothing, if we have no entries to be moved.
      if (entries.length == 0)
        return;
    }

    this.queueCopy_(targetEntry, entries, isMove);
  }.bind(this));
};

/**
 * Checks if the move operation is available between the given two locations.
 *
 * @param {DirectoryEntry} sourceEntry An entry from the source.
 * @param {DirectoryEntry} targetDirEntry Directory entry for the target.
 * @return {boolean} Whether we can move from the source to the target.
 */
FileOperationManager.prototype.isMovable = function(sourceEntry,
                                               targetDirEntry) {
  return (PathUtil.isDriveBasedPath(sourceEntry.fullPath) &&
          PathUtil.isDriveBasedPath(targetDirEntry.fullPath)) ||
         (PathUtil.getRootPath(sourceEntry.fullPath) ==
          PathUtil.getRootPath(targetDirEntry.fullPath));
};

/**
 * Initiate a file copy.
 *
 * @param {DirectoryEntry} targetDirEntry Target directory.
 * @param {Array.<Entry>} entries Entries to copy.
 * @param {boolean} isMove In case of move.
 * @return {FileOperationManager.Task} Copy task.
 * @private
 */
FileOperationManager.prototype.queueCopy_ = function(
    targetDirEntry, entries, isMove) {
  // When copying files, null can be specified as source directory.
  var task;
  if (isMove) {
    if (this.isMovable(entries[0], targetDirEntry)) {
      task = new FileOperationManager.MoveTask(entries, targetDirEntry);
    } else {
      task = new FileOperationManager.CopyTask(entries, targetDirEntry);
      task.deleteAfterCopy = true;
    }
  } else {
    task = new FileOperationManager.CopyTask(entries, targetDirEntry);
  }

  task.taskId = this.generateTaskId_();
  task.initialize(function() {
    this.copyTasks_.push(task);
    this.maybeScheduleCloseBackgroundPage_();
    this.eventRouter_.sendProgressEvent('BEGIN', task.getStatus(), task.taskId);
    if (this.copyTasks_.length == 1)
      this.serviceAllTasks_();
  }.bind(this));

  return task;
};

/**
 * Service all pending tasks, as well as any that might appear during the
 * copy.
 *
 * @private
 */
FileOperationManager.prototype.serviceAllTasks_ = function() {
  if (!this.copyTasks_.length) {
    // All tasks have been serviced, clean up and exit.
    this.resetQueue_();
    return;
  }

  var onTaskProgress = function() {
    this.eventRouter_.sendProgressEvent('PROGRESS',
                                        this.copyTasks_[0].getStatus(),
                                        this.copyTasks_[0].taskId);
  }.bind(this);

  var onEntryChanged = function(kind, entry) {
    this.eventRouter_.sendEntryChangedEvent(kind, entry);
  }.bind(this);

  var onTaskError = function(err) {
    var task = this.copyTasks_.shift();
    var reason = err.data.code === FileError.ABORT_ERR ? 'CANCELED' : 'ERROR';
    this.eventRouter_.sendProgressEvent(reason,
                                        task.getStatus(),
                                        task.taskId,
                                        err);
    this.serviceAllTasks_();
  }.bind(this);

  var onTaskSuccess = function() {
    // The task at the front of the queue is completed. Pop it from the queue.
    var task = this.copyTasks_.shift();
    this.maybeScheduleCloseBackgroundPage_();
    this.eventRouter_.sendProgressEvent('SUCCESS',
                                        task.getStatus(),
                                        task.taskId);
    this.serviceAllTasks_();
  }.bind(this);

  var nextTask = this.copyTasks_[0];
  this.eventRouter_.sendProgressEvent('PROGRESS',
                                      nextTask.getStatus(),
                                      nextTask.taskId);
  nextTask.run(onEntryChanged, onTaskProgress, onTaskSuccess, onTaskError);
};

/**
 * Timeout before files are really deleted (to allow undo).
 */
FileOperationManager.DELETE_TIMEOUT = 30 * 1000;

/**
 * Schedules the files deletion.
 *
 * @param {Array.<Entry>} entries The entries.
 */
FileOperationManager.prototype.deleteEntries = function(entries) {
  var task = {
    entries: entries,
    taskId: this.generateTaskId_()
  };
  this.deleteTasks_.push(task);
  this.eventRouter_.sendDeleteEvent('BEGIN', entries.map(function(entry) {
    return util.makeFilesystemUrl(entry.fullPath);
  }), task.taskId);
  this.maybeScheduleCloseBackgroundPage_();
  if (this.deleteTasks_.length == 1)
    this.serviceAllDeleteTasks_();
};

/**
 * Service all pending delete tasks, as well as any that might appear during the
 * deletion.
 *
 * Must not be called if there is an in-flight delete task.
 *
 * @private
 */
FileOperationManager.prototype.serviceAllDeleteTasks_ = function() {
  // Returns the urls of the given task's entries.
  var getTaskUrls = function(task) {
    return task.entries.map(function(entry) {
      return util.makeFilesystemUrl(entry.fullPath);
    });
  };

  var onTaskSuccess = function() {
    var urls = getTaskUrls(this.deleteTasks_[0]);
    var taskId = this.deleteTasks_[0].taskId;
    this.deleteTasks_.shift();
    this.eventRouter_.sendDeleteEvent('SUCCESS', urls, taskId);

    if (!this.deleteTasks_.length) {
      // All tasks have been serviced, clean up and exit.
      this.maybeScheduleCloseBackgroundPage_();
      return;
    }

    var nextTask = this.deleteTasks_[0];
    this.eventRouter_.sendDeleteEvent('PROGRESS',
                                      urls,
                                      nextTask.taskId);
    this.serviceDeleteTask_(nextTask, onTaskSuccess, onTaskFailure);
  }.bind(this);

  var onTaskFailure = function(error) {
    var urls = getTaskUrls(this.deleteTasks_[0]);
    var taskId = this.deleteTasks_[0].taskId;
    this.deleteTasks_ = [];
    this.eventRouter_.sendDeleteEvent('ERROR',
                                      urls,
                                      taskId);
    this.maybeScheduleCloseBackgroundPage_();
  }.bind(this);

  this.serviceDeleteTask_(this.deleteTasks_[0], onTaskSuccess, onTaskFailure);
};

/**
 * Performs the deletion.
 *
 * @param {Object} task The delete task (see deleteEntries function).
 * @param {function()} successCallback Callback run on success.
 * @param {function(FileOperationManager.Error)} errorCallback Callback run on
 *     error.
 * @private
 */
FileOperationManager.prototype.serviceDeleteTask_ = function(
    task, successCallback, errorCallback) {
  var downcount = task.entries.length;
  if (downcount == 0) {
    successCallback();
    return;
  }

  var filesystemError = null;
  var onComplete = function() {
    if (--downcount > 0)
      return;

    // All remove operations are processed. Run callback.
    if (filesystemError) {
      errorCallback(new FileOperationManager.Error(
          util.FileOperationErrorType.FILESYSTEM_ERROR, filesystemError));
    } else {
      successCallback();
    }
  };

  for (var i = 0; i < task.entries.length; i++) {
    var entry = task.entries[i];
    util.removeFileOrDirectory(
        entry,
        function(currentEntry) {
          this.eventRouter_.sendEntryChangedEvent(
              util.EntryChangedKind.DELETED, currentEntry);
          onComplete();
        }.bind(this, entry),
        function(error) {
          if (!filesystemError)
            filesystemError = error;
          onComplete();
        });
  }
};

/**
 * Creates a zip file for the selection of files.
 *
 * @param {Entry} dirEntry The directory containing the selection.
 * @param {Array.<Entry>} selectionEntries The selected entries.
 */
FileOperationManager.prototype.zipSelection = function(
    dirEntry, selectionEntries) {
  var zipTask = new FileOperationManager.ZipTask(
      selectionEntries, dirEntry, dirEntry);
  zipTask.taskId = this.generateTaskId_(this.copyTasks_);
  zipTask.zip = true;
  zipTask.initialize(function() {
    this.copyTasks_.push(zipTask);
    this.eventRouter_.sendProgressEvent('BEGIN',
                                        zipTask.getStatus(),
                                        zipTask.taskId);
    if (this.copyTasks_.length == 1)
      this.serviceAllTasks_();
  }.bind(this));
};

/**
 * Generates new task ID.
 *
 * @return {string} New task ID.
 * @private
 */
FileOperationManager.prototype.generateTaskId_ = function() {
  return 'file-operation-' + this.taskIdCounter_++;
};

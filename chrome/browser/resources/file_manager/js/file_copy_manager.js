// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @constructor
 */
function FileCopyManager() {
  this.copyTasks_ = [];
  this.deleteTasks_ = [];
  this.cancelObservers_ = [];
  this.cancelRequested_ = false;
  this.cancelCallback_ = null;
  this.unloadTimeout_ = null;

  this.eventRouter_ = new FileCopyManager.EventRouter();
}

/**
 * Get FileCopyManager instance. In case is hasn't been initialized, a new
 * instance is created.
 *
 * @return {FileCopyManager} A FileCopyManager instance.
 */
FileCopyManager.getInstance = function() {
  if (!FileCopyManager.instance_)
    FileCopyManager.instance_ = new FileCopyManager();

  return FileCopyManager.instance_;
};

/**
 * Manages cr.Event dispatching.
 * Currently this can send three types of events: "copy-progress",
 * "copy-operation-completed" and "delete".
 *
 * TODO(hidehiko): Reorganize the event dispatching mechanism.
 * @constructor
 * @extends {cr.EventTarget}
 */
FileCopyManager.EventRouter = function() {
};

/**
 * Extends cr.EventTarget.
 */
FileCopyManager.EventRouter.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Dispatches a simple "copy-progress" event with reason and current
 * FileCopyManager status. If it is an ERROR event, error should be set.
 *
 * @param {string} reason Event type. One of "BEGIN", "PROGRESS", "SUCCESS",
 *     "ERROR" or "CANCELLED". TODO(hidehiko): Use enum.
 * @param {Object} status Current FileCopyManager's status. See also
 *     FileCopyManager.getStatus().
 * @param {FileCopyManager.Error=} opt_error The info for the error. This
 *     should be set iff the reason is "ERROR".
 */
FileCopyManager.EventRouter.prototype.sendProgressEvent = function(
    reason, status, opt_error) {
  var event = new cr.Event('copy-progress');
  event.reason = reason;
  event.status = status;
  if (opt_error)
    event.error = opt_error;
  this.dispatchEvent(event);
};

/**
 * Dispatches an event to notify that an entry is changed (created or deleted).
 * @param {util.EntryChangedType} type The enum to represent if the entry
 *     is created or deleted.
 * @param {Entry} entry The changed entry.
 */
FileCopyManager.EventRouter.prototype.sendEntryChangedEvent = function(
    type, entry) {
  var event = new cr.Event('entry-changed');
  event.type = type;
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
 */
FileCopyManager.EventRouter.prototype.sendDeleteEvent = function(
    reason, urls) {
  var event = new cr.Event('delete');
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
 * @param {DirectoryEntry} targetDirEntry Target directory.
 * @param {DirectoryEntry=} opt_zipBaseDirEntry Base directory dealt as a root
 *     in ZIP archive.
 * @constructor
 */
FileCopyManager.Task = function(targetDirEntry, opt_zipBaseDirEntry) {
  this.targetDirEntry = targetDirEntry;
  this.zipBaseDirEntry = opt_zipBaseDirEntry;
  this.originalEntries = null;

  this.pendingDirectories = [];
  this.pendingFiles = [];
  this.pendingBytes = 0;

  this.completedDirectories = [];
  this.completedFiles = [];
  this.completedBytes = 0;

  this.deleteAfterCopy = false;
  this.move = false;
  this.zip = false;

  // If directory already exists, we try to make a copy named 'dir (X)',
  // where X is a number. When we do this, all subsequent copies from
  // inside the subtree should be mapped to the new directory name.
  // For example, if 'dir' was copied as 'dir (1)', then 'dir\file.txt' should
  // become 'dir (1)\file.txt'.
  this.renamedDirectories_ = [];
};

/**
 * Simple wrapper for util.deduplicatePath. On error, this method translates
 * the FileError to FileCopyManager.Error object.
 *
 * @param {DirectoryEntry} dirEntry The target directory entry.
 * @param {string} relativePath The path to be deduplicated.
 * @param {function(string)} successCallback Callback run with the deduplicated
 *     path on success.
 * @param {function(FileCopyManager.Error)} errorCallback Callback run on error.
 */
FileCopyManager.Task.deduplicatePath = function(
    dirEntry, relativePath, successCallback, errorCallback) {
  util.deduplicatePath(
      dirEntry, relativePath, successCallback,
      function(err) {
        var onFileSystemError = function(error) {
          errorCallback(new FileCopyManager.Error(
              util.FileOperationErrorType.FILESYSTEM_ERROR, error));
        };

        if (err.code == FileError.PATH_EXISTS_ERR) {
          // Failed to uniquify the file path. There should be an existing
          // entry, so return the error with it.
          util.resolvePath(
              dirEntry, relativePath,
              function(entry) {
                errorCallback(new FileCopyManager.Error(
                    util.FileOperationErrorType.TARGET_EXISTS, entry));
              },
              onFileSystemError);
          return;
        }
        onFileSystemError(err);
      });
};

/**
 * @param {Array.<Entry>} entries Entries.
 * @param {function()} callback When entries resolved.
 */
FileCopyManager.Task.prototype.setEntries = function(entries, callback) {
  var self = this;
  this.originalEntries = entries;
  // When moving directories, FileEntry.moveTo() is used if both source
  // and target are on Drive. There is no need to recurse into directories.
  util.recurseAndResolveEntries(entries, !this.move, function(result) {
    self.pendingDirectories = result.dirEntries;
    self.pendingFiles = result.fileEntries;
    self.totalBytes = result.fileBytes;
    self.completedBytes = 0;

    callback();
  });
};

/**
 * @return {Entry} Next entry.
 */
FileCopyManager.Task.prototype.getNextEntry = function() {
  // We should keep the file in pending list and remove it after complete.
  // Otherwise, if we try to get status in the middle of copying. The returned
  // status is wrong (miss count the pasting item in totalItems).
  var nextEntry = null;
  if (this.move) {
    // This may be moving from search results, where it fails if we move parent
    // entries earlier than child entries. We should process the deepest entry
    // first. Since move of each entry is done by a single .MoveTo() call, we
    // don't need to care about the recursive traversal order.
    nextEntry = this.getDeepestEntry_();
  } else {
    // Copying tasks are recursively processed. So, directories must be
    // processed earlier than their child files. Since
    // util.recurseAndResolveEntries is already listing entries in the recursive
    // traversal order, we just keep the ordering.
    if (this.pendingDirectories.length)
      nextEntry = this.pendingDirectories[0];
    else if (this.pendingFiles.length)
      nextEntry = this.pendingFiles[0];
  }
  if (nextEntry)
    nextEntry.inProgress = true;
  return nextEntry;
};

/**
 * Remove the completed entry from the pending lists.
 * @param {Entry} entry Entry.
 * @param {number} size Bytes completed.
 */
FileCopyManager.Task.prototype.markEntryComplete = function(entry, size) {
  if (entry.isDirectory && this.pendingDirectories) {
    for (var i = 0; i < this.pendingDirectories.length; i++) {
      if (this.pendingDirectories[i].inProgress) {
        this.completedDirectories.push(entry);
        this.pendingDirectories.splice(i, 1);
        return;
      }
    }
  } else if (this.pendingFiles) {
    for (var i = 0; i < this.pendingFiles.length; i++) {
      if (this.pendingFiles[i].inProgress) {
        this.completedFiles.push(entry);
        this.completedBytes += size;
        this.pendingFiles.splice(i, 1);
        return;
      }
    }
  }
  throw new Error('Try to remove a source entry which is not correspond to' +
                  ' the finished target entry');
};

/**
 * Updates copy progress status for the entry.
 *
 * @param {Entry} entry Entry which is being coppied.
 * @param {number} size Number of bytes that has been copied since last update.
 */
FileCopyManager.Task.prototype.updateFileCopyProgress = function(entry, size) {
  if (entry.isFile && this.pendingFiles && this.pendingFiles[0].inProgress)
    this.completedBytes += size;
};

/**
 * @param {string} fromName Old name.
 * @param {string} toName New name.
 */
FileCopyManager.Task.prototype.registerRename = function(fromName, toName) {
  this.renamedDirectories_.push({from: fromName + '/', to: toName + '/'});
};

/**
 * @param {string} path A path.
 * @return {string} Path after renames.
 */
FileCopyManager.Task.prototype.applyRenames = function(path) {
  // Directories are processed in pre-order, so we will store only the first
  // renaming point:
  // x   -> x (1)    -- new directory created.
  // x\y -> x (1)\y  -- no more renames inside the new directory, so
  //                    this one will not be stored.
  // x\y\a.txt       -- only one rename will be applied.
  for (var index = 0; index < this.renamedDirectories_.length; ++index) {
    var rename = this.renamedDirectories_[index];
    if (path.indexOf(rename.from) == 0) {
      path = rename.to + path.substr(rename.from.length);
    }
  }
  return path;
};

/**
 * Obtains the deepest entry by referring to its full path.
 * @return {Entry} The deepest entry.
 * @private
 */
FileCopyManager.Task.prototype.getDeepestEntry_ = function() {
  var result = null;
  for (var i = 0; i < this.pendingDirectories.length; i++) {
    if (!result ||
        this.pendingDirectories[i].fullPath.length > result.fullPath.length)
    result = this.pendingDirectories[i];
  }
  for (var i = 0; i < this.pendingFiles.length; i++) {
    if (!result ||
        this.pendingFiles[i].fullPath.length > result.fullPath.length)
    result = this.pendingFiles[i];
  }
  return result;
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
FileCopyManager.Error = function(code, data) {
  this.code = code;
  this.data = data;
};

// FileCopyManager methods.

/**
 * Initializes the filesystem if it is not done yet.
 * @param {function()} callback Completion callback.
 */
FileCopyManager.prototype.initialize = function(callback) {
  // Already initialized.
  if (this.root_) {
    callback();
    return;
  }
  chrome.fileBrowserPrivate.requestFileSystem(function(filesystem) {
    this.root_ = filesystem.root;
    callback();
  }.bind(this));
};

/**
 * Called before a new method is run in the manager. Prepares the manager's
 * state for running a new method.
 */
FileCopyManager.prototype.willRunNewMethod = function() {
  // Cancel any pending close actions so the file copy manager doesn't go away.
  if (this.unloadTimeout_)
    clearTimeout(this.unloadTimeout_);
  this.unloadTimeout_ = null;
};

/**
 * @return {Object} Status object.
 */
FileCopyManager.prototype.getStatus = function() {
  var rv = {
    pendingItems: 0,  // Files + Directories
    pendingFiles: 0,
    pendingDirectories: 0,
    pendingBytes: 0,

    completedItems: 0,  // Files + Directories
    completedFiles: 0,
    completedDirectories: 0,
    completedBytes: 0,

    percentage: NaN,
    pendingCopies: 0,
    pendingMoves: 0,
    pendingZips: 0,
    filename: ''  // In case pendingItems == 1
  };

  var pendingFile = null;

  for (var i = 0; i < this.copyTasks_.length; i++) {
    var task = this.copyTasks_[i];
    var pendingFiles = task.pendingFiles.length;
    var pendingDirectories = task.pendingDirectories.length;
    rv.pendingFiles += pendingFiles;
    rv.pendingDirectories += pendingDirectories;
    rv.pendingBytes += (task.totalBytes - task.completedBytes);

    rv.completedFiles += task.completedFiles.length;
    rv.completedDirectories += task.completedDirectories.length;
    rv.completedBytes += task.completedBytes;

    if (task.zip) {
      rv.pendingZips += pendingFiles + pendingDirectories;
    } else if (task.move || task.deleteAfterCopy) {
      rv.pendingMoves += pendingFiles + pendingDirectories;
    } else {
      rv.pendingCopies += pendingFiles + pendingDirectories;
    }

    if (task.pendingFiles.length === 1)
      pendingFile = task.pendingFiles[0];

    if (task.pendingDirectories.length === 1)
      pendingFile = task.pendingDirectories[0];

  }
  rv.pendingItems = rv.pendingFiles + rv.pendingDirectories;
  rv.completedItems = rv.completedFiles + rv.completedDirectories;

  rv.totalFiles = rv.pendingFiles + rv.completedFiles;
  rv.totalDirectories = rv.pendingDirectories + rv.completedDirectories;
  rv.totalItems = rv.pendingItems + rv.completedItems;
  rv.totalBytes = rv.pendingBytes + rv.completedBytes;

  rv.percentage = rv.completedBytes / rv.totalBytes;
  if (rv.pendingItems === 1)
    rv.filename = pendingFile.name;

  return rv;
};

/**
 * Adds an event listener for the tasks.
 * @param {string} type The name of the event.
 * @param {function(cr.Event)} handler The handler for the event.
 *     This is called when the event is dispatched.
 */
FileCopyManager.prototype.addEventListener = function(type, handler) {
  this.eventRouter_.addEventListener(type, handler);
};

/**
 * Removes an event listener for the tasks.
 * @param {string} type The name of the event.
 * @param {function(cr.Event)} handler The handler to be removed.
 */
FileCopyManager.prototype.removeEventListener = function(type, handler) {
  this.eventRouter_.removeEventListener(type, handler);
};

/**
 * Says if there are any tasks in the queue.
 * @return {boolean} True, if there are any tasks.
 */
FileCopyManager.prototype.hasQueuedTasks = function() {
  return this.copyTasks_.length > 0 || this.deleteTasks_.length > 0;
};

/**
 * Unloads the host page in 5 secs of idleing. Need to be called
 * each time this.copyTasks_.length or this.deleteTasks_.length
 * changed.
 *
 * @private
 */
FileCopyManager.prototype.maybeScheduleCloseBackgroundPage_ = function() {
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
FileCopyManager.prototype.resetQueue_ = function() {
  for (var i = 0; i < this.cancelObservers_.length; i++)
    this.cancelObservers_[i]();

  this.copyTasks_ = [];
  this.cancelObservers_ = [];
  this.maybeScheduleCloseBackgroundPage_();
};

/**
 * Request that the current copy queue be abandoned.
 *
 * @param {function()=} opt_callback On cancel.
 */
FileCopyManager.prototype.requestCancel = function(opt_callback) {
  this.cancelRequested_ = true;
  if (this.cancelCallback_)
    this.cancelCallback_();
  if (opt_callback)
    this.cancelObservers_.push(opt_callback);

  // If there is any active task it will eventually call maybeCancel_.
  // Otherwise call it right now.
  if (this.copyTasks_.length == 0)
    this.doCancel_();
};

/**
 * Perform the bookkeeping required to cancel.
 *
 * @private
 */
FileCopyManager.prototype.doCancel_ = function() {
  this.resetQueue_();
  this.cancelRequested_ = false;
  this.eventRouter_.sendProgressEvent('CANCELLED', this.getStatus());
};

/**
 * Used internally to check if a cancel has been requested, and handle
 * it if so.
 *
 * @return {boolean} If canceled.
 * @private
 */
FileCopyManager.prototype.maybeCancel_ = function() {
  if (!this.cancelRequested_)
    return false;

  this.doCancel_();
  return true;
};

/**
 * Kick off pasting.
 *
 * @param {Array.<string>} files Pathes of source files.
 * @param {Array.<string>} directories Pathes of source directories.
 * @param {boolean} isCut If the source items are removed from original
 *     location.
 * @param {string} targetPath Target path.
 */
FileCopyManager.prototype.paste = function(
    files, directories, isCut, targetPath) {
  var self = this;
  var entries = [];
  var added = 0;
  var total;

  var steps = {
    start: function() {
      // Filter entries.
      var entryFilterFunc = function(entry) {
        if (entry == '')
          return false;
        if (isCut && entry.replace(/\/[^\/]+$/, '') == targetPath)
          // Moving to the same directory is a redundant operation.
          return false;
        return true;
      };
      directories = directories ? directories.filter(entryFilterFunc) : [];
      files = files ? files.filter(entryFilterFunc) : [];

      // Check the number of filtered entries.
      total = directories.length + files.length;
      if (total == 0)
        return;

      // Retrieve entries.
      util.getDirectories(self.root_, {create: false}, directories,
                          steps.onEntryFound, steps.onPathError);
      util.getFiles(self.root_, {create: false}, files,
                    steps.onEntryFound, steps.onPathError);
    },

    onEntryFound: function(entry) {
      // When getDirectories/getFiles finish, they call addEntry with null.
      // We don't want to add null to our entries.
      if (entry == null)
        return;
      entries.push(entry);
      added++;
      if (added == total)
        steps.onSourceEntriesFound();
    },

    onSourceEntriesFound: function() {
      self.root_.getDirectory(targetPath, {},
                              steps.onTargetEntryFound, steps.onPathError);
    },

    onTargetEntryFound: function(targetEntry) {
      self.queueCopy_(targetEntry, entries, isCut);
    },

    onPathError: function(err) {
      self.eventRouter_.sendProgressEvent(
          'ERROR',
          self.getStatus(),
          new FileCopyManager.Error(
              util.FileOperationErrorType.FILESYSTEM_ERROR, err));
    }
  };

  steps.start();
};

/**
 * Checks if the move operation is avaiable between the given two locations.
 *
 * @param {DirectoryEntry} sourceEntry An entry from the source.
 * @param {DirectoryEntry} targetDirEntry Directory entry for the target.
 * @return {boolean} Whether we can move from the source to the target.
 */
FileCopyManager.prototype.isMovable = function(sourceEntry,
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
 * @param {boolean} deleteAfterCopy In case of move.
 * @return {FileCopyManager.Task} Copy task.
 * @private
 */
FileCopyManager.prototype.queueCopy_ = function(
    targetDirEntry, entries, deleteAfterCopy) {
  var self = this;
  // When copying files, null can be specified as source directory.
  var copyTask = new FileCopyManager.Task(targetDirEntry);
  if (deleteAfterCopy) {
    if (this.isMovable(entries[0], targetDirEntry)) {
      copyTask.move = true;
    } else {
      copyTask.deleteAfterCopy = true;
    }
  }
  copyTask.setEntries(entries, function() {
    self.copyTasks_.push(copyTask);
    self.maybeScheduleCloseBackgroundPage_();
    if (self.copyTasks_.length == 1) {
      // Assume self.cancelRequested_ == false.
      // This moved us from 0 to 1 active tasks, let the servicing begin!
      self.serviceAllTasks_();
    } else {
      // Force to update the progress of butter bar when there are new tasks
      // coming while servicing current task.
      self.eventRouter_.sendProgressEvent('PROGRESS', self.getStatus());
    }
  });

  return copyTask;
};

/**
 * Service all pending tasks, as well as any that might appear during the
 * copy.
 *
 * @private
 */
FileCopyManager.prototype.serviceAllTasks_ = function() {
  var self = this;

  var onTaskProgress = function() {
    self.eventRouter_.sendProgressEvent('PROGRESS', self.getStatus());
  };

  var onEntryChanged = function(type, entry) {
    self.eventRouter_.sendEntryChangedEvent(type, entry);
  };

  var onTaskError = function(err) {
    if (self.maybeCancel_())
      return;
    self.eventRouter_.sendProgressEvent('ERROR', self.getStatus(), err);
    self.resetQueue_();
  };

  var onTaskSuccess = function() {
    if (self.maybeCancel_())
      return;

    // The task at the front of the queue is completed. Pop it from the queue.
    self.copyTasks_.shift();
    self.maybeScheduleCloseBackgroundPage_();

    if (!self.copyTasks_.length) {
      // All tasks have been serviced, clean up and exit.
      self.eventRouter_.sendProgressEvent('SUCCESS', self.getStatus());
      self.resetQueue_();
      return;
    }

    // We want to dispatch a PROGRESS event when there are more tasks to serve
    // right after one task finished in the queue. We treat all tasks as one
    // big task logically, so there is only one BEGIN/SUCCESS event pair for
    // these continuous tasks.
    self.eventRouter_.sendProgressEvent('PROGRESS', self.getStatus());

    self.serviceTask_(self.copyTasks_[0], onEntryChanged, onTaskProgress,
                      onTaskSuccess, onTaskError);
  };

  // If the queue size is 1 after pushing our task, it was empty before,
  // so we need to kick off queue processing and dispatch BEGIN event.

  this.eventRouter_.sendProgressEvent('BEGIN', this.getStatus());
  this.serviceTask_(this.copyTasks_[0], onEntryChanged, onTaskProgress,
                    onTaskSuccess, onTaskError);
};

/**
 * Runs a given task.
 * Note that the responsibility of this method is just dispatching to the
 * appropriate serviceXxxTask_() method.
 * TODO(hidehiko): Remove this method by introducing FileCopyManager.Task.run()
 *     (crbug.com/246976).
 *
 * @param {FileCopyManager.Task} task A task to be run.
 * @param {function(util.EntryChangedType, Entry)} entryChangedCallback Callback
 *     invoked when an entry is changed.
 * @param {function()} progressCallback Callback invoked periodically during
 *     the operation.
 * @param {function()} successCallback Callback run on success.
 * @param {function(FileCopyManager.Error)} errorCallback Callback run on error.
 * @private
 */
FileCopyManager.prototype.serviceTask_ = function(
    task, entryChangedCallback, progressCallback,
    successCallback, errorCallback) {
  if (task.zip)
    this.serviceZipTask_(task, entryChangedCallback, progressCallback,
                         successCallback, errorCallback);
  else if (task.move)
    this.serviceMoveTask_(task, entryChangedCallback, progressCallback,
                          successCallback, errorCallback);
  else
    this.serviceCopyTask_(task, entryChangedCallback, progressCallback,
                          successCallback, errorCallback);
};

/**
 * Service all entries in the copy (and move) task.
 * Note: this method contains also the operation of "Move" due to historical
 * reason.
 *
 * @param {FileCopyManager.Task} task A copy task to be run.
 * @param {function(util.EntryChangedType, Entry)} entryChangedCallback Callback
 *     invoked when an entry is changed.
 * @param {function()} progressCallback Callback invoked periodically during
 *     the copying.
 * @param {function()} successCallback On success.
 * @param {function(FileCopyManager.Error)} errorCallback On error.
 * @private
 */
FileCopyManager.prototype.serviceCopyTask_ = function(
    task, entryChangedCallback, progressCallback, successCallback,
    errorCallback) {
  // TODO(hidehiko): We should be able to share the code to iterate on entries
  // with serviceMoveTask_().
  if (task.pendingDirectories.length + task.pendingFiles.length == 0) {
    successCallback();
    return;
  }

  var self = this;

  var deleteOriginals = function() {
    var count = task.originalEntries.length;

    var onEntryDeleted = function(entry) {
      entryChangedCallback(util.EntryChangedType.DELETED, entry);
      count--;
      if (!count)
        successCallback();
    };

    var onFilesystemError = function(err) {
      errorCallback(new FileCopyManager.Error(
          util.FileOperationErrorType.FILESYSTEM_ERROR, err));
    };

    for (var i = 0; i < task.originalEntries.length; i++) {
      var entry = task.originalEntries[i];
      util.removeFileOrDirectory(
          entry, onEntryDeleted.bind(self, entry), onFilesystemError);
    }
  };

  var onEntryServiced = function() {
    // We should not dispatch a PROGRESS event when there is no pending items
    // in the task.
    if (task.pendingDirectories.length + task.pendingFiles.length == 0) {
      if (task.deleteAfterCopy) {
        deleteOriginals();
      } else {
        successCallback();
      }
      return;
    }

    progressCallback();
    self.processCopyEntry_(
        task, task.getNextEntry(), entryChangedCallback, progressCallback,
        onEntryServiced, errorCallback);
  };

  this.processCopyEntry_(
      task, task.getNextEntry(), entryChangedCallback, progressCallback,
      onEntryServiced, errorCallback);
};

/**
 * Copies the next entry in a given task.
 * TODO(olege): Refactor this method into a separate class.
 *
 * @param {FileManager.Task} task A task.
 * @param {Entry} sourceEntry An entry to be copied.
 * @param {function(util.EntryChangedType, Entry)} entryChangedCallback Callback
 *     invoked when an entry is changed.
 * @param {function()} progressCallback Callback invoked periodically during
 *     the copying.
 * @param {function()} successCallback On success.
 * @param {function(FileCopyManager.Error)} errorCallback On error.
 * @private
 */
FileCopyManager.prototype.processCopyEntry_ = function(
    task, sourceEntry, entryChangedCallback, progressCallback, successCallback,
    errorCallback) {
  if (this.maybeCancel_())
    return;

  var self = this;

  // |sourceEntry.originalSourcePath| is set in util.recurseAndResolveEntries.
  var sourcePath = sourceEntry.originalSourcePath;
  if (sourceEntry.fullPath.substr(0, sourcePath.length) != sourcePath) {
    // We found an entry in the list that is not relative to the base source
    // path, something is wrong.
    errorCallback(new FileCopyManager.Error(
        util.FileOperationErrorType.UNEXPECTED_SOURCE_FILE,
        sourceEntry.fullPath));
    return;
  }

  var targetDirEntry = task.targetDirEntry;
  var originalPath = sourceEntry.fullPath.substr(sourcePath.length + 1);
  originalPath = task.applyRenames(originalPath);

  var onDeduplicated = function(targetRelativePath) {
    var onCopyComplete = function(entry, size) {
      entryChangedCallback(util.EntryChangedType.CREATED, entry);
      task.markEntryComplete(entry, size);
      successCallback();
    };

    var onFilesystemError = function(err) {
      errorCallback(new FileCopyManager.Error(
          util.FileOperationErrorType.FILESYSTEM_ERROR, err));
    };

    if (sourceEntry.isDirectory) {
      // Copying the directory means just creating a new directory.
      targetDirEntry.getDirectory(
          targetRelativePath,
          {create: true, exclusive: true},
          function(targetEntry) {
            if (targetRelativePath != originalPath) {
              task.registerRename(originalPath, targetRelativePath);
            }
            onCopyComplete(targetEntry, 0);
          },
          util.flog('Error getting dir: ' + targetRelativePath,
                    onFilesystemError));
      return;
    }

    var onCopyProgress = function(entry, size) {
      task.updateFileCopyProgress(entry, size);
      progressCallback();
    };

    // Hereafter copy a file.
    var isSourceOnDrive = PathUtil.isDriveBasedPath(sourceEntry.fullPath);
    var isTargetOnDrive = PathUtil.isDriveBasedPath(targetDirEntry.fullPath);

    if (!isSourceOnDrive && !isTargetOnDrive) {
      // Sending a file from local to local.
      // To copy local file, we use File blob and FileWriter to take the
      // progress.
      targetDirEntry.getFile(
          targetRelativePath,
          {create: true, exclusive: true},
          function(targetEntry) {
            self.copyFileEntry_(
                sourceEntry, targetEntry,
                onCopyProgress, onCopyComplete, onFilesystemError);
          },
          util.flog('Error getting file: ' + targetRelativePath,
                    onFilesystemError));
      return;
    }

    // Sending a file from a) Drive to Drive, b) Drive to local or c) local to
    // Drive.
    var sourceFileUrl = sourceEntry.toURL();
    var sourceFilePath = util.extractFilePath(sourceFileUrl);

    // Progress callback.
    // Because the uploading the file from local cache to Drive server will be
    // done as a part of background Drive file system sync, so for this copy
    // operation, what we need to take care about is only file downloading.
    var numTransferredBytes = 0;
    var onFileTransfersUpdated = null;
    if (isSourceOnDrive) {
      onFileTransfersUpdated = function(statusList) {
        for (var i = 0; i < statusList.length; i++) {
          var status = statusList[i];

          // Comparing urls is unreliable, since they may use different
          // url encoding schemes (eg. rfc2396 vs. rfc3986).
          var filePath = util.extractFilePath(status.fileUrl);
          if (filePath == sourceFilePath) {
            var processed = status.processed;
            if (processed > numTransferredBytes) {
              onCopyProgress(sourceEntry, processed - numTransferredBytes);
              numTransferredBytes = processed;
            }
            return;
          }
        }
      };

      // Currently, we do NOT upload the file during the copy operation.
      // It will be done as a part of file system sync after copy operation.
      // So, we can cancel only file downloading.
      self.cancelCallback_ = function() {
        self.cancelCallback_ = null;
        chrome.fileBrowserPrivate.cancelFileTransfers(
            [sourceFileUrl], function() {});
      };
    }

    // If this is the copy operation from Drive file system,
    // we use copyTo method.
    targetDirEntry.getDirectory(
        PathUtil.dirname(targetRelativePath), {create: false},
        function(dirEntry) {
          if (onFileTransfersUpdated)
            chrome.fileBrowserPrivate.onFileTransfersUpdated
                .addListener(onFileTransfersUpdated);

          sourceEntry.copyTo(
              dirEntry, PathUtil.basename(targetRelativePath),
              function(entry) {
                self.cancelCallback_ = null;
                if (onFileTransfersUpdated)
                  chrome.fileBrowserPrivate.onFileTransfersUpdated
                      .removeListener(onFileTransfersUpdated);

                entry.getMetadata(function(metadata) {
                  if (metadata.size > numTransferredBytes)
                    onCopyProgress(
                        sourceEntry, metadata.size - numTransferredBytes);
                  onCopyComplete(entry, 0);
                });
              },
              function(error) {
                self.cancelCallback_ = null;
                chrome.fileBrowserPrivate.onFileTransfersUpdated.removeListener(
                    onFileTransfersUpdated);
                onFilesystemError(error);
              });
        },
        onFilesystemError);
  };

  FileCopyManager.Task.deduplicatePath(
      targetDirEntry, originalPath, onDeduplicated, errorCallback);
};

/**
 * Copies the contents of sourceEntry into targetEntry.
 *
 * @param {FileEntry} sourceEntry The file entry that will be copied.
 * @param {FileEntry} targetEntry The file entry to which sourceEntry will be
 *     copied.
 * @param {function(FileEntry, number)} progressCallback Function that will be
 *     called when a part of the source entry is copied. It takes |targetEntry|
 *     and size of the last copied chunk as parameters.
 * @param {function(FileEntry, number)} successCallback Function that will be
 *     called the copy operation finishes. It takes |targetEntry| and size of
 *     the last (not previously reported) copied chunk as parameters.
 * @param {function(FileError)} errorCallback Function that will be called
 *     if an error is encountered. Takes error type and additional error data
 *     as parameters.
 * @private
 */
FileCopyManager.prototype.copyFileEntry_ = function(sourceEntry,
                                                    targetEntry,
                                                    progressCallback,
                                                    successCallback,
                                                    errorCallback) {
  if (this.maybeCancel_())
    return;

  var self = this;

  var onSourceFileFound = function(file) {
    var onWriterCreated = function(writer) {
      var reportedProgress = 0;
      writer.onerror = function(progress) {
        errorCallback(writer.error);
      };

      writer.onprogress = function(progress) {
        if (self.maybeCancel_()) {
          // If the copy was cancelled, we should abort the operation.
          writer.abort();
          return;
        }
        // |progress.loaded| will contain total amount of data copied by now.
        // |progressCallback| expects data amount delta from the last progress
        // update.
        progressCallback(targetEntry, progress.loaded - reportedProgress);
        reportedProgress = progress.loaded;
      };

      writer.onwrite = function() {
        sourceEntry.getMetadata(function(metadata) {
          chrome.fileBrowserPrivate.setLastModified(targetEntry.toURL(),
              '' + Math.round(metadata.modificationTime.getTime() / 1000));
          successCallback(targetEntry, file.size - reportedProgress);
        });
      };

      writer.write(file);
    };

    targetEntry.createWriter(onWriterCreated, errorCallback);
  };

  sourceEntry.file(onSourceFileFound, errorCallback);
};

/**
 * Moves all entries in the task.
 *
 * @param {FileCopyManager.Task} task A move task to be run.
 * @param {function(util.EntryChangedType, Entry)} entryChangedCallback Callback
 *     invoked when an entry is changed.
 * @param {function()} progressCallback Callback invoked periodically during
 *     the moving.
 * @param {function()} successCallback On success.
 * @param {function(FileCopyManager.Error)} errorCallback On error.
 * @private
 */
FileCopyManager.prototype.serviceMoveTask_ = function(
    task, entryChangedCallback, progressCallback, successCallback,
    errorCallback) {
  if (task.pendingDirectories.length + task.pendingFiles.length == 0) {
    successCallback();
    return;
  }

  this.processMoveEntry_(
      task, task.getNextEntry(), entryChangedCallback,
      (function onCompleted() {
        // We should not dispatch a PROGRESS event when there is no pending
        // items in the task.
        if (task.pendingDirectories.length + task.pendingFiles.length == 0) {
          successCallback();
          return;
        }

        // Move the next entry.
        progressCallback();
        this.processMoveEntry_(
            task, task.getNextEntry(), entryChangedCallback,
            onCompleted.bind(this), errorCallback);
      }).bind(this),
      errorCallback);
};

/**
 * Moves the next entry in a given task.
 *
 * Implementation note: This method can be simplified more. For example, in
 * Task.setEntries(), the flag to recurse is set to false for move task,
 * so that all the entries' originalSourcePath should be
 * dirname(sourceEntry.fullPath).
 * Thus, targetRelativePath should contain exact one component. Also we can
 * skip applyRenames, because the destination directory always should be
 * task.targetDirEntry.
 * The unnecessary complexity is due to historical reason.
 * TODO(hidehiko): Refactor this method.
 *
 * @param {FileManager.Task} task A move task.
 * @param {Entry} sourceEntry An entry to be moved.
 * @param {function(util.EntryChangedType, Entry)} entryChangedCallback Callback
 *     invoked when an entry is changed.
 * @param {function()} successCallback On success.
 * @param {function(FileCopyManager.Error)} errorCallback On error.
 * @private
 */
FileCopyManager.prototype.processMoveEntry_ = function(
    task, sourceEntry, entryChangedCallback, successCallback, errorCallback) {
  if (this.maybeCancel_())
    return;

  // |sourceEntry.originalSourcePath| is set in util.recurseAndResolveEntries.
  var sourcePath = sourceEntry.originalSourcePath;
  if (sourceEntry.fullPath.substr(0, sourcePath.length) != sourcePath) {
    // We found an entry in the list that is not relative to the base source
    // path, something is wrong.
    errorCallback(new FileCopyManager.Error(
        util.FileOperationErrorType.UNEXPECTED_SOURCE_FILE,
        sourceEntry.fullPath));
    return;
  }

  FileCopyManager.Task.deduplicatePath(
      task.targetDirEntry,
      task.applyRenames(sourceEntry.fullPath.substr(sourcePath.length + 1)),
      function(targetRelativePath) {
        var onFilesystemError = function(err) {
          errorCallback(new FileCopyManager.Error(
              util.FileOperationErrorType.FILESYSTEM_ERROR,
              err));
        };

        task.targetDirEntry.getDirectory(
            PathUtil.dirname(targetRelativePath), {create: false},
            function(dirEntry) {
              sourceEntry.moveTo(
                  dirEntry, PathUtil.basename(targetRelativePath),
                  function(targetEntry) {
                    entryChangedCallback(
                        util.EntryChangedType.CREATED, targetEntry);
                    entryChangedCallback(
                        util.EntryChangedType.DELETED, sourceEntry);
                    task.markEntryComplete(targetEntry, 0);
                    successCallback();
                  },
                  onFilesystemError);
            },
            onFilesystemError);
      },
      errorCallback);
};

/**
 * Service a zip file creation task.
 *
 * @param {FileCopyManager.Task} task A zip task to be run.
 * @param {function(util.EntryChangedType, Entry)} entryChangedCallback Callback
 *     invoked when an entry is changed.
 * @param {function()} progressCallback Callback invoked periodically during
 *     the moving.
 * @param {function()} successCallback On complete.
 * @param {function(FileCopyManager.Error)} errorCallback On error.
 * @private
 */
FileCopyManager.prototype.serviceZipTask_ = function(
    task, entryChangedCallback, progressCallback, successCallback,
    errorCallback) {
  var dirURL = task.zipBaseDirEntry.toURL();
  var selectionURLs = [];
  for (var i = 0; i < task.pendingDirectories.length; i++)
    selectionURLs.push(task.pendingDirectories[i].toURL());
  for (var i = 0; i < task.pendingFiles.length; i++)
    selectionURLs.push(task.pendingFiles[i].toURL());

  // TODO(hidehiko): we should localize the name.
  var destName = 'Archive';
  if (task.originalEntries.length == 1) {
    var entryPath = task.originalEntries[0].fullPath;
    var i = entryPath.lastIndexOf('/');
    var basename = (i < 0) ? entryPath : entryPath.substr(i + 1);
    i = basename.lastIndexOf('.');
    destName = ((i < 0) ? basename : basename.substr(0, i));
  }

  var onDeduplicated = function(destPath) {
    var onZipSelectionComplete = function(success) {
      var onFilesystemError = function(err) {
        errorCallback(new FileCopyManager.Error(
            util.FileOperationErrorType.FILESYSTEM_ERROR,
            err));
      };

      if (success) {
        task.targetDirEntry.getFile(
            destPath, {create: false},
            function(entry) {
              entryChangedCallback(util.EntryChangedType.CREATE, entry);
              successCallback();
            },
            onFilesystemError);
      } else {
        onFilesystemError(
            Object.create(FileError.prototype, {
              code: {
                get: function() { return FileError.INVALID_MODIFICATION_ERR; }
              }
            }));
      }
    };

    progressCallback();
    chrome.fileBrowserPrivate.zipSelection(dirURL, selectionURLs, destPath,
        onZipSelectionComplete);
  };

  FileCopyManager.Task.deduplicatePath(
      task.targetDirEntry, destName + '.zip', onDeduplicated, errorCallback);
};

/**
 * Timeout before files are really deleted (to allow undo).
 */
FileCopyManager.DELETE_TIMEOUT = 30 * 1000;

/**
 * Schedules the files deletion.
 *
 * @param {Array.<Entry>} entries The entries.
 */
FileCopyManager.prototype.deleteEntries = function(entries) {
  var task = { entries: entries };
  this.deleteTasks_.push(task);
  this.maybeScheduleCloseBackgroundPage_();
  if (this.deleteTasks_.length == 1)
    this.serviceAllDeleteTasks_();
};

/**
 * Service all pending delete tasks, as well as any that might appear during the
 * deletion.
 *
 * @private
 */
FileCopyManager.prototype.serviceAllDeleteTasks_ = function() {
  var self = this;

  var onTaskSuccess = function() {
    var task = self.deleteTasks_[0];
    self.deleteTasks_.shift();
    if (!self.deleteTasks_.length) {
      // All tasks have been serviced, clean up and exit.
      self.eventRouter_.sendDeleteEvent(
          'SUCCESS',
          task.entries.map(function(e) {
            return util.makeFilesystemUrl(e.fullPath);
          }));
      self.maybeScheduleCloseBackgroundPage_();
      return;
    }

    // We want to dispatch a PROGRESS event when there are more tasks to serve
    // right after one task finished in the queue. We treat all tasks as one
    // big task logically, so there is only one BEGIN/SUCCESS event pair for
    // these continuous tasks.
    self.eventRouter_.sendDeleteEvent(
        'PROGRESS',
        task.entries.map(function(e) {
          return util.makeFilesystemUrl(e.fullPath);
        }));
    self.serviceDeleteTask_(self.deleteTasks_[0], onTaskSuccess, onTaskFailure);
  };

  var onTaskFailure = function(task) {
    self.deleteTasks_ = [];
    self.eventRouter_.sendDeleteEvent(
        'ERROR',
        task.entries.map(function(e) {
          return util.makeFilesystemUrl(e.fullPath);
        }));
    self.maybeScheduleCloseBackgroundPage_();
  };

  // If the queue size is 1 after pushing our task, it was empty before,
  // so we need to kick off queue processing and dispatch BEGIN event.
  this.eventRouter_.sendDeleteEvent(
      'BEGIN',
      this.deleteTasks_[0].entries.map(function(e) {
        return util.makeFilesystemUrl(e.fullPath);
      }));
  this.serviceDeleteTask_(this.deleteTasks_[0], onTaskSuccess, onTaskFailure);
};

/**
 * Performs the deletion.
 *
 * @param {Object} task The delete task (see deleteEntries function).
 * @param {function()} successCallback Callback run on success.
 * @param {function(FileCopyManager.Error)} errorCallback Callback run on error.
 * @private
 */
FileCopyManager.prototype.serviceDeleteTask_ = function(
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
      errorCallback(new FileCopyManager.Error(
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
              util.EntryChangedType.DELETED, currentEntry);
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
FileCopyManager.prototype.zipSelection = function(dirEntry, selectionEntries) {
  var self = this;
  var zipTask = new FileCopyManager.Task(dirEntry, dirEntry);
  zipTask.zip = true;
  zipTask.setEntries(selectionEntries, function() {
    // TODO: per-entry zip progress update with accurate byte count.
    // For now just set completedBytes to same value as totalBytes so that the
    // progress bar is full.
    zipTask.completedBytes = zip.Task.totalBytes;
    self.copyTasks_.push(zipTask);
    if (self.copyTasks_.length == 1) {
      // Assume self.cancelRequested_ == false.
      // This moved us from 0 to 1 active tasks, let the servicing begin!
      self.serviceAllTasks_();
    } else {
      // Force to update the progress of butter bar when there are new tasks
      // coming while servicing current task.
      self.eventRouter_.sendProgressEvent('PROGRESS', self.getStatus());
    }
  });
};

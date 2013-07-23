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
  this.listeners_ = [];

  window.addEventListener('error', function(e) {
    this.log_('Unhandled error: ', e.message, e.filename + ':' + e.lineno);
  }.bind(this));
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
 * A record of a queued copy operation.
 *
 * Multiple copy operations may be queued at any given time.  Additional
 * Tasks may be added while the queue is being serviced.  Though a
 * cancel operation cancels everything in the queue.
 *
 * @param {DirectoryEntry} targetDirEntry Target directory.
 * @param {boolean} sourceOnDrive True if the source entries are on Drive.
 * @param {boolean} targetOnDrive True if the target entry is on Drive.
 * @param {DirectoryEntry=} opt_zipBaseDirEntry Base directory dealt as a root
 *     in ZIP archive.
 * @constructor
 */
FileCopyManager.Task = function(targetDirEntry,
                                sourceOnDrive,
                                targetOnDrive,
                                opt_zipBaseDirEntry) {
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
  this.sourceOnDrive = sourceOnDrive;
  this.targetOnDrive = targetOnDrive;

  // If directory already exists, we try to make a copy named 'dir (X)',
  // where X is a number. When we do this, all subsequent copies from
  // inside the subtree should be mapped to the new directory name.
  // For example, if 'dir' was copied as 'dir (1)', then 'dir\file.txt' should
  // become 'dir (1)\file.txt'.
  this.renamedDirectories_ = [];
};

/**
 * @param {Array.<Entry>} entries Entries.
 * @param {function} callback When entries resolved.
 */
FileCopyManager.Task.prototype.setEntries = function(entries, callback) {
  var self = this;
  this.originalEntries = entries;
  // When moving directories, FileEntry.moveTo() is used if both source
  // and target are on Drive. There is no need to recurse into directories.
  util.recurseAndResolveEntries(entries, !this.move, function(result) {
    self.pendingDirectories = result.dirEntries;
    self.pendingFiles = result.fileEntries;
    self.pendingBytes = result.fileBytes;

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
        this.pendingBytes -= size;
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
  if (entry.isFile && this.pendingFiles && this.pendingFiles[0].inProgress) {
    this.completedBytes += size;
    this.pendingBytes -= size;
  }
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
 *
 * @param {string} reason Error type.
 * @param {Object} data Additional data.
 * @constructor
 */
FileCopyManager.Error = function(reason, data) {
  this.reason = reason;
  this.code = FileCopyManager.Error[reason];
  this.data = data;
};

/** @const */
FileCopyManager.Error.CANCELLED = 0;
/** @const */
FileCopyManager.Error.UNEXPECTED_SOURCE_FILE = 1;
/** @const */
FileCopyManager.Error.TARGET_EXISTS = 2;
/** @const */
FileCopyManager.Error.FILESYSTEM_ERROR = 3;


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
    rv.pendingBytes += task.pendingBytes;

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
 * Send an event to all the FileManager windows.
 *
 * @param {string} eventName Event name.
 * @param {Object} eventArgs An object with arbitrary event parameters.
 * @private
 */
FileCopyManager.prototype.sendEvent_ = function(eventName, eventArgs) {
  if (this.cancelRequested_)
    return;  // Swallow events until cancellation complete.

  eventArgs.status = this.getStatus();
  for (var i = 0; i < this.listeners_.length; ++i) {
    this.listeners_[i](eventName, eventArgs);
  }
};

/**
 * Adds a listener for running task events.
 * @param {function(string, Object)} listener A listener to be added.
 */
FileCopyManager.prototype.addListener = function(listener) {
  this.listeners_.push(listener);
};

/**
 * Removes the listener for running task events. If the listener is not added
 * by addListener(), it is simply ignored.
 * @param {function(string, Object)} listener A listener to be removed.
 */
FileCopyManager.prototype.removeListener = function(listener) {
  var index = this.listeners_.indexOf(listener);
  if (index >= 0)
    this.listeners_.splice(index, 1);
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
 * Reports an error on all of the active Files.app's windows.
 * @private
 */
FileCopyManager.prototype.log_ = function() {
  var windows = getContentWindows();
  for (var i = 0; i < windows.length; i++) {
    windows[i].console.error.apply(windows[i].console, arguments);
  }
};

/**
 * Dispatch a simple copy-progress event with reason and optional err data.
 *
 * @param {string} reason Event type.
 * @param {FileCopyManager.Error=} opt_err Error.
 * @private
 */
FileCopyManager.prototype.sendProgressEvent_ = function(reason, opt_err) {
  var event = {};
  event.reason = reason;
  if (opt_err)
    event.error = opt_err;
  this.sendEvent_('copy-progress', event);
};

/**
 * Dispatch an event of file operation completion (allows to update the UI).
 *
 * @private
 * @param {string} reason Completed file operation: 'movied|copied|deleted'.
 * @param {Array.<Entry>} affectedEntries deleted ot created entries.
 */
FileCopyManager.prototype.sendOperationEvent_ = function(reason,
                                                         affectedEntries) {
  var event = {};
  event.reason = reason;
  event.affectedEntries = affectedEntries;
  this.sendEvent_('copy-operation-complete', event);
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
 * @param {function=} opt_callback On cancel.
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
  this.sendProgressEvent_('CANCELLED');
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
 * @param {boolean} isOnDrive If the source items are on Google Drive.
 * @param {string} targetPath Target path.
 * @param {boolean} targetOnDrive If target is on Drive.
 */
FileCopyManager.prototype.paste = function(files, directories, isCut, isOnDrive,
                                           targetPath, targetOnDrive) {
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
      self.queueCopy_(targetEntry,
                      entries,
                      isCut,
                      isOnDrive,
                      targetOnDrive);
    },

    onPathError: function(err) {
      var myError = new FileCopyManager.Error('FILESYSTEM_ERROR', err);
      self.sendProgressEvent_('ERROR', myError);
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
 * @param {boolean} sourceOnDrive Source directory on Drive.
 * @param {boolean} targetOnDrive Target directory on Drive.
 * @return {FileCopyManager.Task} Copy task.
 * @private
 */
FileCopyManager.prototype.queueCopy_ = function(targetDirEntry,
                                                entries,
                                                deleteAfterCopy,
                                                sourceOnDrive,
                                                targetOnDrive) {
  var self = this;
  // When copying files, null can be specified as source directory.
  var copyTask = new FileCopyManager.Task(
      targetDirEntry, sourceOnDrive, targetOnDrive);
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
      self.sendProgressEvent_('PROGRESS');
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

  var onTaskError = function(err) {
    if (self.maybeCancel_())
      return;
    self.sendProgressEvent_('ERROR', err);
    self.resetQueue_();
  };

  var onTaskSuccess = function(task) {
    if (self.maybeCancel_())
      return;
    if (!self.copyTasks_.length) {
      // All tasks have been serviced, clean up and exit.
      self.sendProgressEvent_('SUCCESS');
      self.resetQueue_();
      return;
    }

    // We want to dispatch a PROGRESS event when there are more tasks to serve
    // right after one task finished in the queue. We treat all tasks as one
    // big task logically, so there is only one BEGIN/SUCCESS event pair for
    // these continuous tasks.
    self.sendProgressEvent_('PROGRESS');

    self.serviceNextTask_(onTaskSuccess, onTaskError);
  };

  // If the queue size is 1 after pushing our task, it was empty before,
  // so we need to kick off queue processing and dispatch BEGIN event.

  this.sendProgressEvent_('BEGIN');
  this.serviceNextTask_(onTaskSuccess, onTaskError);
};

/**
 * Service all entries in the next copy task.
 *
 * @param {function} successCallback On success.
 * @param {function} errorCallback On error.
 * @private
 */
FileCopyManager.prototype.serviceNextTask_ = function(
    successCallback, errorCallback) {
  var self = this;
  var task = this.copyTasks_[0];

  var onFilesystemError = function(err) {
    errorCallback(new FileCopyManager.Error('FILESYSTEM_ERROR', err));
  };

  var onTaskComplete = function() {
    self.copyTasks_.shift();
    self.maybeScheduleCloseBackgroundPage_();
    successCallback(task);
  };

  var deleteOriginals = function() {
    var count = task.originalEntries.length;

    var onEntryDeleted = function(entry) {
      self.sendOperationEvent_('deleted', [entry]);
      count--;
      if (!count)
        onTaskComplete();
    };

    for (var i = 0; i < task.originalEntries.length; i++) {
      var entry = task.originalEntries[i];
      util.removeFileOrDirectory(
          entry, onEntryDeleted.bind(self, entry), onFilesystemError);
    }
  };

  var onEntryServiced = function(targetEntry, size) {
    // We should not dispatch a PROGRESS event when there is no pending items
    // in the task.
    if (task.pendingDirectories.length + task.pendingFiles.length == 0) {
      if (task.deleteAfterCopy) {
        deleteOriginals();
      } else {
        onTaskComplete();
      }
      return;
    }

    self.sendProgressEvent_('PROGRESS');

    // We yield a few ms between copies to give the browser a chance to service
    // events (like perhaps the user clicking to cancel the copy, for example).
    setTimeout(function() {
      self.serviceNextTaskEntry_(task, onEntryServiced, errorCallback);
    }, 10);
  };

  if (!task.zip)
    this.serviceNextTaskEntry_(task, onEntryServiced, errorCallback);
  else
    this.serviceZipTask_(task, onTaskComplete, errorCallback);
};

/**
 * Service the next entry in a given task.
 * TODO(olege): Refactor this method into a separate class.
 *
 * @param {FileManager.Task} task A task.
 * @param {function} successCallback On success.
 * @param {function} errorCallback On error.
 * @private
 */
FileCopyManager.prototype.serviceNextTaskEntry_ = function(
    task, successCallback, errorCallback) {
  if (this.maybeCancel_())
    return;

  var self = this;
  var sourceEntry = task.getNextEntry();

  if (!sourceEntry) {
    // All entries in this task have been copied.
    successCallback(null);
    return;
  }

  // |sourceEntry.originalSourcePath| is set in util.recurseAndResolveEntries.
  var sourcePath = sourceEntry.originalSourcePath;
  if (sourceEntry.fullPath.substr(0, sourcePath.length) != sourcePath) {
    // We found an entry in the list that is not relative to the base source
    // path, something is wrong.
    onError('UNEXPECTED_SOURCE_FILE', sourceEntry.fullPath);
    return;
  }

  var targetDirEntry = task.targetDirEntry;
  var originalPath = sourceEntry.fullPath.substr(sourcePath.length + 1);
  originalPath = task.applyRenames(originalPath);

  var onCopyCompleteBase = function(entry, size) {
    task.markEntryComplete(entry, size);
    successCallback(entry, size);
  };

  var onCopyComplete = function(entry, size) {
    self.sendOperationEvent_('copied', [entry]);
    onCopyCompleteBase(entry, size);
  };

  var onCopyProgress = function(entry, size) {
    task.updateFileCopyProgress(entry, size);
    self.sendProgressEvent_('PROGRESS');
  };

  var onError = function(reason, data) {
    self.log_('serviceNextTaskEntry error: ' + reason + ':', data);
    errorCallback(new FileCopyManager.Error(reason, data));
  };

  var onFilesystemCopyComplete = function(sourceEntry, targetEntry) {
    // TODO(benchan): We currently do not know the size of data being
    // copied by FileEntry.copyTo(), so task.completedBytes will not be
    // increased. We will address this issue once we need to use
    // task.completedBytes to track the progress.
    self.sendOperationEvent_('copied', [sourceEntry, targetEntry]);
    onCopyCompleteBase(targetEntry, 0);
  };

  var onFilesystemMoveComplete = function(sourceEntry, targetEntry) {
    self.sendOperationEvent_('moved', [sourceEntry, targetEntry]);
    onCopyCompleteBase(targetEntry, 0);
  };

  var onFilesystemError = function(err) {
    onError('FILESYSTEM_ERROR', err);
  };

  /**
   * Resolves the immediate parent directory entry and the file name of a
   * given path, where the path is specified by a directory (not necessarily
   * the immediate parent) and a path (not necessarily the file name) related
   * to that directory. For instance,
   *   Given:
   *     |dirEntry| = DirectoryEntry('/root/dir1')
   *     |relativePath| = 'dir2/file'
   *
   *   Return:
   *     |parentDirEntry| = DirectoryEntry('/root/dir1/dir2')
   *     |fileName| = 'file'
   *
   * @param {DirectoryEntry} dirEntry A directory entry.
   * @param {string} relativePath A path relative to |dirEntry|.
   * @param {function(Entry,string)} successCallback A callback for returning
   *     the |parentDirEntry| and |fileName| upon success.
   * @param {function(FileError)} errorCallback An error callback when there is
   *     an error getting |parentDirEntry|.
   */
  var resolveDirAndBaseName = function(dirEntry, relativePath,
                                 successCallback, errorCallback) {
    // |intermediatePath| contains the intermediate path components
    // that are appended to |dirEntry| to form |parentDirEntry|.
    var intermediatePath = '';
    var fileName = relativePath;

    // Extract the file name component from |relativePath|.
    var index = relativePath.lastIndexOf('/');
    if (index != -1) {
      intermediatePath = relativePath.substr(0, index);
      fileName = relativePath.substr(index + 1);
    }

    if (intermediatePath == '') {
      successCallback(dirEntry, fileName);
    } else {
      dirEntry.getDirectory(intermediatePath,
                            {create: false},
                            function(entry) {
                              successCallback(entry, fileName);
                            },
                            errorCallback);
    }
  };

  var onDeduplicated = function(targetRelativePath) {
    if (task.move) {
      resolveDirAndBaseName(
          targetDirEntry, targetRelativePath,
          function(dirEntry, fileName) {
            sourceEntry.moveTo(dirEntry, fileName,
                               onFilesystemMoveComplete.bind(self, sourceEntry),
                               onFilesystemError);
          },
          onFilesystemError);
      return;
    }

    // TODO(benchan): drive::FileSystem has not implemented directory copy,
    // and thus we only call FileEntry.copyTo() for files. Revisit this
    // code when drive::FileSystem supports directory copy.
    if (sourceEntry.isFile && (task.sourceOnDrive || task.targetOnDrive)) {
      var sourceFileUrl = sourceEntry.toURL();
      var targetFileUrl = targetDirEntry.toURL() + '/' +
                          encodeURIComponent(targetRelativePath);
      var sourceFilePath = util.extractFilePath(sourceFileUrl);
      var targetFilePath = util.extractFilePath(targetFileUrl);
      var transferedBytes = 0;

      var onStartTransfer = function() {
        chrome.fileBrowserPrivate.onFileTransfersUpdated.addListener(
            onFileTransfersUpdated);
      };

      var onFailTransfer = function(err) {
        chrome.fileBrowserPrivate.onFileTransfersUpdated.removeListener(
            onFileTransfersUpdated);

        self.log_('Error copying ' + sourceFileUrl + ' to ' + targetFileUrl);
        onFilesystemError(err);
      };

      var onSuccessTransfer = function(targetEntry) {
        chrome.fileBrowserPrivate.onFileTransfersUpdated.removeListener(
            onFileTransfersUpdated);

        targetEntry.getMetadata(function(metadata) {
          if (metadata.size > transferedBytes)
            onCopyProgress(sourceEntry, metadata.size - transferedBytes);
          onFilesystemCopyComplete(sourceEntry, targetEntry);
        });
      };

      var downTransfer = 0;
      var onFileTransfersUpdated = function(statusList) {
        for (var i = 0; i < statusList.length; i++) {
          var s = statusList[i];
          // Comparing urls is unreliable, since they may use different
          // url encoding schemes (eg. rfc2396 vs. rfc3986).
          var filePath = util.extractFilePath(s.fileUrl);
          if (filePath == sourceFilePath || filePath == targetFilePath) {
            var processed = s.processed;

            // It becomes tricky when both the sides are on Drive.
            // Currently, it is implemented by download followed by upload.
            // Note, however, download will not happen if the file is cached.
            if (task.sourceOnDrive && task.targetOnDrive) {
              if (filePath == sourceFilePath) {
                // Download transfer is detected. Let's halve the progress.
                downTransfer = processed = (s.processed >> 1);
              } else {
                // If download transfer has been detected, the upload transfer
                // is stacked on top of it after halving. Otherwise, just use
                // the upload transfer as-is.
                processed = downTransfer > 0 ?
                   downTransfer + (s.processed >> 1) : s.processed;
              }
            }

            if (processed > transferedBytes) {
              onCopyProgress(sourceEntry, processed - transferedBytes);
              transferedBytes = processed;
            }
          }
        }
      };

      if (task.sourceOnDrive && task.targetOnDrive) {
        resolveDirAndBaseName(
            targetDirEntry, targetRelativePath,
            function(dirEntry, fileName) {
              onStartTransfer();
              sourceEntry.copyTo(dirEntry, fileName, onSuccessTransfer,
                                                     onFailTransfer);
            },
            onFilesystemError);
        return;
      }

      var onFileTransferCompleted = function() {
        self.cancelCallback_ = null;
        if (chrome.runtime.lastError) {
          onFailTransfer({
            code: chrome.runtime.lastError.message,
            toDrive: task.targetOnDrive,
            sourceFileUrl: sourceFileUrl
          });
        } else {
          targetDirEntry.getFile(targetRelativePath, {}, onSuccessTransfer,
                                                         onFailTransfer);
        }
      };

      self.cancelCallback_ = function() {
        self.cancelCallback_ = null;
        chrome.fileBrowserPrivate.onFileTransfersUpdated.removeListener(
            onFileTransfersUpdated);
        if (task.sourceOnDrive) {
          chrome.fileBrowserPrivate.cancelFileTransfers([sourceFileUrl],
                                                        function() {});
        } else {
          chrome.fileBrowserPrivate.cancelFileTransfers([targetFileUrl],
                                                        function() {});
        }
      };

      // TODO(benchan): Until drive::FileSystem supports FileWriter, we use the
      // transferFile API to copy files into or out from a drive file system.
      onStartTransfer();
      chrome.fileBrowserPrivate.transferFile(
          sourceFileUrl, targetFileUrl, onFileTransferCompleted);
      return;
    }

    if (sourceEntry.isDirectory) {
      targetDirEntry.getDirectory(
          targetRelativePath,
          {create: true, exclusive: true},
          function(targetEntry) {
            if (targetRelativePath != originalPath) {
              task.registerRename(originalPath, targetRelativePath);
            }
            onCopyComplete(targetEntry);
          },
          util.flog('Error getting dir: ' + targetRelativePath,
                    onFilesystemError));
    } else {
      targetDirEntry.getFile(
          targetRelativePath,
          {create: true, exclusive: true},
          function(targetEntry) {
            self.copyEntry_(sourceEntry, targetEntry,
                            onCopyProgress, onCopyComplete, onError);
          },
          util.flog('Error getting file: ' + targetRelativePath,
                    onFilesystemError));
    }
  };

  util.deduplicatePath(targetDirEntry, originalPath,
                       onDeduplicated, onError);
};

/**
 * Service a zip file creation task.
 *
 * @param {FileManager.Task} task A task.
 * @param {function} completeCallback On complete.
 * @param {function} errorCallback On error.
 * @private
 */
FileCopyManager.prototype.serviceZipTask_ = function(task, completeCallback,
                                                     errorCallback) {
  var self = this;
  var dirURL = task.zipBaseDirEntry.toURL();
  var selectionURLs = [];
  for (var i = 0; i < task.pendingDirectories.length; i++)
    selectionURLs.push(task.pendingDirectories[i].toURL());
  for (var i = 0; i < task.pendingFiles.length; i++)
    selectionURLs.push(task.pendingFiles[i].toURL());

  var destName = 'Archive';
  if (task.originalEntries.length == 1) {
    var entryPath = task.originalEntries[0].fullPath;
    var i = entryPath.lastIndexOf('/');
    var basename = (i < 0) ? entryPath : entryPath.substr(i + 1);
    i = basename.lastIndexOf('.');
    destName = ((i < 0) ? basename : basename.substr(0, i));
  }

  var onError = function(reason, data) {
    self.log_('serviceZipTask error: ' + reason + ':', data);
    errorCallback(new FileCopyManager.Error(reason, data));
  };

  var onDeduplicated = function(destPath) {
    var onZipSelectionComplete = function(success) {
      if (success) {
        self.sendProgressEvent_('SUCCESS');
      } else {
        self.sendProgressEvent_('ERROR',
            new FileCopyManager.Error('FILESYSTEM_ERROR', ''));
      }
      completeCallback(task);
    };

    self.sendProgressEvent_('PROGRESS');
    chrome.fileBrowserPrivate.zipSelection(dirURL, selectionURLs, destPath,
        onZipSelectionComplete);
  };

  util.deduplicatePath(
      task.targetDirEntry, destName + '.zip', onDeduplicated, onError);
};

/**
 * Copy the contents of sourceEntry into targetEntry.
 *
 * @private
 * @param {Entry} sourceEntry entry that will be copied.
 * @param {Entry} targetEntry entry to which sourceEntry will be copied.
 * @param {function(Entry, number)} progressCallback function that will be
 *     called when a part of the source entry is copied. It takes |targetEntry|
 *     and size of the last copied chunk as parameters.
 * @param {function(Entry, number)} successCallback function that will be called
 *     the copy operation finishes. It takes |targetEntry| and size of the last
 *     (not previously reported) copied chunk as parameters.
 * @param {function(string, object)} errorCallback function that will be called
 *     if an error is encountered. Takes error type and additional error data
 *     as parameters.
 */
FileCopyManager.prototype.copyEntry_ = function(sourceEntry,
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
        errorCallback('FILESYSTEM_ERROR', writer.error);
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

      writer.onwriteend = function() {
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

  var onTaskSuccess = function(task) {
    self.deleteTasks_.shift();
    if (!self.deleteTasks_.length) {
      // All tasks have been serviced, clean up and exit.
      self.sendDeleteEvent_(task, 'SUCCESS');
      self.maybeScheduleCloseBackgroundPage_();
      return;
    }

    // We want to dispatch a PROGRESS event when there are more tasks to serve
    // right after one task finished in the queue. We treat all tasks as one
    // big task logically, so there is only one BEGIN/SUCCESS event pair for
    // these continuous tasks.
    self.sendDeleteEvent_(self.deleteTasks_[0], 'PROGRESS');
    self.serviceDeleteTask_(self.deleteTasks_[0], onTaskSuccess, onTaskFailure);
  };

  var onTaskFailure = function(task) {
    self.deleteTasks_ = [];
    self.sendDeleteEvent_(task, 'ERROR');
    self.maybeScheduleCloseBackgroundPage_();
  };

  // If the queue size is 1 after pushing our task, it was empty before,
  // so we need to kick off queue processing and dispatch BEGIN event.
  this.sendDeleteEvent_(this.deleteTasks_[0], 'BEGIN');
  this.serviceDeleteTask_(this.deleteTasks_[0], onTaskSuccess, onTaskFailure);
};

/**
 * Performs the deletion.
 *
 * @param {Object} task The delete task (see deleteEntries function).
 * @param {function(Object)} onComplete Completion callback with the task
 *     as an argument.
 * @param {function(Object)} onFailure Failure callback with the task as an
 *     argument.
 * @private
 */
FileCopyManager.prototype.serviceDeleteTask_ = function(
    task, onComplete, onFailure) {
  var downcount = task.entries.length;

  var onEntryComplete = function() {
    if (--downcount == 0)
      onComplete(task);
  }.bind(this);

  var onEntryFailure = function() {
    if (--downcount == 0)
      onFailure(task);
  }.bind(this);

  if (downcount == 0)
    onComplete(task);

  for (var i = 0; i < task.entries.length; i++) {
    var entry = task.entries[i];
    util.removeFileOrDirectory(
        entry,
        onEntryComplete,
        onEntryFailure);
  }
};

/**
 * Send a 'delete' event to listeners.
 *
 * @param {Object} task The delete task (see deleteEntries function).
 * @param {string} reason Event reason.
 * @private
 */
FileCopyManager.prototype.sendDeleteEvent_ = function(task, reason) {
  this.sendEvent_('delete', {
    reason: reason,
    urls: task.entries.map(function(e) {
      return util.makeFilesystemUrl(e.fullPath);
    })
  });
};

/**
 * Creates a zip file for the selection of files.
 *
 * @param {Entry} dirEntry the directory containing the selection.
 * @param {boolean} isOnDrive If directory is on Drive.
 * @param {Array.<Entry>} selectionEntries the selected entries.
 */
FileCopyManager.prototype.zipSelection = function(dirEntry, isOnDrive,
                                                  selectionEntries) {
  var self = this;
  var zipTask = new FileCopyManager.Task(
      dirEntry, isOnDrive, isOnDrive, dirEntry);
  zipTask.zip = true;
  zipTask.setEntries(selectionEntries, function() {
    // TODO: per-entry zip progress update with accurate byte count.
    // For now just set pendingBytes to zero so that the progress bar is full.
    zipTask.pendingBytes = 0;
    self.copyTasks_.push(zipTask);
    if (self.copyTasks_.length == 1) {
      // Assume self.cancelRequested_ == false.
      // This moved us from 0 to 1 active tasks, let the servicing begin!
      self.serviceAllTasks_();
    } else {
      // Force to update the progress of butter bar when there are new tasks
      // coming while servicing current task.
      self.sendProgressEvent_('PROGRESS');
    }
  });
};

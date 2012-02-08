// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function FileCopyManager() {
  this.copyTasks_ = [];
  this.cancelObservers_ = [];
  this.cancelRequested_ = false;
}

FileCopyManager.prototype = {
  __proto__: cr.EventTarget.prototype
};

/**
 * A record of a queued copy operation.
 *
 * Multiple copy operations may be queued at any given time.  Additional
 * Tasks may be added while the queue is being serviced.  Though a
 * cancel operation cancels everything in the queue.
 */
FileCopyManager.Task = function(sourceDirEntry, targetDirEntry) {
  this.sourceDirEntry = sourceDirEntry;
  this.targetDirEntry = targetDirEntry;
  this.originalEntries = null;

  this.pendingDirectories = [];
  this.pendingFiles = [];
  this.pendingBytes = 0;

  this.completedDirectories = [];
  this.completedFiles = [];
  this.completedBytes = 0;

  this.deleteAfterCopy = false;

  // If directory already exists, we try to make a copy named 'dir (X)',
  // where X is a number. When we do this, all subsequent copies from
  // inside the subtree should be mapped to the new directory name.
  // For example, if 'dir' was copied as 'dir (1)', then 'dir\file.txt' should
  // become 'dir (1)\file.txt'.
  this.renamedDirectories_ = [];
}

FileCopyManager.Task.prototype.setEntries = function(entries, callback) {
  var self = this;

  function onEntriesRecursed(result) {
    self.pendingDirectories = result.dirEntries;
    self.pendingFiles = result.fileEntries;
    self.pendingBytes = result.fileBytes;
    callback();
  }

  this.originalEntries = entries;
  util.recurseAndResolveEntries(entries, onEntriesRecursed);
}

FileCopyManager.Task.prototype.takeNextEntry = function() {
  if (this.pendingDirectories.length)
    return this.pendingDirectories.shift();

  if (this.pendingFiles.length)
    return this.pendingFiles.shift();

  return null;
};

FileCopyManager.Task.prototype.markEntryComplete = function(entry, size) {
  if (entry.isDirectory) {
    this.completedDirectories.push(entry);
  } else {
    this.completedFiles.push(entry);
    this.completedBytes += size;
  }
};

FileCopyManager.Task.prototype.registerRename = function(fromName, toName) {
  this.renamedDirectories_.push({from: fromName + '/', to: toName + '/'});
};

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
 * Error class used to report problems with a copy operation.
 */
FileCopyManager.Error = function(reason, data) {
  this.reason = reason;
  this.code = FileCopyManager.Error[reason];
  this.data = data;
}

FileCopyManager.Error.CANCELLED = 0;
FileCopyManager.Error.UNEXPECTED_SOURCE_FILE = 1;
FileCopyManager.Error.TARGET_EXISTS = 2;
FileCopyManager.Error.FILESYSTEM_ERROR = 3;

// FileCopyManager methods.

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
  };

  for (var i = 0; i < this.copyTasks_.length; i++) {
    var task = this.copyTasks_[i];
    rv.pendingFiles += task.pendingFiles.length;
    rv.pendingDirectories += task.pendingDirectories.length;
    rv.pendingBytes += task.pendingBytes;
    rv.pendingItems += rv.pendingFiles + rv.pendingDirectories;

    rv.completedFiles += task.completedFiles.length;
    rv.completedDirectories += task.completedDirectories.length;
    rv.completedBytes += task.completedBytes;
    rv.completedItems += rv.completedFiles + rv.completedDirectories;

  }

  rv.totalFiles = rv.pendingFiles + rv.completedFiles;
  rv.totalDirectories = rv.pendingDirectories + rv.completedDirectories;
  rv.totalItems = rv.pendingItems + rv.completedItems;
  rv.totalBytes = rv.pendingBytes + rv.completedBytes;

  return rv;
};

/**
 * Completely clear out the copy queue, either because we encountered an error
 * or completed successfully.
 */
FileCopyManager.prototype.resetQueue_ = function() {
  for (var i = 0; i < this.cancelObservers_.length; i++)
    this.cancelObservers_[i]();

  this.copyTasks_ = [];
  this.cancelObservers_ = [];
  this.cancelRequested_ = false;
};

/**
 * Request that the current copy queue be abandoned.
 */
FileCopyManager.prototype.requestCancel = function(opt_callback) {
  this.cancelRequested_ = true;
  if (opt_callback)
    this.cancelObservers_.push(opt_callback);
};

/**
 * Perform the bookeeping required to cancel.
 */
FileCopyManager.prototype.doCancel_ = function() {
  var event = new cr.Event('copy-progress');
  event.reason = 'CANCELLED';
  this.dispatchEvent(event);
  this.resetQueue_();
};

/**
 * Used internally to check if a cancel has been requested, and handle
 * it if so.
 */
FileCopyManager.prototype.maybeCancel_ = function() {
  if (!this.cancelRequested_)
    return false;

  this.doCancel_();
  return true;
}

/**
 * Initiate a file copy.
 */
FileCopyManager.prototype.queueCopy = function(sourceDirEntry,
                                               targetDirEntry,
                                               entries,
                                               deleteAfterCopy) {
  var self = this;
  var copyTask = new FileCopyManager.Task(sourceDirEntry, targetDirEntry);
  copyTask.deleteAfterCopy = deleteAfterCopy;
  copyTask.setEntries(entries, function() {
    self.copyTasks_.push(copyTask);
    if (self.copyTasks_.length == 1) {
      // This moved us from 0 to 1 active tasks, let the servicing begin!
      self.serviceAllTasks_();
    }
  });

  return copyTask;
};

/**
 * Service all pending tasks, as well as any that might appear during the
 * copy.
 */
FileCopyManager.prototype.serviceAllTasks_ = function() {
  var self = this;

  function onTaskError(err) {
    var event = new cr.Event('copy-progress');
    event.reason = 'ERROR';
    event.error = err;
    self.dispatchEvent(event);
    self.resetQueue_();
  }

  function onTaskSuccess(task) {
    if (task == null) {
      // All tasks have been serviced, clean up and exit.
      var event = new cr.Event('copy-progress');
      event.reason = 'SUCCESS';
      self.dispatchEvent(event);
      self.resetQueue_();
      return;
    }

    self.serviceNextTask_(onTaskSuccess, onTaskError);
  }

  // If the queue size is 1 after pushing our task, it was empty before,
  // so we need to kick off queue processing.
  this.serviceNextTask_(onTaskSuccess, onTaskError);
};

/**
 * Service all entries in the next copy task.
 */
FileCopyManager.prototype.serviceNextTask_ = function(
    successCallback, errorCallback) {
  if (this.maybeCancel_())
    return;

  if (!this.copyTasks_.length) {
    successCallback(null);
    return;
  }

  var self = this;
  var task = this.copyTasks_[0];

  function onFilesystemError(err) {
    errorCallback(new FileCopyManager.Error('FILESYSTEM_ERROR', err));
  }

  function onTaskComplete() {
    self.copyTasks_.shift();
    successCallback(task);
  }

  function deleteOriginals() {
    var count = task.originalEntries.length;

    function onEntryDeleted() {
      count--;
      if (!count)
        onTaskComplete();
    }

    for (var i = 0; i < task.originalEntries.length; i++) {
      var entry = task.originalEntries[i];
      if (entry.isDirectory) {
        entry.removeRecursively(onEntryDeleted, onFilesystemError);
      } else {
        entry.remove(onEntryDeleted, onFilesystemError);
      }
    }
  }

  function onEntryServiced(targetEntry, size) {
    if (!targetEntry) {
      // All done with the entries in this task.
      if (task.deleteAfterCopy) {
        deleteOriginals()
      } else {
        onTaskComplete();
      }
      return;
    }

    var event = new cr.Event('copy-progress');
    event.reason = 'PROGRESS';
    self.dispatchEvent(event);

    // We yield a few ms between copies to give the browser a chance to service
    // events (like perhaps the user clicking to cancel the copy, for example).
    setTimeout(function() {
      self.serviceNextTaskEntry_(task, onEntryServiced, errorCallback);
    }, 10);
  }

  this.serviceNextTaskEntry_(task, onEntryServiced, errorCallback);
}

/**
 * Service the next entry in a given task.
 */
FileCopyManager.prototype.serviceNextTaskEntry_ = function(
    task, successCallback, errorCallback) {
  if (this.maybeCancel_())
    return;

  var self = this;
  var sourceEntry = task.takeNextEntry();

  if (!sourceEntry) {
    // All entries in this task have been copied.
    successCallback(null);
    return;
  }

  var sourcePath = task.sourceDirEntry.fullPath;
  if (sourceEntry.fullPath.substr(0, sourcePath.length) != sourcePath) {
    // We found an entry in the list that is not relative to the base source
    // path, something is wrong.
    return onError('UNEXPECTED_SOURCE_FILE', sourceEntry.fullPath);
  }

  var targetDirEntry = task.targetDirEntry;
  var originalPath = sourceEntry.fullPath.substr(sourcePath.length + 1);
  originalPath = task.applyRenames(originalPath);

  var targetRelativePrefix = originalPath;
  var targetExt = '';
  var index = targetRelativePrefix.lastIndexOf('.');
  if (index != -1) {
    targetExt = targetRelativePrefix.substr(index);
    targetRelativePrefix = targetRelativePrefix.substr(0, index);
  }

  // If file already exists, we try to make a copy named 'file (1).ext'.
  // If file is already named 'file (X).ext', we go with 'file (X+1).ext'.
  // If new name is still occupied, we increase the number up to 10 times.
  var copyNumber = 0;
  var match = /^(.*?)(?: \((\d+)\))?$/.exec(targetRelativePrefix);
  if (match && match[2]) {
    copyNumber = parseInt(match[2], 10);
    targetRelativePrefix = match[1];
  }

  var targetRelativePath = '';
  var renameTries = 0;
  var firstExistingEntry = null;

  function onCopyComplete(entry, size) {
    task.markEntryComplete(entry, size);
    successCallback(entry, size);
  }

  function onError(reason, data) {
    console.log('serviceNextTaskEntry error: ' + reason + ':', data);
    errorCallback(new FileCopyManager.Error(reason, data));
  }

  function onFilesystemError(err) {
    onError('FILESYSTEM_ERROR', err);
  }

  function onTargetExists(existingEntry) {
    if (!firstExistingEntry)
      firstExistingEntry = existingEntry;
    renameTries++;
    if (renameTries < 10) {
      copyNumber++;
      tryNextCopy();
    } else {
      onError('TARGET_EXISTS', firstExistingEntry);
    }
  }

  function onTargetNotResolved(err) {
    // We expect to be unable to resolve the target file, since we're going
    // to create it during the copy.  However, if the resolve fails with
    // anything other than NOT_FOUND, that's trouble.
    if (err.code != FileError.NOT_FOUND_ERR)
      return onError('FILESYSTEM_ERROR', err);

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
                           onCopyComplete, onError);
          },
          util.flog('Error getting file: ' + targetRelativePath,
                    onFilesystemError));
    }
  }

  function tryNextCopy() {
    targetRelativePath = targetRelativePrefix;
    if (copyNumber > 0) {
      targetRelativePath += ' (' + copyNumber + ')';
    }
    targetRelativePath += targetExt;

    // Check to see if the target exists.  This kicks off the rest of the copy
    // if the target is not found, or raises an error if it does.
    util.resolvePath(targetDirEntry, targetRelativePath, onTargetExists,
                     onTargetNotResolved);
  }

  tryNextCopy();
};

/**
 * Copy the contents of sourceEntry into targetEntry.
 */
FileCopyManager.prototype.copyEntry_ = function(
    sourceEntry, targetEntry, successCallback, errorCallback) {
  if (this.maybeCancel_())
    return;

  function onSourceFileFound(file) {
    function onWriterCreated(writer) {
      writer.onerror = function(err) {
        errorCallback('FILESYSTEM_ERROR', err);
      };
      writer.onwriteend = function() {
        successCallback(targetEntry, file.size)
      };
      writer.write(file);
    }

    targetEntry.createWriter(onWriterCreated, errorCallback);
  }

  sourceEntry.file(onSourceFileFound, errorCallback);
};

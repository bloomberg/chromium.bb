// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function FileCopyManager(root) {
  this.copyTasks_ = [];
  this.cancelObservers_ = [];
  this.cancelRequested_ = false;
  this.root_ = root;
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
  this.move = false;
  this.sourceOnGData = false;
  this.targetOnGData = false;

  // If directory already exists, we try to make a copy named 'dir (X)',
  // where X is a number. When we do this, all subsequent copies from
  // inside the subtree should be mapped to the new directory name.
  // For example, if 'dir' was copied as 'dir (1)', then 'dir\file.txt' should
  // become 'dir (1)\file.txt'.
  this.renamedDirectories_ = [];
};

FileCopyManager.Task.prototype.setEntries = function(entries, callback) {
  var self = this;

  function onEntriesRecursed(result) {
    self.pendingDirectories = result.dirEntries;
    self.pendingFiles = result.fileEntries;
    self.pendingBytes = result.fileBytes;
    callback();
  }

  this.originalEntries = entries;
  // When moving directories, FileEntry.moveTo() is used if both source
  // and target are on GData. There is no need to recurse into directories.
  var recurse = !this.move;
  util.recurseAndResolveEntries(entries, recurse, onEntriesRecursed);
};

FileCopyManager.Task.prototype.getNextEntry = function() {
  // We should keep the file in pending list and remove it after complete.
  // Otherwise, if we try to get status in the middle of copying. The returned
  // status is wrong (miss count the pasting item in totalItems).
  if (this.pendingDirectories.length) {
    this.pendingDirectories[0].inProgress = true;
    return this.pendingDirectories[0];
  }

  if (this.pendingFiles.length) {
    this.pendingFiles[0].inProgress = true;
    return this.pendingFiles[0];
  }

  return null;
};

FileCopyManager.Task.prototype.markEntryComplete = function(entry, size) {
  // It is probably not safe to directly remove the first entry in pending list.
  // We need to check if the removed entry (srcEntry) corresponding to the added
  // entry (target entry).
  if (entry.isDirectory && this.pendingDirectories &&
      this.pendingDirectories[0].inProgress) {
    this.completedDirectories.push(entry);
    this.pendingDirectories.shift();
  } else if (this.pendingFiles && this.pendingFiles[0].inProgress) {
    this.completedFiles.push(entry);
    this.completedBytes += size;
    this.pendingFiles.shift();
  } else {
    throw new Error('Try to remove a source entry which is not correspond to' +
                    ' the finished target entry');
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
};

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
    completedBytes: 0
  };

  for (var i = 0; i < this.copyTasks_.length; i++) {
    var task = this.copyTasks_[i];
    rv.pendingFiles += task.pendingFiles.length;
    rv.pendingDirectories += task.pendingDirectories.length;
    rv.pendingBytes += task.pendingBytes;

    rv.completedFiles += task.completedFiles.length;
    rv.completedDirectories += task.completedDirectories.length;
    rv.completedBytes += task.completedBytes;
  }
  rv.pendingItems = rv.pendingFiles + rv.pendingDirectories;
  rv.completedItems = rv.completedFiles + rv.completedDirectories;

  rv.totalFiles = rv.pendingFiles + rv.completedFiles;
  rv.totalDirectories = rv.pendingDirectories + rv.completedDirectories;
  rv.totalItems = rv.pendingItems + rv.completedItems;
  rv.totalBytes = rv.pendingBytes + rv.completedBytes;

  return rv;
};

/**
 * Get the overall progress data of all queued copy tasks.
 * @return {Object} An object containing the following parameters:
 *    percentage - The percentage (0-1) of finished items.
 *    pendingItems - The number of pending/unfinished items.
 */
FileCopyManager.prototype.getProgress = function() {
  var status = this.getStatus();
  return {
    // TODO(bshe): Need to figure out a way to get completed bytes in real
    // time. We currently use completedItems and totalItems to estimate the
    // progress. There are completeBytes and totalBytes ready to use.
    // However, the completedBytes is not in real time. It only updates
    // itself after each item finished. So if there is a large item to
    // copy, the progress bar will stop moving until it finishes and jump
    // a large portion of the bar.
    // There is case that when user copy a large file, we want to show an
    // 100% animated progress bar. So we use completedItems + 1 here.
    percentage: (status.completedItems + 1) / status.totalItems,
    pendingItems: status.pendingItems
  };
};

/**
 * Dispatch a simple copy-progress event with reason and optional err data.
 */
FileCopyManager.prototype.sendProgressEvent_ = function(reason, opt_err) {
  var event = new cr.Event('copy-progress');
  event.reason = reason;
  if (opt_err)
    event.error = opt_err;
  this.dispatchEvent(event);
};

/**
 * Dispatch an event of file operation completion (allows to update the UI).
 * @param {string} reason Completed file operation: 'movied|copied|deleted'.
 * @param {Array.<Entry>} affectedEntries deleted ot created entries.
 */
FileCopyManager.prototype.sendOperationEvent_ = function(reason,
                                                         affectedEntries) {
  var event = new cr.Event('copy-operation-complete');
  event.reason = reason;
  event.affectedEntries = affectedEntries;
  this.dispatchEvent(event);
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
  this.sendProgressEvent_('CANCELLED');
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
};

/**
 * Convert string in clipboard to entries and kick off pasting.
 */
FileCopyManager.prototype.paste = function(clipboard, targetPath,
                                           targetOnGData) {
  var self = this;
  var results = {
    sourceDirEntry: null,
    entries: [],
    isCut: false,
    isOnGData: false
  };

  function onPathError(err) {
    self.sendProgressEvent_('ERROR',
                            new FileCopyManager.Error('FILESYSTEM_ERROR', err));
  }

  function onSourceEntryFound(dirEntry) {
    function onTargetEntryFound(targetEntry) {
      self.queueCopy(results.sourceDirEntry,
            targetEntry,
            results.entries,
            results.isCut,
            results.isOnGData,
            targetOnGData);
    }

    function onComplete() {
      self.root_.getDirectory(targetPath, {},
                              onTargetEntryFound, onPathError);
    }

    function onEntryFound(entry) {
      // When getDirectories/getFiles finish, they call addEntry with null.
      // We dont want to add null to our entries.
      if (entry != null) {
        results.entries.push(entry);
        added++;
        if (added == total && results.sourceDirEntry != null)
          onComplete();
      }
    }

    results.sourceDirEntry = dirEntry;
    var directories = [];
    var files = [];

    if (clipboard.directories) {
      directories = clipboard.directories.split('\n');
      directories = directories.filter(function(d) { return d != '' });
    }
    if (clipboard.files) {
      files = clipboard.files.split('\n');
      files = files.filter(function(f) { return f != '' });
    }

    var total = directories.length + files.length;
    var added = 0;

    results.isCut = (clipboard.isCut == 'true');
    results.isOnGData = (clipboard.isOnGData == 'true');

    util.getDirectories(self.root_, {create: false}, directories, onEntryFound,
                        onPathError);
    util.getFiles(self.root_, {create: false}, files, onEntryFound,
                  onPathError);
  }

  self.root_.getDirectory(clipboard.sourceDir,
                          {create: false},
                          onSourceEntryFound,
                          onPathError);
};

/**
 * Initiate a file copy.
 */
FileCopyManager.prototype.queueCopy = function(sourceDirEntry,
                                               targetDirEntry,
                                               entries,
                                               deleteAfterCopy,
                                               sourceOnGData,
                                               targetOnGData) {
  var self = this;
  var copyTask = new FileCopyManager.Task(sourceDirEntry, targetDirEntry);
  if (deleteAfterCopy) {
    if (DirectoryModel.getRootPath(sourceDirEntry.fullPath) ==
            DirectoryModel.getRootPath(targetDirEntry.fullPath)) {
      copyTask.move = true;
    } else {
      copyTask.deleteAfterCopy = true;
    }
  }
  copyTask.sourceOnGData = sourceOnGData;
  copyTask.targetOnGData = targetOnGData;
  copyTask.setEntries(entries, function() {
    self.copyTasks_.push(copyTask);
    if (self.copyTasks_.length == 1) {
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
 */
FileCopyManager.prototype.serviceAllTasks_ = function() {
  var self = this;

  function onTaskError(err) {
    self.sendProgressEvent_('ERROR', err);
    self.resetQueue_();
  }

  function onTaskSuccess(task) {
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
  }

  // If the queue size is 1 after pushing our task, it was empty before,
  // so we need to kick off queue processing and dispatch BEGIN event.

  this.sendProgressEvent_('BEGIN');
  this.serviceNextTask_(onTaskSuccess, onTaskError);
};

/**
 * Service all entries in the next copy task.
 */
FileCopyManager.prototype.serviceNextTask_ = function(
    successCallback, errorCallback) {
  if (this.maybeCancel_())
    return;

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

    function onEntryDeleted(entry) {
      self.sendOperationEvent_('deleted', [entry]);
      count--;
      if (!count)
        onTaskComplete();
    }

    for (var i = 0; i < task.originalEntries.length; i++) {
      var entry = task.originalEntries[i];
      util.removeFileOrDirectory(
          entry, onEntryDeleted.bind(self, entry), onFilesystemError);
    }
  }

  function onEntryServiced(targetEntry, size) {
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
  }

  this.serviceNextTaskEntry_(task, onEntryServiced, errorCallback);
};

/**
 * Service the next entry in a given task.
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

  function onCopyCompleteBase(entry, size) {
    task.markEntryComplete(entry, size);
    successCallback(entry, size);
  }

  function onCopyComplete(entry, size) {
    self.sendOperationEvent_('copied', [entry]);
    onCopyCompleteBase(entry, size);
  }

  function onError(reason, data) {
    console.log('serviceNextTaskEntry error: ' + reason + ':', data);
    errorCallback(new FileCopyManager.Error(reason, data));
  }

  function onFilesystemCopyComplete(sourceEntry, targetEntry) {
    // TODO(benchan): We currently do not know the size of data being
    // copied by FileEntry.copyTo(), so task.completedBytes will not be
    // increased. We will address this issue once we need to use
    // task.completedBytes to track the progress.
    self.sendOperationEvent_('copied', [sourceEntry, targetEntry]);
    onCopyCompleteBase(targetEntry, 0);
  }

  function onFilesystemMoveComplete(sourceEntry, targetEntry) {
    self.sendOperationEvent_('moved', [sourceEntry, targetEntry]);
    onCopyCompleteBase(targetEntry, 0);
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
   * @param {function(Entry,string)} A callback for returning the
   *        |parentDirEntry| and |fileName| upon success.
   * @param {function(FileError)} An error callback when there is an error
   *        getting |parentDirEntry|.
   */
  function resolveDirAndBaseName(dirEntry, relativePath,
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
  }

  function onTargetNotResolved(err) {
    // We expect to be unable to resolve the target file, since we're going
    // to create it during the copy.  However, if the resolve fails with
    // anything other than NOT_FOUND, that's trouble.
    if (err.code != FileError.NOT_FOUND_ERR)
      return onError('FILESYSTEM_ERROR', err);

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

    if (task.sourceOnGData && task.targetOnGData) {
      // TODO(benchan): GDataFileSystem has not implemented directory copy,
      // and thus we only call FileEntry.copyTo() for files. Revisit this
      // code when GDataFileSystem supports directory copy.
      if (!sourceEntry.isDirectory) {
        resolveDirAndBaseName(
            targetDirEntry, targetRelativePath,
            function(dirEntry, fileName) {
              sourceEntry.copyTo(dirEntry, fileName,
                  onFilesystemCopyComplete.bind(self, sourceEntry),
                  onFilesystemError);
            },
            onFilesystemError);
        return;
      }
    }

    // TODO(benchan): Until GDataFileSystem supports FileWriter, we use the
    // transferFile API to copy files into or out from a gdata file system.
    if (sourceEntry.isFile && (task.sourceOnGData || task.targetOnGData)) {
      var sourceFileUrl = sourceEntry.toURL();
      var targetFileUrl = targetDirEntry.toURL() + '/' + targetRelativePath;
      chrome.fileBrowserPrivate.transferFile(
        sourceFileUrl, targetFileUrl,
        function() {
          if (chrome.extension.lastError) {
            console.log(
                'Error copying ' + sourceFileUrl + ' to ' + targetFileUrl);
            onFilesystemError({
              code: chrome.extension.lastError.message,
              toGDrive: task.targetOnGData,
              sourceFileUrl: sourceFileUrl
            });
          } else {
            targetDirEntry.getFile(targetRelativePath, {},
                onFilesystemCopyComplete.bind(self, sourceEntry),
                onFilesystemError);
          }
        });
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
        successCallback(targetEntry, file.size);
      };
      writer.write(file);
    }

    targetEntry.createWriter(onWriterCreated, errorCallback);
  }

  sourceEntry.file(onSourceFileFound, errorCallback);
};

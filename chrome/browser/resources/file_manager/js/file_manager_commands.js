// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var CommandUtil = {};

/**
 * Extracts root on which command event was dispatched.
 *
 * @param {Event} event Command event for which to retrieve root to operate on.
 * @param {cr.ui.List} rootsList Root list to extract root node.
 * @return {DirectoryEntry} Found root.
 */
CommandUtil.getCommandRoot = function(event, rootsList) {
  var result = rootsList.dataModel.item(
                   rootsList.getIndexOfListItem(event.target)) ||
               rootsList.selectedItem;

  return result;
};

/**
 * @param {Event} event Command event for which to retrieve root type.
 * @param {cr.ui.List} rootsList Root list to extract root node.
 * @return {string} Found root.
 */
CommandUtil.getCommandRootType = function(event, rootsList) {
  var root = CommandUtil.getCommandRoot(event, rootsList);

  return root && PathUtil.getRootType(root.fullPath);
};

/**
 * Checks if command can be executed on drive.
 * @param {Event} event Command event to mark.
 * @param {FileManager} fileManager FileManager to use.
 */
CommandUtil.canExecuteEnabledOnDriveOnly = function(event, fileManager) {
  event.canExecute = fileManager.isOnDrive();
};

/**
 * Checks if command should be visible on drive.
 * @param {Event} event Command event to mark.
 * @param {FileManager} fileManager FileManager to use.
 */
CommandUtil.canExecuteVisibleOnDriveOnly = function(event, fileManager) {
  event.canExecute = fileManager.isOnDrive();
  event.command.setHidden(!fileManager.isOnDrive());
};

/**
 * Returns a single selected/passed entry or null.
 * @param {Event} event Command event.
 * @param {FileManager} fileManager FileManager to use.
 * @return {FileEntry} The entry or null.
 */
CommandUtil.getSingleEntry = function(event, fileManager) {
  if (event.target.entry) {
    return event.target.entry;
  }
  var selection = fileManager.getSelection();
  if (selection.totalCount == 1) {
    return selection.entries[0];
  }
  return null;
};

/**
 * Registers handler on specific command on specific node.
 * @param {Node} node Node to register command handler on.
 * @param {string} commandId Command id to respond to.
 * @param {{execute:function, canExecute:function}} handler Handler to use.
 * @param {...Object} var_args Additional arguments to pass to handler.
 */
CommandUtil.registerCommand = function(node, commandId, handler, var_args) {
  var args = Array.prototype.slice.call(arguments, 3);

  node.addEventListener('command', function(event) {
    if (event.command.id == commandId) {
      handler.execute.apply(handler, [event].concat(args));
      event.cancelBubble = true;
    }
  });

  node.addEventListener('canExecute', function(event) {
    if (event.command.id == commandId)
      handler.canExecute.apply(handler, [event].concat(args));
  });
};

/**
 * Sets Commands.defaultCommand for the commandId and prevents handling
 * the keydown events for this command. Not doing that breaks relationship
 * of original keyboard event and the command. WebKit would handle it
 * differently in some cases.
 * @param {Node} node to register command handler on.
 * @param {string} commandId Command id to respond to.
 */
CommandUtil.forceDefaultHandler = function(node, commandId) {
  var doc = node.ownerDocument;
  var command = doc.querySelector('command[id="' + commandId + '"]');
  node.addEventListener('keydown', function(e) {
    if (command.matchesEvent(e)) {
      // Prevent cr.ui.CommandManager of handling it and leave it
      // for the default handler.
      e.stopPropagation();
    }
  });
  CommandUtil.registerCommand(node, commandId, Commands.defaultCommand, doc);
};

var Commands = {};

/**
 * Forwards all command events to standard document handlers.
 */
Commands.defaultCommand = {
  execute: function(event, document) {
    document.execCommand(event.command.id);
  },
  canExecute: function(event, document) {
    event.canExecute = document.queryCommandEnabled(event.command.id);
  }
};

/**
 * Unmounts external drive.
 */
Commands.unmountCommand = {
  execute: function(event, rootsList, fileManager) {
    var root = CommandUtil.getCommandRoot(event, rootsList);
    if (!root) return;

    var doUnmount = function() {
      fileManager.unmountVolume(PathUtil.getRootPath(root.fullPath));
    };

    if (fileManager.butterBar_.forceDeleteAndHide()) {
      // TODO(dgozman): add completion callback to file copy manager.
      setTimeout(doUnmount, 1000);
    } else {
      doUnmount();
    }
  },
  canExecute: function(event, rootsList) {
    var rootType = CommandUtil.getCommandRootType(event, rootsList);

    event.canExecute = (rootType == RootType.ARCHIVE ||
                        rootType == RootType.REMOVABLE);
    event.command.label = rootType == RootType.ARCHIVE ?
        str('CLOSE_ARCHIVE_BUTTON_LABEL') :
        str('UNMOUNT_DEVICE_BUTTON_LABEL');
  }
};

/**
 * Formats external drive.
 */
Commands.formatCommand = {
  execute: function(event, rootsList, fileManager) {
    var root = CommandUtil.getCommandRoot(event, rootsList);

    if (root) {
      var url = util.makeFilesystemUrl(PathUtil.getRootPath(root.fullPath));
      fileManager.confirm.show(
          loadTimeData.getString('FORMATTING_WARNING'),
          chrome.fileBrowserPrivate.formatDevice.bind(null, url));
    }
  },
  canExecute: function(event, rootsList, fileManager, directoryModel) {
    var root = CommandUtil.getCommandRoot(event, rootsList);
    var removable = root &&
                    PathUtil.getRootType(root.fullPath) == RootType.REMOVABLE;
    var isReadOnly = root && directoryModel.isPathReadOnly(root.fullPath);
    event.canExecute = removable && !isReadOnly;
    event.command.setHidden(!removable);
  }
};

/**
 * Imports photos from external drive
 */
Commands.importCommand = {
  execute: function(event, rootsList) {
    var root = CommandUtil.getCommandRoot(event, rootsList);

    if (root) {
      chrome.windows.create({url: chrome.extension.getURL('photo_import.html') +
          '#' + PathUtil.getRootPath(root.fullPath), type: 'popup'});
    }
  },
  canExecute: function(event, rootsList) {
    event.canExecute =
        (CommandUtil.getCommandRootType(event, rootsList) != RootType.DRIVE);
  }
};

/**
 * Initiates new folder creation.
 */
Commands.newFolderCommand = {
  execute: function(event, fileManager) {
    fileManager.createNewFolder();
  },
  canExecute: function(event, fileManager, directoryModel) {
    event.canExecute = !fileManager.isOnReadonlyDirectory() &&
                       !directoryModel.isSearching() &&
                       !fileManager.isRenamingInProgress();
  }
};

/**
 * Deletes selected files.
 */
Commands.deleteFileCommand = {
  execute: function(event, fileManager) {
    fileManager.deleteSelection();
  },
  canExecute: function(event, fileManager) {
    var selection = fileManager.getSelection();
    event.canExecute = !fileManager.isOnReadonlyDirectory() &&
                       selection &&
                       selection.totalCount > 0;
  }
};

/**
 * Pastes files from clipboard.
 */
Commands.pasteFileCommand = {
  execute: Commands.defaultCommand.execute,
  canExecute: function(event, document, fileTransferController) {
    event.canExecute = (fileTransferController &&
        fileTransferController.queryPasteCommandEnabled());
  }
};

/**
 * Initiates file renaming.
 */
Commands.renameFileCommand = {
  execute: function(event, fileManager) {
    fileManager.initiateRename();
  },
  canExecute: function(event, fileManager) {
    var selection = fileManager.getSelection();
    event.canExecute =
        !fileManager.isRenamingInProgress() &&
        !fileManager.isOnReadonlyDirectory() &&
        selection &&
        selection.totalCount == 1;
  }
};

/**
 * Opens drive help.
 */
Commands.volumeHelpCommand = {
  execute: function() {
    if (fileManager.isOnDrive())
      window.open(FileManager.GOOGLE_DRIVE_HELP, 'help');
    else
      window.open(FileManager.FILES_APP_HELP, 'help');
  },
  canExecute: function(event, fileManager) {
    event.canExecute = true;
  }
};

/**
 * Opens drive buy-more-space url.
 */
Commands.driveBuySpaceCommand = {
  execute: function() {
    window.open(FileManager.GOOGLE_DRIVE_BUY_STORAGE, 'buy-more-space');
  },
  canExecute: CommandUtil.canExecuteVisibleOnDriveOnly
};

/**
 * Clears drive cache.
 */
Commands.driveClearCacheCommand = {
  execute: function() {
    chrome.fileBrowserPrivate.clearDriveCache();
  },
  canExecute: CommandUtil.canExecuteVisibleOnDriveOnly
};

/**
 * Reload the metadata of the file system from the server
 */
Commands.driveReloadCommand = {
  execute: function() {
    chrome.fileBrowserPrivate.reloadDrive();
  },
  canExecute: CommandUtil.canExecuteVisibleOnDriveOnly
};

/**
 * Opens drive.google.com.
 */
Commands.driveGoToDriveCommand = {
  execute: function() {
    window.open(FileManager.GOOGLE_DRIVE_ROOT, 'drive-root');
  },
  canExecute: CommandUtil.canExecuteVisibleOnDriveOnly
};

/**
 * Displays open with dialog for current selection.
 */
Commands.openWithCommand = {
  execute: function(event, fileManager) {
    var tasks = fileManager.getSelection().tasks;
    if (tasks) {
      tasks.showTaskPicker(fileManager.defaultTaskPicker,
          str('OPEN_WITH_BUTTON_LABEL'),
          null,
          function(task) {
            tasks.execute(task.taskId);
          });
    }
  },
  canExecute: function(event, fileManager) {
    var tasks = fileManager.getSelection().tasks;
    event.canExecute = tasks && tasks.size() > 1;
  }
};

/**
 * Focuses search input box.
 */
Commands.searchCommand = {
  execute: function(event, fileManager, elmnt) {
    elmnt.focus();
  },
  canExecute: function(event, fileManager) {
    event.canExecute = !fileManager.isRenamingInProgress();
  }
};

/**
 * Flips 'available offline' flag on the file.
 */
Commands.togglePinnedCommand = {
  execute: function(event, fileManager) {
    var pin = !event.command.checked;
    var entry = CommandUtil.getSingleEntry(event, fileManager);

    var showError = function(filesystem) {
      fileManager.alert.showHtml(str('DRIVE_OUT_OF_SPACE_HEADER'),
          strf('DRIVE_OUT_OF_SPACE_MESSAGE',
               unescape(entry.name),
               util.bytesToString(filesystem.size)));
    };

    var callback = function(props) {
      var fileProps = props[0];
      if (fileProps.errorCode && pin) {
        fileManager.metadataCache_.get(entry, 'filesystem', showError);
      }
      // We don't have update events yet, so clear the cached data.
      fileManager.metadataCache_.clear(entry, 'drive');
      fileManager.metadataCache_.get(entry, 'drive', function(drive) {
        fileManager.updateMetadataInUI_('drive', [entry.toURL()], [drive]);
      });
    };

    chrome.fileBrowserPrivate.pinDriveFile([entry.toURL()], pin, callback);
    event.command.checked = pin;
  },
  canExecute: function(event, fileManager) {
    var entry = CommandUtil.getSingleEntry(event, fileManager);
    var drive = entry && fileManager.metadataCache_.getCached(entry, 'drive');

    if (!fileManager.isOnDrive() || !entry || entry.isDirectory || !drive ||
        drive.hosted) {
      event.canExecute = false;
      event.command.setHidden(true);
    } else {
      event.canExecute = true;
      event.command.setHidden(false);
      event.command.checked = drive.pinned;
    }
  }
};

/**
 * Creates zip file for current selection.
 */
Commands.zipSelectionCommand = {
  execute: function(event, fileManager, directoryModel) {
    var dirEntry = directoryModel.getCurrentDirEntry();
    var selectionEntries = fileManager.getSelection().entries;
    fileManager.copyManager_.zipSelection(dirEntry, fileManager.isOnDrive(),
                                          selectionEntries);
  },
  canExecute: function(event, fileManager) {
    var selection = fileManager.getSelection();
    event.canExecute = !fileManager.isOnReadonlyDirectory() &&
        !fileManager.isOnDrive() &&
        selection && selection.totalCount > 0;
  }
};

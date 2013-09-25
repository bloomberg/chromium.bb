// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

var CommandUtil = {};

/**
 * Extracts entry on which command event was dispatched.
 *
 * @param {DirectoryTree|DirectoryItem|NavigationList|HTMLLIElement|cr.ui.List}
 *     element Directory to extract a path from.
 * @return {Entry} Entry of the found node.
 */
CommandUtil.getCommandEntry = function(element) {
  if (element instanceof NavigationList) {
    // element is a NavigationList.

    /** @type {NavigationModelItem} */
    var selectedItem = element.selectedItem;
    return selectedItem && selectedItem.getCachedEntry();
  } else if (element instanceof NavigationListItem) {
    // element is a subitem of NavigationList.
    /** @type {NavigationList} */
    var navigationList = element.parentElement;
    var index = navigationList.getIndexOfListItem(element);
    /** @type {NavigationModelItem} */
    var item = (index != -1) ? navigationList.dataModel.item(index) : null;
    return item && item.getCachedEntry();
  } else if (element instanceof DirectoryTree) {
    // element is a DirectoryTree.
    return element.selectedItem;
  } else if (element instanceof DirectoryItem) {
    // element is a sub item in DirectoryTree.

    // DirectoryItem.fullPath is set on initialization, but entry is lazily.
    // We may use fullPath just in case that the entry has not been set yet.
    return element.entry;
  } else if (cr.ui.List) {
    // element is a normal List (eg. the file list on the right panel).
    var entry = element.selectedItem;
    return entry;
  } else {
    console.warn('Unsupported element');
    return null;
  }
};

/**
 * @param {NavigationList} navigationList navigation list to extract root node.
 * @return {?RootType} Type of the found root.
 */
CommandUtil.getCommandRootType = function(navigationList) {
  var root = CommandUtil.getCommandEntry(navigationList);
  return root &&
         PathUtil.isRootPath(root.fullPath) &&
         PathUtil.getRootType(root.fullPath);
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
 * Checks if command should be visible on drive with pressing ctrl key.
 * @param {Event} event Command event to mark.
 * @param {FileManager} fileManager FileManager to use.
 */
CommandUtil.canExecuteVisibleOnDriveWithCtrlKeyOnly =
    function(event, fileManager) {
  event.canExecute = fileManager.isOnDrive() && fileManager.isCtrlKeyPressed();
  event.command.setHidden(!event.canExecute);
};

/**
 * Sets as the command as always enabled.
 * @param {Event} event Command event to mark.
 */
CommandUtil.canExecuteAlways = function(event) {
  event.canExecute = true;
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
 * @param {...*} var_args Additional arguments to pass to handler.
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

/**
 * Handle of the command events.
 * @param {HTMLDocument} doc Document of Files.app's UI.
 * @constructor
 */
var CommandHandler = function(doc) {
  // Set member variable.
  this.commands_ = {};

  // Decorate command tags in the document.
  var commands = doc.querySelectorAll('command');
  for (var i = 0; i < commands.length; i++) {
    cr.ui.Command.decorate(commands[i]);
  }

  // Register events.
  doc.addEventListener('command', this.onCommand_.bind(this));
  doc.addEventListener('canExecute', this.onCanExecute_.bind(this));
};

/**
 * Registers handler on specific command on specific node.
 * @param {string} commandId Command id to respond to.
 * @param {{execute:function, canExecute:function}} handler Handler to use.
 * @param {...*} var_args Additional arguments to pass to handler.
 */
CommandHandler.prototype.registerCommand = function(commandId,
                                                    handler,
                                                    var_args) {
  handler.args = Array.prototype.slice.call(arguments, 2);
  this.commands_[commandId] = handler;
};

/**
 * Handles command events.
 * @param {Event} event Command event.
 * @private
 */
CommandHandler.prototype.onCommand_ = function(event) {
  var handler = this.commands_[event.command.id];
  handler.execute.apply(handler, [event].concat(handler.args));
};

/**
 * Handles canExecute events.
 * @param {Event} event Can execute event.
 * @private
 */
CommandHandler.prototype.onCanExecute_ = function(event) {
  var handler = this.commands_[event.command.id];
  handler.canExecute.apply(handler, [event].concat(handler.args));
};

/**
 * A command.
 * @interface
 */
var Command = function() {};

/**
 * Handles the execute event.
 * @param {Event} event Command event.
 * @param {...*} var_args Additional arguments.
 */
Command.prototype.execute = function(event, var_args) {};

/**
 * Handles the can execute event.
 * @param {Event} event Can execute event.
 * @param {...*} var_args Additional arguments.
 */
Command.prototype.canExecute = function(event, var_args) {};

var Commands = {};

/**
 * Forwards all command events to standard document handlers.
 * @implements {Command}
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
 * @implements {Command}
 */
Commands.unmountCommand = {
  /**
   * @param {Event} event Command event.
   * @param {FileManager} fileManager The file manager instance.
   */
  execute: function(event, fileManager) {
    var root = CommandUtil.getCommandEntry(event.target);
    if (root)
      fileManager.unmountVolume(PathUtil.getRootPath(root.fullPath));
  },
  /**
   * @param {Event} event Command event.
   */
  canExecute: function(event) {
    var rootType = CommandUtil.getCommandRootType(event.target);

    event.canExecute = (rootType == RootType.ARCHIVE ||
                        rootType == RootType.REMOVABLE);
    event.command.setHidden(!event.canExecute);
    event.command.label = rootType == RootType.ARCHIVE ?
        str('CLOSE_ARCHIVE_BUTTON_LABEL') :
        str('UNMOUNT_DEVICE_BUTTON_LABEL');
  }
};

/**
 * Formats external drive.
 * @implements {Command}
 */
Commands.formatCommand = {
  /**
   * @param {Event} event Command event.
   * @param {FileManager} fileManager The file manager instance.
   */
  execute: function(event, fileManager) {
    var root = CommandUtil.getCommandEntry(event.target);

    if (root) {
      var url = util.makeFilesystemUrl(PathUtil.getRootPath(root.fullPath));
      fileManager.confirm.show(
          loadTimeData.getString('FORMATTING_WARNING'),
          chrome.fileBrowserPrivate.formatDevice.bind(null, url));
    }
  },
  /**
   * @param {Event} event Command event.
   * @param {FileManager} fileManager The file manager instance.
   * @param {DirectoryModel} directoryModel The directory model instance.
   */
  canExecute: function(event, fileManager, directoryModel) {
    var root = CommandUtil.getCommandEntry(event.target);
    var removable = root &&
                    PathUtil.getRootType(root.fullPath) == RootType.REMOVABLE;
    var isReadOnly = root && directoryModel.isPathReadOnly(root.fullPath);
    event.canExecute = removable && !isReadOnly;
    event.command.setHidden(!removable);
  }
};

/**
 * Imports photos from external drive.
 * @implements {Command}
 */
Commands.importCommand = {
  /**
   * @param {Event} event Command event.
   * @param {NavigationList} navigationList Target navigation list.
   */
  execute: function(event, navigationList) {
    var root = CommandUtil.getCommandEntry(navigationList);
    if (!root)
      return;

    // TODO(mtomasz): Implement launching Photo Importer.
  },
  /**
   * @param {Event} event Command event.
   * @param {NavigationList} navigationList Target navigation list.
   */
  canExecute: function(event, navigationList) {
    var rootType = CommandUtil.getCommandRootType(navigationList);
    event.canExecute = (rootType != RootType.DRIVE);
  }
};

/**
 * Initiates new folder creation.
 * @implements {Command}
 */
Commands.newFolderCommand = {
  execute: function(event, fileManager) {
    fileManager.createNewFolder();
  },
  canExecute: function(event, fileManager, directoryModel) {
    event.canExecute = !fileManager.isOnReadonlyDirectory() &&
                       !fileManager.isRenamingInProgress() &&
                       !directoryModel.isSearching() &&
                       !directoryModel.isScanning();
  }
};

/**
 * Initiates new window creation.
 * @implements {Command}
 */
Commands.newWindowCommand = {
  execute: function(event, fileManager, directoryModel) {
    chrome.runtime.getBackgroundPage(function(background) {
      var appState = {
        defaultPath: directoryModel.getCurrentDirPath()
      };
      background.launchFileManager(appState);
    });
  },
  canExecute: function(event, fileManager) {
    event.canExecute = (fileManager.dialogType == DialogType.FULL_PAGE);
  }
};

/**
 * Changed the default app handling inserted media.
 * @implements {Command}
 */
Commands.changeDefaultAppCommand = {
  execute: function(event, fileManager) {
    fileManager.showChangeDefaultAppPicker();
  },
  canExecute: CommandUtil.canExecuteAlways
};

/**
 * Deletes selected files.
 * @implements {Command}
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
 * @implements {Command}
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
 * @implements {Command}
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
 * @implements {Command}
 */
Commands.volumeHelpCommand = {
  execute: function() {
    if (fileManager.isOnDrive())
      util.visitURL(urlConstants.GOOGLE_DRIVE_HELP);
    else
      util.visitURL(urlConstants.FILES_APP_HELP);
  },
  canExecute: CommandUtil.canExecuteAlways
};

/**
 * Opens drive buy-more-space url.
 * @implements {Command}
 */
Commands.driveBuySpaceCommand = {
  execute: function() {
    util.visitURL(urlConstants.GOOGLE_DRIVE_BUY_STORAGE);
  },
  canExecute: CommandUtil.canExecuteVisibleOnDriveOnly
};

/**
 * Clears drive cache.
 * @implements {Command}
 */
Commands.driveClearCacheCommand = {
  execute: function() {
    chrome.fileBrowserPrivate.clearDriveCache();
  },
  canExecute: CommandUtil.canExecuteVisibleOnDriveWithCtrlKeyOnly
};

/**
 * Opens drive.google.com.
 * @implements {Command}
 */
Commands.driveGoToDriveCommand = {
  execute: function() {
    util.visitURL(urlConstants.GOOGLE_DRIVE_ROOT);
  },
  canExecute: CommandUtil.canExecuteVisibleOnDriveOnly
};

/**
 * Displays open with dialog for current selection.
 * @implements {Command}
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
 * @implements {Command}
 */
Commands.searchCommand = {
  execute: function(event, fileManager, element) {
    element.focus();
    element.select();
  },
  canExecute: function(event, fileManager) {
    event.canExecute = !fileManager.isRenamingInProgress();
  }
};

/**
 * Activates the n-th volume.
 * @implements {Command}
 */
Commands.volumeSwitchCommand = {
  execute: function(event, navigationList, index) {
    navigationList.selectByIndex(index - 1);
  },
  canExecute: function(event, navigationList, index) {
    event.canExecute = index > 0 && index <= navigationList.dataModel.length;
  }
};

/**
 * Flips 'available offline' flag on the file.
 * @implements {Command}
 */
Commands.togglePinnedCommand = {
  execute: function(event, fileManager) {
    var pin = !event.command.checked;
    event.command.checked = pin;
    var entries = Commands.togglePinnedCommand.getTargetEntries_();
    var currentEntry;
    var error = false;
    var steps = {
      // Pick an entry and pin it.
      start: function() {
        // Check if all the entries are pinned or not.
        if (entries.length == 0)
          return;
        currentEntry = entries.shift();
        chrome.fileBrowserPrivate.pinDriveFile(
            currentEntry.toURL(),
            pin,
            steps.entryPinned);
      },

      // Check the result of pinning
      entryPinned: function() {
        // Convert to boolean.
        error = !!chrome.runtime.lastError;
        if (error && pin) {
          fileManager.metadataCache_.get(
              currentEntry, 'filesystem', steps.showError);
        }
        fileManager.metadataCache_.clear(currentEntry, 'drive');
        fileManager.metadataCache_.get(
            currentEntry, 'drive', steps.updateUI.bind(this));
      },

      // Update the user interface accoding to the cache state.
      updateUI: function(drive) {
        fileManager.updateMetadataInUI_(
            'drive', [currentEntry.toURL()], [drive]);
        if (!error)
          steps.start();
      },

      // Show the error
      showError: function(filesystem) {
        fileManager.alert.showHtml(str('DRIVE_OUT_OF_SPACE_HEADER'),
                                   strf('DRIVE_OUT_OF_SPACE_MESSAGE',
                                        unescape(currentEntry.name),
                                        util.bytesToString(filesystem.size)));
      }
    };
    steps.start();
  },

  canExecute: function(event, fileManager) {
    var entries = Commands.togglePinnedCommand.getTargetEntries_();
    var checked = true;
    for (var i = 0; i < entries.length; i++) {
      checked = checked && entries[i].pinned;
    }
    if (entries.length > 0) {
      event.canExecute = true;
      event.command.setHidden(false);
      event.command.checked = checked;
    } else {
      event.canExecute = false;
      event.command.setHidden(true);
    }
  },

  /**
   * Obtains target entries from the selection.
   * If directories are included in the selection, it just returns an empty
   * array to avoid confusing because pinning directory is not supported
   * currently.
   *
   * @return {Array.<Entry>} Target entries.
   * @private
   */
  getTargetEntries_: function() {
    var hasDirectory = false;
    var results = fileManager.getSelection().entries.filter(function(entry) {
      hasDirectory = hasDirectory || entry.isDirectory;
      if (!entry || hasDirectory)
        return false;
      var metadata = fileManager.metadataCache_.getCached(entry, 'drive');
        if (!metadata || metadata.hosted)
          return false;
      entry.pinned = metadata.pinned;
      return true;
    });
    return hasDirectory ? [] : results;
  }
};

/**
 * Creates zip file for current selection.
 * @implements {Command}
 */
Commands.zipSelectionCommand = {
  execute: function(event, fileManager, directoryModel) {
    var dirEntry = directoryModel.getCurrentDirEntry();
    var selectionEntries = fileManager.getSelection().entries;
    fileManager.fileOperationManager_.zipSelection(dirEntry, selectionEntries);
  },
  canExecute: function(event, fileManager) {
    var selection = fileManager.getSelection();
    event.canExecute = !fileManager.isOnReadonlyDirectory() &&
        !fileManager.isOnDrive() &&
        selection && selection.totalCount > 0;
  }
};

/**
 * Shows the share dialog for the current selection (single only).
 * @implements {Command}
 */
Commands.shareCommand = {
  execute: function(event, fileManager) {
    fileManager.shareSelection();
  },
  canExecute: function(event, fileManager) {
    var selection = fileManager.getSelection();
    event.canExecute = fileManager.isOnDrive() &&
        !fileManager.isDriveOffline() &&
        selection && selection.totalCount == 1;
    event.command.setHidden(!fileManager.isOnDrive());
  }
};

/**
 * Creates a shortcut of the selected folder (single only).
 * @implements {Command}
 */
Commands.createFolderShortcutCommand = {
  /**
   * @param {Event} event Command event.
   * @param {FileManager} fileManager The file manager instance.
   */
  execute: function(event, fileManager) {
    var entry = CommandUtil.getCommandEntry(event.target);
    if (entry)
      fileManager.createFolderShortcut(entry.fullPath);
  },

  /**
   * @param {Event} event Command event.
   * @param {FileManager} fileManager The file manager instance.
   */
  canExecute: function(event, fileManager) {
    var target = event.target;
    if (!target instanceof NavigationListItem &&
        !target instanceof DirectoryItem) {
      event.command.setHidden(true);
      return;
    }

    var entry = CommandUtil.getCommandEntry(event.target);
    var folderShortcutExists = entry &&
                               fileManager.folderShortcutExists(entry.fullPath);

    var onlyOneFolderSelected = true;
    // Only on list, user can select multiple files. The command is enabled only
    // when a single file is selected.
    if (event.target instanceof cr.ui.List &&
        !(event.target instanceof NavigationList)) {
      var items = event.target.selectedItems;
      onlyOneFolderSelected = (items.length == 1 && items[0].isDirectory);
    }

    var eligible = entry &&
                   PathUtil.isEligibleForFolderShortcut(entry.fullPath);
    event.canExecute =
        eligible && onlyOneFolderSelected && !folderShortcutExists;
    event.command.setHidden(!eligible || !onlyOneFolderSelected);
  }
};

/**
 * Removes the folder shortcut.
 * @implements {Command}
 */
Commands.removeFolderShortcutCommand = {
  /**
   * @param {Event} event Command event.
   * @param {FileManager} fileManager The file manager instance.
   */
  execute: function(event, fileManager) {
    var entry = CommandUtil.getCommandEntry(event.target);
    if (entry && entry.fullPath)
      fileManager.removeFolderShortcut(entry.fullPath);
  },

  /**
   * @param {Event} event Command event.
   * @param {FileManager} fileManager The file manager instance.
   */
  canExecute: function(event, fileManager) {
    var target = event.target;
    if (!target instanceof NavigationListItem &&
        !target instanceof DirectoryItem) {
      event.command.setHidden(true);
      return;
    }

    var entry = CommandUtil.getCommandEntry(target);
    var path = entry && entry.fullPath;

    var eligible = path && PathUtil.isEligibleForFolderShortcut(path);
    var isShortcut = path && fileManager.folderShortcutExists(path);
    event.canExecute = isShortcut && eligible;
    event.command.setHidden(!event.canExecute);
  }
};

/**
 * Zoom in to the Files.app.
 * @implements {Command}
 */
Commands.zoomInCommand = {
  execute: function(event) {
    chrome.fileBrowserPrivate.zoom('in');
  },
  canExecute: CommandUtil.canExecuteAlways
};

/**
 * Zoom out from the Files.app.
 * @implements {Command}
 */
Commands.zoomOutCommand = {
  execute: function(event) {
    chrome.fileBrowserPrivate.zoom('out');
  },
  canExecute: CommandUtil.canExecuteAlways
};

/**
 * Reset the zoom factor.
 * @implements {Command}
 */
Commands.zoomResetCommand = {
  execute: function(event) {
    chrome.fileBrowserPrivate.zoom('reset');
  },
  canExecute: CommandUtil.canExecuteAlways
};

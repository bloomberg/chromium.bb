// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * pyautoAPI object provides a set of functions used by PyAuto tests
 * to drive the file manager.
 *
 * Refer to chrome/test/functional/chromeos_file_browser.py for examples
 * of how this API is used.
 *
 * TODO(olege): Fix style warnings.
 */
var pyautoAPI = {
  /**
   * Add the item with given name to the current selection.
   * @param {string} name Name of the item to add to selection
   */
  addItemToSelection: function(name) {
    var entryExists = false;
    var dm = fileManager.directoryModel_.getFileList();
    for (var i = 0; i < dm.length; i++) {
      if (dm.item(i).name == name) {
        fileManager.currentList_.selectionModel.setIndexSelected(i, true);
        fileManager.currentList_.scrollIndexIntoView(i);
        fileManager.focusCurrentList_();
        entryExists = true;
        break;
      }
    }
    this.sendValue_(entryExists);
  },

  /**
   * List all items in the current directory.
   * We assume names do not contain '|' charecter.
   */
  listDirectory: function() {
    var list = [];
    var dm = fileManager.directoryModel_.getFileList();
    for (var i = 0; i < dm.length; i++) {
      list.push(dm.item(i).name);
    }
    this.sendJSONValue_(list);
  },

  /**
   * Save the item using the given name.
   *
   * @param {string} name Name given to item to be saved.
   */
  saveItemAs: function(name) {
    if (fileManager.dialogType_ == FileManager.DialogType.SELECT_SAVEAS_FILE) {
      fileManager.filenameInput_.value = name;
      fileManager.onOk_();
    } else {
      throw new Error('Cannot save an item in this dialog type.');
    }
    this.sendDone_();
  },

  /**
   * Open selected item.
   */
  openItem: function() {
    switch (fileManager.dialogType_) {
      case FileManager.DialogType.SELECT_FOLDER:
      case FileManager.DialogType.SELECT_OPEN_FILE:
      case FileManager.DialogType.SELECT_OPEN_MULTI_FILE:
        fileManager.onOk_();
        break;
      default:
        throw new Error('Cannot open an item in this dialog type.');
    }
    this.sendDone_();
  },

  /**
   * Execute the default task for the selected item.
   */
  executeDefaultTask: function() {
    switch (fileManager.dialogType_) {
      case FileManager.DialogType.FULL_PAGE:
        if (fileManager.selection.tasks)
          fileManager.selection.tasks.executeDefault();
        else
          throw new Error('Cannot execute a task on an empty selection.');
        break;
      default:
        throw new Error('Cannot execute a task in this dialog type.');
    }
    this.sendDone_();
  },

  /**
   * Executes the clipboard command.
   * @param {string} command Command name.
   */
  executeClipboardCommand_: function(command) {
    // Input should not be focused, or the cut/cop/paste command
    // will be treated as textual editing.
    fileManager.filenameInput_.blur();
    fileManager.document_.execCommand(command);
  },

  /**
   * Copy selected items to clipboard.
   */
  copyItems: function() {
    this.executeClipboardCommand_('copy');
    this.sendDone_();
  },

  /**
   * Cut selected items to clipboard.
   */
  cutItems: function() {
    this.executeClipboardCommand_('cut');
    this.sendDone_();
  },

  /**
   * Paste items from clipboard.
   */
  pasteItems: function() {
    var dm = fileManager.directoryModel_;
    var onRescan = function() {
      dm.removeEventListener('rescan-completed', onRescan);
      this.sendDone_();
    }.bind(this);

    dm.addEventListener('rescan-completed', onRescan);
    this.executeClipboardCommand_('paste');
  },

  /**
   * Rename selected item.
   * @param {string} name New name of the item.
   */
  renameItem: function(name) {
    var entry = fileManager.selection.entries[0];
    fileManager.directoryModel_.renameEntry(entry, name, this.sendDone_,
        this.sendDone_);
  },

  /**
   * Delete selected entries.
   */
  deleteItems: function() {
    var dm = fileManager.directoryModel_;
    var onRescan = function() {
      dm.removeEventListener('rescan-completed', onRescan);
      this.sendDone_();
    }.bind(this);

    dm.addEventListener('rescan-completed', onRescan);
    fileManager.deleteSelection();
  },

  /**
   * Create directory.
   * @param {string} name Name of the directory.
   */
  createDirectory: function(name) {
    var dm = fileManager.directoryModel_;
    var onRescan = function() {
      dm.removeEventListener('rescan-completed', onRescan);
      this.sendDone_();
    }.bind(this);

    dm.addEventListener('rescan-completed', onRescan);
    fileManager.directoryModel_.createDirectory(name, function() {});
  },

  /**
   * Change to a directory.
   * A path starting with '/' * is absolute, otherwise it is relative to the
   * current directory.
   * @param {string} path Path to directory.
   */
  changeDirectory: function(path) {
    if (path.charAt(0) != '/')
      path = fileManager.getCurrentDirectory() + '/' + path;
    var dm = fileManager.directoryModel_;

    var onChanged = function() {
      dm.removeEventListener('directory-changed', onChanged);
      this.sendDone_();
    }.bind(this);

    dm.addEventListener('directory-changed', onChanged);
    dm.changeDirectory(path);
  },

  /**
   * Get the absolute path of current directory.
   */
  currentDirectory: function() {
    this.sendValue_(fileManager.getCurrentDirectory());
  },

  /**
   * Get remaining and total size of selected directory.
   */
  getSelectedDirectorySizeStats: function() {
    var directoryURL = fileManager.selection.entries[0].toURL();
    chrome.fileBrowserPrivate.getSizeStats(directoryURL, function(stats) {
      this.sendJSONValue_(stats);
    }.bind(this));
  },

  /**
   * Returns whether the file manager is initialized.
   * This function is polled by pyauto before calling any
   * of the functions above.
   */
  isInitialized: function() {
    var initialized = fileManager &&
        fileManager.workerInitialized_ &&
        fileManager.getCurrentDirectory();
    this.sendValue_(!!initialized);
  },

  /**
   * Callback function for returning primitiv types (int, string, boolean)
   */
  sendValue_: function(value) {
    window.domAutomationController.send(value);
  },

  /**
   * Callback function for returning a JSON encoded value.
   */
  sendJSONValue_: function(value) {
    window.domAutomationController.send(JSON.stringify(value));
  },

  /**
   * Callback function signalling completion of operation.
   */
  sendDone_: function() {
    window.domAutomationController.send('done');
  }
};

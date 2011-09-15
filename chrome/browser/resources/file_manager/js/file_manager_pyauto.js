// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * pyautoAPI object provides a set of functions used by PyAuto tests
 * to drive the file manager.
 *
 * Refer to chrome/test/functional/chromeos_file_browser.py for examples
 * of how this API is used.
 */
var pyautoAPI = {
  /**
   * Add the item with given name to the current selection.
   *
   * @param {string} name Name of the item to add to selection
   * @return {boolean} Whether item exists.
   */
  addItemToSelection: function(name) {
    var itemExists = fileManager.addItemToSelection(name);
    window.domAutomationController.send(itemExists);
  },

  /**
   * List all items in the current directory.
   * We assume names do not contain '|' charecter.
   *
   * @return {string} A delimeter seperated string of item names.
   */
  listDirectory: function() {
    var list = fileManager.listDirectory();
    window.domAutomationController.send(list.join('|'));
  },

  /**
   * Save the item using the given name.
   *
   * @param {string} name Name given to item to be saved.
   */
  saveItemAs: function(name) {
    fileManager.doSaveAs(name);
    window.domAutomationController.send('done');
  },

  /**
   * Open selected item.
   */
  openItem: function() {
    fileManager.doOpen();
    window.domAutomationController.send('done');
  },

  /**
   * Copy selected items to clipboard.
   */
  copyItems: function() {
    fileManager.copySelectionToClipboard();
    window.domAutomationController.send('done');
  },

  /**
   * Cut selected items to clipboard.
   */
  cutItems: function() {
    fileManager.cutSelectionToClipboard();
    window.domAutomationController.send('done');
  },

  /**
   * Paste items from clipboard.
   */
  pasteItems: function() {
    fileManager.pasteFromClipboard(this.sendDone_);
  },

  /**
   * Rename selected item.
   *
   * @param {string} name New name of the item.
   */
  renameItem: function(name) {
    entry = fileManager.selection.entries[0];
    fileManager.renameEntry(entry, name, this.sendDone_);
  },

  /**
   * Delete selected entries.
   */
  deleteItems: function() {
    entries = fileManager.selection.entries;
    fileManager.deleteEntries(entries, true, this.sendDone_);
  },

  /**
   * Create directory.
   *
   * @param {string} name Name of the directory.
   */
  createDirectory: function(name) {
    fileManager.createNewFolder(name, this.sendDone_);
  },

  /**
   * Change to a directory.
   *
   * A path starting with '/' * is absolute, otherwise it is relative to the
   * current directory.
   *
   * @param {string} path Path to directory.
   */
  changeDirectory: function(path) {
    if (path.charAt(0) != '/')
      path = fileManager.getCurrentDirectory() + '/' + path
    fileManager.changeDirectory(path, undefined, undefined, this.sendDone_);
  },

  /**
   * Get the absolute path of current directory.
   *
   * @return {string} Path to the current directory.
   */
  currentDirectory: function() {
    path = fileManager.getCurrentDirectory()
    window.domAutomationController.send(path);
  },

  /**
   * Callback function signalling completion of operation.
   */
  sendDone_: function() {
      window.domAutomationController.send('done');
  },

  /**
   * Returns whether the file manager is initialized.
   *
   * This function is polled by pyauto before calling any
   * of the functions above.
   *
   * @return {boolean} Whether file manager is initialied.
   */
  isInitialized: function() {
    var initialized = (fileManager != null) && fileManager.isInitialized()
    window.domAutomationController.send(initialized);
  },
};

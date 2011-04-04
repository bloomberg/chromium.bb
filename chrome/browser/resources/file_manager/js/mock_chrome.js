// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Mock out the chrome.fileBrowserPrivate API for use in the harness.
 */
chrome.fileBrowserPrivate = {

  /**
   * Return a normal HTML5 filesystem api, rather than the real local
   * filesystem.
   *
   * You must start chrome with --allow-file-access-from-files and
   * --unlimited-quota-for-files in order for this to work.
   */
  requestLocalFileSystem: function(callback) {
    window.requestFileSystem(window.PERSISTENT, 16 * 1024 * 1024,
                             callback,
                             util.ferr('Error requesting filesystem'));
  },

  /**
   * Select multiple files.
   */
  selectFiles: function(selectedFiles) {
    console.log('selectFiles called: ' + selectedFiles.length +
                ' files selected');
    console.log(selectedFiles);
  },

  /**
   * Select a single file.
   */
  selectFile: function(selectedFile, index) {
    console.log('selectFile called: ' + selectedFile + ', ' + index);
    console.log(selectedFile);
  },

  /**
   * Cancel the dialog without selecting anything.
   */
  cancelDialog: function() {
    console.log('cancelDialog called');
  },

  /**
   * Return localized strings.
   */
  getStrings: function(callback) {
    // Keep this list in sync with the strings in generated_resources.grd and
    // extension_file_manager_api.cc!
    callback({
      LOCALE_FMT_DATE_SHORT: '%b %d, %Y',
      LOCALE_MONTHS_SHORT: 'Jan^Feb^Mar^Apr^May^Jun^Jul^Aug^Sep^Oct^Nov^Dec',
      LOCALE_DAYS_SHORT: 'Sun^Mon^Tue^Wed^Thu^Fri^Sat',

      SHORT_DATE_FORMAT: '%b %-d, %Y',
      FILES_DISPLAYED_SUMMARY: '%1 Files Displayed',
      FILES_SELECTED_SUMMARY: '%1 Files Selected',
      FILE_IS_DIRECTORY: 'Folder',
      PARENT_DIRECTORY: 'Parent Directory',

      ROOT_DIRECTORY_LABEL: 'Files',
      NAME_COLUMN_LABEL: 'Name',
      SIZE_COLUMN_LABEL: 'Size',
      DATE_COLUMN_LABEL: 'Date',
      PREVIEW_COLUMN_LABEL: 'Preview',

      CANCEL_LABEL: 'Cancel',
      OPEN_LABEL: 'Open',
      SAVE_LABEL: 'Save',

      SELECT_FOLDER_TITLE: 'Select a folder to open',
      SELECT_OPEN_FILE_TITLE: 'Select a file to open',
      SELECT_OPEN_MULTI_FILE_TITLE: 'Select one or more files',
      SELECT_SAVEAS_FILE: 'Select a file to save as',

      COMPUTING_SELECTION: 'Computing selection...',
      NOTHING_SELECTED: 'No files selected',
      ONE_FILE_SELECTED: 'One file selected, $1',
      MANY_FILES_SELECTED: '$1 files selected, $2',
    });
  }
};

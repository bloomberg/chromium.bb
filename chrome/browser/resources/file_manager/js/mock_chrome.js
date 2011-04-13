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
  },

  /**
   * Select a single file.
   */
  selectFile: function(selectedFile, index) {
    console.log('selectFile called: ' + selectedFile + ', ' + index);
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
      LOCALE_FMT_DATE_SHORT: '%b %-d, %Y',
      LOCALE_MONTHS_SHORT: 'Jan^Feb^Mar^Apr^May^Jun^Jul^Aug^Sep^Oct^Nov^Dec',
      LOCALE_DAYS_SHORT: 'Sun^Mon^Tue^Wed^Thu^Fri^Sat',

      BODY_FONT_FAMILY: 'sans-serif',
      BODY_FONT_SIZE: '13px',

      FILE_IS_DIRECTORY: 'Folder',
      PARENT_DIRECTORY: 'Parent Directory',

      ROOT_DIRECTORY_LABEL: 'Files',
      DOWNLOADS_DIRECTORY_LABEL: 'File Shelf',
      MEDIA_DIRECTORY_LABEL: 'External Storage',
      NAME_COLUMN_LABEL: 'Name',
      SIZE_COLUMN_LABEL: 'Size',
      DATE_COLUMN_LABEL: 'Date',
      PREVIEW_COLUMN_LABEL: 'Preview',

      ERROR_CREATING_FOLDER: 'Unable to create folder "$1": $2',
      ERROR_INVALID_FOLDER_CHARACTER: 'Invalid character in folder name: $1',
      NEW_FOLDER_PROMPT: 'Enter a name for the new folder',
      NEW_FOLDER_BUTTON_LABEL: 'New Folder',
      FILENAME_LABEL: 'File Name',

      CANCEL_LABEL: 'Cancel',
      OPEN_LABEL: 'Open',
      SAVE_LABEL: 'Save',

      SELECT_FOLDER_TITLE: 'Select a folder to open',
      SELECT_OPEN_FILE_TITLE: 'Select a file to open',
      SELECT_OPEN_MULTI_FILE_TITLE: 'Select one or more files',
      SELECT_SAVEAS_FILE_TITLE: 'Select a file to save as',

      COMPUTING_SELECTION: 'Computing selection...',
      NOTHING_SELECTED: 'No files selected',
      ONE_FILE_SELECTED: 'One file selected, $1',
      MANY_FILES_SELECTED: '$1 files selected, $2',
    });
  }
};

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
    window.webkitRequestFileSystem(window.PERSISTENT, 16 * 1024 * 1024,
                                   callback,
                                   util.ferr('Error requesting filesystem'));
  },

  /**
   * View multiple files.
   */
  viewFiles: function(selectedFiles) {
    console.log('viewFiles called: ' + selectedFiles.length +
                ' files selected');
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
   * Disk mount/unmount notification.
   */
  onDiskChanged: {
    callbacks: [],
    addListener: function(cb) { this.callbacks.push(cb) }
  },

  getFileTasks: function(urlList, callback) {
    if (urlList.length == 0)
      return callback([]);

    if (!callback)
      throw new Error('Missing callback');

    var tasks =
    [ { taskId: 'upload-picasr',
        title: 'Upload to Picasr',
        regexp: /\.(jpe?g|gif|png|cr2?|tiff)$/i,
        iconUrl: 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAOCAYAAAAmL5yKAAAAAXNSR0IArs4c6QAAAAZiS0dEAP8A/wD/oL2nkwAAAAlwSFlzAAALEwAACxMBAJqcGAAAAAd0SU1FB9sEEBcJA0AW6BUAAACdSURBVCjPzZExC4MwEIW/1L2U/gwHf1/3WrqIkz/PWVAoXdolRNLBJNhwiS6FPjguuZf3csnBL2HBLikNtSFmS3yIROUMWhKrHR2XNZiLa9tGkaqtDa4TjBX0yIf8+osLnT3BnKDIvddm/uCRE+fgDc7r4iBPJWAWDADQLh8Tt3neSAYKdAu8gc69L4rAN8v+Fk/3DrxcluD5mr/CB34jRiE3x1kcAAAAAElFTkSuQmCC',
      },
      { taskId: 'upload-orcbook',
        title: 'Upload to OrcBook',
        regexp: /\.(jpe?g|png|cr2?|tiff)$/i,
        iconUrl: 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAOCAYAAAAmL5yKAAAAAXNSR0IArs4c6QAAAAZiS0dEAP8A/wD/oL2nkwAAAAlwSFlzAAALEwAACxMBAJqcGAAAAAd0SU1FB9sEEBcOAw9XftIAAADFSURBVCjPrZKxCsIwEIa/FHFwsvYxROjSQXAoqLiIL+xgBtvZ91A6uOnQc2hT0zRqkR4c3P25+/PfJTCwLU6wEpgBWkDXuInDPSwF5r7mJIeNQFTnIiCeONpVdYlLoK9wEUhNg8+B9FDVaZcgCKAovjTXfvPJFwGZtKW60pt8bOGBzfLouemnFY/MAs8wDeEI4NzaybewBu4AysKVgrK0gfe5iB9vjdAUqQ/S1Y/R3IX9Zc1zxc7zxe2/0Iskt7AsG0hhx14W8XV43FgV4gAAAABJRU5ErkJggg==',
      },
    ];

    // Copy all tasks, then remove the ones that don't match.
    var candidateTasks = [].concat(tasks);

    for (var i = 0; i < urlList.length; i++) {
      if (candidateTasks.length == 0)
        return callback([]);

      for (var taskIndex = candidateTasks.length - 1; taskIndex >= 0;
           taskIndex--) {
        if (candidateTasks[taskIndex].regexp.test(urlList[i]))
          continue;

        // This task doesn't match this url, remove the task.
        candidateTasks.splice(taskIndex, 1);
      }
    }

    callback(candidateTasks);
  },

  executeTask: function(taskId, urlList) {
    console.log('executing task: ' + taskId + ': ' + urlList.length + ' urls');
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
      ERROR_INVALID_FILE_CHARACTER: 'Invalid character in file name: $1',
      NEW_FOLDER_PROMPT: 'Enter a name for the new folder',
      NEW_FOLDER_BUTTON_LABEL: 'New Folder',
      FILENAME_LABEL: 'File Name',

      EJECT_BUTTON: 'Eject',
      IMAGE_DIMENSIONS: 'Image Dimensions',
      VOLUME_LABEL: 'Volume Label',
      READ_ONLY: 'Read Only',

      ERROR_RENAMING: 'Unable to rename "$1": $2',
      RENAME_PROMPT: 'Enter a new name',
      RENAME_BUTTON_LABEL: 'Rename',

      ERROR_DELETING: 'Unable to delete "$1": $2',
      DELETE_BUTTON_LABEL: 'Delete',

      ERROR_MOVING: 'Unable to move "$1": $2',
      MOVE_BUTTON_LABEL: 'Move',

      ERROR_PASTING: 'Unable to paste "$1": $2',
      PASTE_BUTTON_LABEL: 'Paste',

      COPY_BUTTON_LABEL: 'Copy',
      CUT_BUTTON_LABEL: 'Cut',

      DEVICE_TYPE_FLASH: 'Flash Device',
      DEVICE_TYPE_HDD: 'Hard Disk Device',
      DEVICE_TYPE_OPTICAL: 'Optical Device',
      DEVICE_TYPE_UNDEFINED: 'Unknown Device',

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

      CONFIRM_DELETE: 'Are you sure?',
    });
  }
};

chrome.extension = {
  getURL: function() {
    return document.location.href;
  }
};

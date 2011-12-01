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
  onMountCompleted: {
    callbacks: [],
    addListener: function(cb) { this.callbacks.push(cb) }
  },

  /**
   * File system change notification.
   */
  onFileChanged: {
    callbacks: [],
    addListener: function(cb) { this.callbacks.push(cb) }
  },

  /**
   * File watchers.
   */
  addFileWatch: function(path, callback) { callback(true) },

  removeFileWatch: function(path, callback) { callback(true) },

  /**
   * Returns common tasks for a given list of files.
   */
  getFileTasks: function(urlList, callback) {
    if (urlList.length == 0)
      return callback([]);

    // This is how File Manager gets the extension id.
    var extensionId = chrome.extension.getURL('').split('/')[2];

    if (!callback)
      throw new Error('Missing callback');

    var tasks =
    [ { taskId: extensionId + '|upload-picasr',
        title: 'Upload to Picasr',
        regexp: /\.(jpe?g|gif|png|cr2?|tiff)$/i,
        iconUrl: 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAOCAYAAAAmL5yKAAAAAXNSR0IArs4c6QAAAAZiS0dEAP8A/wD/oL2nkwAAAAlwSFlzAAALEwAACxMBAJqcGAAAAAd0SU1FB9sEEBcJA0AW6BUAAACdSURBVCjPzZExC4MwEIW/1L2U/gwHf1/3WrqIkz/PWVAoXdolRNLBJNhwiS6FPjguuZf3csnBL2HBLikNtSFmS3yIROUMWhKrHR2XNZiLa9tGkaqtDa4TjBX0yIf8+osLnT3BnKDIvddm/uCRE+fgDc7r4iBPJWAWDADQLh8Tt3neSAYKdAu8gc69L4rAN8v+Fk/3DrxcluD5mr/CB34jRiE3x1kcAAAAAElFTkSuQmCC'
      },
      { taskId: extensionId + '|upload-orcbook',
        title: 'Upload to OrcBook',
        regexp: /\.(jpe?g|png|cr2?|tiff)$/i,
        iconUrl: 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAOCAYAAAAmL5yKAAAAAXNSR0IArs4c6QAAAAZiS0dEAP8A/wD/oL2nkwAAAAlwSFlzAAALEwAACxMBAJqcGAAAAAd0SU1FB9sEEBcOAw9XftIAAADFSURBVCjPrZKxCsIwEIa/FHFwsvYxROjSQXAoqLiIL+xgBtvZ91A6uOnQc2hT0zRqkR4c3P25+/PfJTCwLU6wEpgBWkDXuInDPSwF5r7mJIeNQFTnIiCeONpVdYlLoK9wEUhNg8+B9FDVaZcgCKAovjTXfvPJFwGZtKW60pt8bOGBzfLouemnFY/MAs8wDeEI4NzaybewBu4AysKVgrK0gfe5iB9vjdAUqQ/S1Y/R3IX9Zc1zxc7zxe2/0Iskt7AsG0hhx14W8XV43FgV4gAAAABJRU5ErkJggg=='
      },
      { taskId: extensionId + '|mount-archive',
        title: 'Mount',
        regexp: /\.(zip)$/i,
        iconUrl: 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAOCAYAAAAmL5yKAAAAAXNSR0IArs4c6QAAAAZiS0dEAP8A/wD/oL2nkwAAAAlwSFlzAAALEwAACxMBAJqcGAAAAAd0SU1FB9sEEBcOAw9XftIAAADFSURBVCjPrZKxCsIwEIa/FHFwsvYxROjSQXAoqLiIL+xgBtvZ91A6uOnQc2hT0zRqkR4c3P25+/PfJTCwLU6wEpgBWkDXuInDPSwF5r7mJIeNQFTnIiCeONpVdYlLoK9wEUhNg8+B9FDVaZcgCKAovjTXfvPJFwGZtKW60pt8bOGBzfLouemnFY/MAs8wDeEI4NzaybewBu4AysKVgrK0gfe5iB9vjdAUqQ/S1Y/R3IX9Zc1zxc7zxe2/0Iskt7AsG0hhx14W8XV43FgV4gAAAABJRU5ErkJggg=='
      },
      {
        taskId: extensionId + '|gallery',
        title: 'View and Edit',
        regexp: /\.(jpe?g|gif|png|cr2?|tiff|webp|bmp)$/i,
        iconUrl: 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAOCAYAAAAmL5yKAAAAAXNSR0IArs4c6QAAAAZiS0dEAP8A/wD/oL2nkwAAAAlwSFlzAAALEwAACxMBAJqcGAAAAAd0SU1FB9sEEBcOAw9XftIAAADFSURBVCjPrZKxCsIwEIa/FHFwsvYxROjSQXAoqLiIL+xgBtvZ91A6uOnQc2hT0zRqkR4c3P25+/PfJTCwLU6wEpgBWkDXuInDPSwF5r7mJIeNQFTnIiCeONpVdYlLoK9wEUhNg8+B9FDVaZcgCKAovjTXfvPJFwGZtKW60pt8bOGBzfLouemnFY/MAs8wDeEI4NzaybewBu4AysKVgrK0gfe5iB9vjdAUqQ/S1Y/R3IX9Zc1zxc7zxe2/0Iskt7AsG0hhx14W8XV43FgV4gAAAABJRU5ErkJggg=='
      }
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

    setTimeout(function() {
      callback(candidateTasks);
    }, 200);
  },

  /**
   * Executes a task.
   */
  executeTask: function(taskId, urlList) {
    console.log('executing task: ' + taskId + ': ' + urlList.length + ' urls');
    var parts = taskId.split('|');
    taskId = parts[parts.length - 1];
    function createEntry(url) {
      return {
        toURL: function() { return url; }
      };
    }
    var details = {entries: urlList.map(createEntry)};
    var listeners = chrome.fileBrowserHandler.onExecute.listeners_;
    for (var listener, index = 0; listener = listeners[index]; ++index) {
      listener(taskId, details);
    }
  },

  /**
   * Event fired on mount and unmount operations.
   */
  onDiskChanged: {
    listeners_: [],
    addListener: function(listener) {
      chrome.fileBrowserPrivate.onDiskChanged.listeners_.push(listener);
    }
  },

  addMount: function(source, type, options) {
    var event = {
      eventType: 'added',
      volumeInfo: {
        devicePath: source,
        type: type,
        mountPath: '/'
      }
    };
    var listeners = chrome.fileBrowserPrivate.onDiskChanged.listeners_;
    for (var listener, index = 0; listener = listeners[index]; ++index) {
      listener(event);
    }
  },

  getMountPoints: function(callback) {
    // This will work in harness.
    var path = 'filesystem:file:///persistent/media';
    var result = {};
    result[path] = {mountPath: path, type: 'file'};
    callback(result);
  },

  removeMount: function(mountPoint) {
    console.log('unmounted: ' + mountPoint);
    var event = {
      eventType: 'removed',
      volumeInfo: {
        devicePath: '',
        type: '',
        mountPath: mountPoint
      }
    };
    var listeners = chrome.fileBrowserPrivate.onDiskChanged.listeners_;
    for (var listener, index = 0; listener = listeners[index]; ++index) {
      listener(event);
    }
  },

  getSizeStats: function() {},

  /**
   * Return localized strings.
   */
  getStrings: function(callback) {
    // Keep this list in sync with the strings in generated_resources.grd and
    // extension_file_browser_private_api.cc!
    callback({
      // These two are from locale_settings*.grd
      WEB_FONT_FAMILY: 'Chrome Droid Sans,Droid Sans Fallback,sans-serif',
      WEB_FONT_SIZE: '84%',

      FILE_IS_DIRECTORY: 'Folder',
      PARENT_DIRECTORY: 'Parent Directory',

      ROOT_DIRECTORY_LABEL: 'Files',
      DOWNLOADS_DIRECTORY_LABEL: 'File Shelf',
      DOWNLOADS_DIRECTORY_WARNING: "&lt;strong&gt;Caution:&lt;/strong&gt; These files are temporary and may be automatically deleted to free up disk space.  &lt;a href='javascript://'&gt;Learn More&lt;/a&gt;",
      MEDIA_DIRECTORY_LABEL: 'External Storage',
      NAME_COLUMN_LABEL: 'Name',
      SIZE_COLUMN_LABEL: 'Size',
      TYPE_COLUMN_LABEL: 'Type',
      DATE_COLUMN_LABEL: 'Date',
      PREVIEW_COLUMN_LABEL: 'Preview',

      ERROR_CREATING_FOLDER: 'Unable to create folder "$1": $2',
      ERROR_INVALID_CHARACTER: 'Invalid character: $1',
      ERROR_RESERVED_NAME: 'This name may not be used as a file of folder name',
      ERROR_WHITESPACE_NAME: 'Invalid name',
      NEW_FOLDER_PROMPT: 'Enter a name for the new folder',
      ERROR_NEW_FOLDER_EMPTY_NAME: 'Please specify a folder name',
      NEW_FOLDER_BUTTON_LABEL: 'New folder',
      FILENAME_LABEL: 'File Name',

      DIMENSIONS_LABEL: 'Dimensions',
      DIMENSIONS_FORMAT: '$1 x $2',

      EJECT_BUTTON: 'Eject',
      IMAGE_DIMENSIONS: 'Image Dimensions',
      VOLUME_LABEL: 'Volume Label',
      READ_ONLY: 'Read Only',

      MOUNT_ARCHIVE: 'Open archive',
      UNMOUNT_ARCHIVE: 'Close archive',
      FORMAT_DEVICE: 'Format device',

      ENABLE_PHOTO_EDITOR: 'true',
      GALLERY: 'View and Edit',
      GALLERY_EDIT: 'Edit',
      GALLERY_SHARE: 'Share',
      GALLERY_AUTOFIX: 'Auto-fix',
      GALLERY_FIXED: 'Fixed',
      GALLERY_CROP: 'Crop',
      GALLERY_EXPOSURE: 'Brightness',
      GALLERY_BRIGHTNESS: 'Brightness',
      GALLERY_CONTRAST: 'Contrast',
      GALLERY_ROTATE_LEFT: 'Left',
      GALLERY_ROTATE_RIGHT: 'Right',
      GALLERY_ENTER_WHEN_DONE: 'Press Enter when done',
      GALLERY_UNDO: 'Undo',
      GALLERY_REDO: 'Redo',
      GALLERY_FILE_EXISTS: 'File already exists',

      CONFIRM_OVERWRITE_FILE: 'A file named "$1" already exists. Do you want to replace it?',
      FILE_ALREADY_EXISTS: 'The file named "$1" already exists. Please choose a different name.',
      DIRECTORY_ALREADY_EXISTS: 'The directory named "$1" already exists. Please choose a different name.',
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

      SELECTION_COPIED: 'Selection copied to clipboard.',
      SELECTION_CUT: 'Selection cut to clipboard.',
      PASTE_STARTED: 'Pasting...',
      PASTE_SOME_PROGRESS: 'Pasting $1 of $2 items...',
      PASTE_COMPLETE: 'Paste complete.',
      PASTE_CANCELLED: 'Paste cancelled.',
      PASTE_TARGET_EXISTS_ERROR: 'Paste failed, item exists: $1',
      PASTE_FILESYSTEM_ERROR: 'Paste failed, filesystem error: $1',
      PASTE_UNEXPECTED_ERROR: 'Paste failed, unexpected error: $1',

      DEVICE_TYPE_FLASH: 'Flash Device',
      DEVICE_TYPE_HDD: 'Hard Disk Device',
      DEVICE_TYPE_OPTICAL: 'Optical Device',
      DEVICE_TYPE_UNDEFINED: 'Unknown Device',

      CANCEL_LABEL: 'Cancel',
      OPEN_LABEL: 'Open',
      SAVE_LABEL: 'Save',
      OK_LABEL: 'OK',

      DEFAULT_NEW_FOLDER_NAME: 'New Folder',
      MORE_FILES: 'Show all files',

      SELECT_FOLDER_TITLE: 'Select a folder to open',
      SELECT_OPEN_FILE_TITLE: 'Select a file to open',
      SELECT_OPEN_MULTI_FILE_TITLE: 'Select one or more files',
      SELECT_SAVEAS_FILE_TITLE: 'Save file as',

      COMPUTING_SELECTION: 'Computing selection...',
      NOTHING_SELECTED: 'No files selected',
      ONE_FILE_SELECTED: 'One file selected, $1',
      ONE_DIRECTORY_SELECTED: 'One directory selected',
      MANY_FILES_SELECTED: '$1 files selected, $2',
      MANY_DIRECTORIES_SELECTED: '$1 directories selected',
      MANY_ENTRIES_SELECTED: '$1 items selected, $2',

      CONFIRM_DELETE_ONE: 'Are you sure you want to delete "$1"?',
      CONFIRM_DELETE_SOME: 'Are you sure you want to delete $1 items?',

      UNKNOWN_FILESYSTEM_WARNING:'This device cannot be opened because its' +
          ' filesystem was not recognized.',
      UNSUPPORTED_FILESYSTEM_WARNING: 'This device cannot be opened because' +
          ' its filesystem is not supported.',
      FORMATTING_WARNING: 'Formatting the removable media is going to erase' +
          ' all data. Do you wish to continue?',

      ID3_ALBUM: 'Album',  // TALB
      ID3_BPM: 'BPM ',  // TBPM
      ID3_COMPOSER: 'Composer',  // TCOM
      ID3_COPYRIGHT_MESSAGE: 'Copyright message',  // TCOP
      ID3_DATE: 'Date',  // TDAT
      ID3_PLAYLIST_DELAY: 'Playlist delay',  // TDLY
      ID3_ENCODED_BY: 'Encoded by',  // TENC
      ID3_LYRICIST: 'Lyricist',  // TEXT
      ID3_FILE_TYPE: 'File type',  // TFLT
      ID3_TIME: 'Time',  // TIME
      ID3_TITLE: 'Title',  // TIT2
      ID3_LENGTH: 'Length',  // TLEN
      ID3_FILE_OWNER: 'File owner',  // TOWN
      ID3_LEAD_PERFORMER: 'Artist',  // TPE1
      ID3_BAND: 'Band',  // TPE2
      ID3_TRACK_NUMBER: 'Track number',  // TRCK
      ID3_YEAR: 'Year',  // TYER
      ID3_COPYRIGHT: 'Copyright',  // WCOP
      ID3_OFFICIAL_AUDIO_FILE_WEBPAGE: 'Official audio file webpage',  // WOAF
      ID3_OFFICIAL_ARTIST: 'Official artist',  // WOAR
      ID3_OFFICIAL_AUDIO_SOURCE_WEBPAGE: 'Official audio source webpage', //WOAS
      ID3_PUBLISHERS_OFFICIAL_WEBPAGE: 'Publishers official webpage',  // WPUB
      ID3_USER_DEFINED_URL_LINK_FRAME: 'User defined URL link frame',  // WXXX

      FOLDER: 'Folder',
      DEVICE: 'Device',
      IMAGE_FILE_TYPE: '$1 image file',
      VIDEO_FILE_TYPE: '$1 video file',
      AUDIO_FILE_TYPE: '$1 audio file',
      HTML_DOCUMENT_FILE_TYPE: 'HTML document',
      ZIP_ARCHIVE_FILE_TYPE: 'Zip archive',
      PLAIN_TEXT_FILE_TYPE: 'Plain text file',
      PDF_DOCUMENT_FILE_TYPE: 'PDF document'
    });
  }
};

chrome.extension = {
  getURL: function(path) {
    return path || document.location.href;
  }
};

chrome.test = {
  verbose: false,

  sendMessage: function(msg) {
    if (chrome.test.verbose)
      console.log('chrome.test.sendMessage: ' + msg);
  }
};

chrome.fileBrowserHandler = {
  onExecute: {
    listeners_: [],
    addListener: function(listener) {
      chrome.fileBrowserHandler.onExecute.listeners_.push(listener);
    }
  }
};

chrome.tabs = {
  create: function(createOptions) {
    window.open(createOptions.url);
  }
};

chrome.metricsPrivate = {
  recordValue: function() {},
  recordMediumCount: function() {},
  recordTime: function() {},
  recordUserAction: function() {}
};

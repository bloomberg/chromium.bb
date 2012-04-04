// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function MockEventSource() {
  this.listeners_ = [];
}

MockEventSource.prototype.addListener = function(listener) {
  this.listeners_.push(listener);
};

MockEventSource.prototype.notify = function(var_args) {
  setTimeout(function(args) {
    for (var i = 0; i != this.listeners_.length; i++) {
      this.listeners_[i].apply(null, args);
    }
  }.bind(this, arguments), 0);
};

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
    for (var i = 0; i != selectedFiles.length; i++) {
      window.open(selectedFiles[i]);
    }
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
  onMountCompleted: new MockEventSource(),

  /**
   * File system change notification.
   */
  onFileChanged: new MockEventSource(),

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

    var emptyIcon = 'data:image/gif;base64,' +
        'R0lGODlhAQABAPABAP///wAAACH5BAEKAAAALAAAAAABAAEAAAICRAEAOw%3D%3D';

    var tasks =
    [ { taskId: extensionId + '|play',
        title: 'Listen',
        regexp: /\.(flac|m4a|mp3|oga|ogg|wav)$/i,
        iconUrl: emptyIcon
      },
      { taskId: extensionId + '|mount-archive',
        title: 'Mount',
        regexp: /\.(rar|tar|tar.bz2|tar.gz|tbz|tbz2|tgz|zip)$/i,
        iconUrl: emptyIcon
      },
      {
        taskId: extensionId + '|gallery',
        title: 'View',
        regexp: /\.(bmp|gif|jpe?g|png|webp|3gp|avi|m4v|mov|mp4|mpeg4?|mpg4?|ogm|ogv|ogx|webm)$/i,
        iconUrl: emptyIcon
      },
      {
        taskId: 'fake-extension-id|fake-item',
        title: 'External action',
        regexp: /\.(bmp|gif|jpe?g|png|webp|3gp|avi|m4v|mov|mp4|mpeg4?|mpg4?|ogm|ogv|ogx|webm)$/i,
        iconUrl: 'images/icon16.png'
      },
      {
        taskId: extensionId + '|view-txt',
        title: 'View',
        regexp: /\.txt$/i,
        iconUrl: emptyIcon
      },
      {
        taskId: extensionId + '|view-pdf',
        title: 'View',
        regexp: /\.pdf$/i,
        iconUrl: emptyIcon
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
    chrome.fileBrowserHandler.onExecute.notify(
        taskId, {entries: urlList.map(createEntry)});
  },

  /**
   * Event fired on mount and unmount operations.
   */
  onDiskChanged: new MockEventSource(),

  mountPoints_: [
    {
      mountPath: '/removable/disk1-writeable',
      type: 'device'
    },
    {
      mountPath: '/removable/disk2-readonly',
      type: 'device'
    },
    {
      mountPath: '/removable/disk3-unsupported',
      type: 'device',
      mountCondition: 'unsupported_filesystem'
    },
    {
      mountPath: '/removable/disk4-unknown',
      type: 'device',
      mountCondition: 'unknown_filesystem'
    }
  ],

  fsRe_: new RegExp('^filesystem:[^/]*://[^/]*/persistent(.*)'),

  fileUrlToLocalPath_: function(fileUrl) {
    var match = chrome.fileBrowserPrivate.fsRe_.exec(fileUrl);
    return match && match[1];
  },

  archiveCount_: 0,

  getMountPoints: function(callback) {
    callback([].concat(chrome.fileBrowserPrivate.mountPoints_));
  },


  addMount: function(source, type, options) {
    chrome.fileBrowserPrivate.requestLocalFileSystem(function(filesystem) {
      var path =
          (type == 'gdata') ?
          '/gdata' :
          ('/archive/archive' + (++chrome.fileBrowserPrivate.archiveCount_));
      util.getOrCreateDirectory(filesystem.root, path, function() {
          chrome.fileBrowserPrivate.mountPoints_.push({
            mountPath: path,
            type: type
          });
          chrome.fileBrowserPrivate.onMountCompleted.notify({
            eventType: 'mount',
            status: 'success',
            mountType: type,
            authToken: 'dummy',
            mountPath: path,
            sourceUrl: source
          });
          console.log('Created a mock mount at ' + path);
        },
        util.flog('Error creating a mock mount at ' + path));
    });
  },

  removeMount: function(mountPoint) {
    for (var i = 0; i != chrome.fileBrowserPrivate.mountPoints_.length; i++) {
      if (chrome.fileBrowserPrivate.fileUrlToLocalPath_(mountPoint) ==
          chrome.fileBrowserPrivate.mountPoints_[i].mountPath) {
        chrome.fileBrowserPrivate.mountPoints_.splice(i, 1);
        break;
      }
    }
    function notify() {
      chrome.fileBrowserPrivate.onMountCompleted.notify({
        eventType: 'unmount',
        status: 'success',
        mountPoint: mountPoint,
        sourceUrl: mountPoint
      });
    }
    webkitResolveLocalFileSystemURL(mountPoint, function(entry) {
      util.removeFileOrDirectory(entry,
          util.flog('Deleted a mock mount at ' + entry.fullPath, notify),
          util.flog('Error deleting a mock mount at' + entry.fullPath, notify));
    });
  },

  getSizeStats: function() {},

  getVolumeMetadata: function(url, callback) {
    var metadata = {};
    var urlLocalPath = chrome.fileBrowserPrivate.fileUrlToLocalPath_(url);
    function urlStartsWith(path) {
      return urlLocalPath && urlLocalPath.indexOf(path) == 0;
    }
    if (urlStartsWith('/removable')) {
      metadata.deviceType = 'usb';
      if (urlStartsWith('/removable/disk2')) {
        metadata.isReadOnly = true;
      }
    } else if (urlStartsWith('/gdata')) {
      metadata.deviceType = 'network';
    } else {
      metadata.deviceType = 'file';
    }
    callback(metadata);
  },

  pinned_: {},

  getGDataFileProperties: function(urls, callback) {
    var response = [];
    for (var i = 0; i != urls.length; i++) {
      var url = urls[i];
      response.push({
        fileUrl: url,
        isHosted: url.match(/\.g(doc|slides|sheet|draw|table)$/i),
        isPinned: (url in chrome.fileBrowserPrivate.pinned_)
      });
    }
    setTimeout(callback, 0, response);
  },

  pinGDataFile: function(urls, on, callback) {
    for (var i = 0; i != urls.length; i++) {
      var url = urls[i];
      if (on) {
        chrome.fileBrowserPrivate.pinned_[url] = true;
      } else {
        delete chrome.fileBrowserPrivate.pinned_[url];
      }
    }
    chrome.fileBrowserPrivate.getGDataFileProperties(urls, callback);
  },

  toggleFullscreen: function() {
    if (document.webkitIsFullScreen)
      document.webkitCancelFullScreen();
    else
      document.body.webkitRequestFullScreen();
  },

  isFullscreen: function(callback) {
    setTimeout(callback, 0, document.webkitIsFullScreen);
  },

  /**
   * Return localized strings.
   */
  getStrings: function(callback) {
    // Keep this list in sync with the strings in generated_resources.grd and
    // extension_file_browser_private_api.cc!
    callback({
      // These two are from locale_settings*.grd
      WEB_FONT_FAMILY: 'Open Sans,Chrome Droid Sans,'+
                       'Droid Sans Fallback,sans-serif',
      WEB_FONT_SIZE: '84%',

      FILE_IS_DIRECTORY: 'Folder',

      GDATA_DIRECTORY_LABEL: 'GData',
      ENABLE_GDATA: '1',
      PDF_VIEW_ENABLED: 'true',

      ROOT_DIRECTORY_LABEL: 'Files',
      DOWNLOADS_DIRECTORY_LABEL: 'Downloads',
      DOWNLOADS_DIRECTORY_WARNING: "&lt;strong&gt;Caution:&lt;/strong&gt; These files are temporary and may be automatically deleted to free up disk space.  &lt;a href='javascript://'&gt;Learn More&lt;/a&gt;",
      NAME_COLUMN_LABEL: 'Name',
      SIZE_COLUMN_LABEL: 'Size',
      SIZE_B: 'B',
      SIZE_KB: 'KB',
      SIZE_MB: 'MB',
      SIZE_GB: 'GB',
      SIZE_TB: 'TB',
      SIZE_PB: 'PB',
      TYPE_COLUMN_LABEL: 'Type',
      DATE_COLUMN_LABEL: 'Date',
      PREVIEW_COLUMN_LABEL: 'Preview',
      OFFILE_COLUMN_LABEL: 'Available offline',

      ERROR_CREATING_FOLDER: 'Unable to create folder "$1": $2',
      ERROR_INVALID_CHARACTER: 'Invalid character: $1',
      ERROR_RESERVED_NAME: 'This name may not be used as a file of folder name',
      ERROR_WHITESPACE_NAME: 'Invalid name',
      ERROR_NEW_FOLDER_EMPTY_NAME: 'Please specify a folder name',
      NEW_FOLDER_BUTTON_LABEL: 'New folder',
      FILENAME_LABEL: 'File Name',
      PREPARING_LABEL: 'Preparing',

      DIMENSIONS_LABEL: 'Dimensions',
      DIMENSIONS_FORMAT: '$1 x $2',

      EJECT_BUTTON: 'Eject',
      IMAGE_DIMENSIONS: 'Image Dimensions',
      VOLUME_LABEL: 'Volume Label',
      READ_ONLY: 'Read Only',

      PLAY_MEDIA: 'Play',

      MOUNT_ARCHIVE: 'Open',
      FORMAT_DEVICE: 'Format device',

      ACTION_VIEW: 'View',
      ACTION_OPEN: 'Open',
      ACTION_WATCH: 'Watch',
      ACTION_LISTEN: 'Listen',
      INSTALL_CRX: 'Open',

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
      GALLERY_FILE_HIDDEN_NAME: 'Names starting with dot are reserved ' +
          'for the system. Please choose another name.',
      GALLERY_SAVED: 'Saved',
      GALLERY_KEEP_ORIGINAL: 'Keep original',
      GALLERY_UNSAVED_CHANGES: 'Changes are not saved yet.',
      GALLERY_READONLY_WARNING: '$1 is read only. Edited images will be saved in the Downloads folder.',
      GALLERY_IMAGE_ERROR: 'This file could not be displayed',
      GALLERY_VIDEO_ERROR: 'This file could not be played',
      AUDIO_ERROR: 'This file could not be played',

      CONFIRM_OVERWRITE_FILE: 'A file named "$1" already exists. Do you want to replace it?',
      FILE_ALREADY_EXISTS: 'The file named "$1" already exists. Please choose a different name.',
      DIRECTORY_ALREADY_EXISTS: 'The folder named "$1" already exists. Please choose a different name.',
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

      UNMOUNT_DEVICE_BUTTON_LABEL: 'Unmount',
      FORMAT_DEVICE_BUTTON_LABEL: 'Format',

      GDATA_MOBILE_CONNECTION_OPTION: 'Do not use mobile data for sync',
      GDATA_SHOW_HOSTED_FILES_OPTION: 'Show Google Docs files',

      PASTE_ITEMS_REMAINING: 'Pasting $1 items',
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
      NO_ACTION_FOR_FILE: "To view this file, convert it to a format that's viewable on the web. <a target='_blank' href='$1'>Learn More.</a>",

      DEFAULT_NEW_FOLDER_NAME: 'New Folder',
      MORE_FILES: 'Show all files',

      SELECT_FOLDER_TITLE: 'Select a folder to open',
      SELECT_OPEN_FILE_TITLE: 'Select a file to open',
      SELECT_OPEN_MULTI_FILE_TITLE: 'Select one or more files',
      SELECT_SAVEAS_FILE_TITLE: 'Save file as',

      COMPUTING_SELECTION: 'Computing selection...',
      ONE_FILE_SELECTED: 'One file selected, $1',
      ONE_DIRECTORY_SELECTED: 'One folder selected',
      MANY_FILES_SELECTED: '$1 files selected, $2',
      MANY_DIRECTORIES_SELECTED: '$1 folders selected',
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
      IMAGE_FILE_TYPE: '$1 image',
      VIDEO_FILE_TYPE: '$1 video',
      AUDIO_FILE_TYPE: '$1 audio',
      HTML_DOCUMENT_FILE_TYPE: 'HTML document',
      ZIP_ARCHIVE_FILE_TYPE: 'Zip archive',
      RAR_ARCHIVE_FILE_TYPE: 'RAR archive',
      TAR_ARCHIVE_FILE_TYPE: 'Tar archive',
      TAR_BZIP2_ARCHIVE_FILE_TYPE: 'Bzip2 compressed tar archive',
      TAR_GZIP_ARCHIVE_FILE_TYPE: 'Gzip compressed tar archive',
      PLAIN_TEXT_FILE_TYPE: 'Plain text file',
      PDF_DOCUMENT_FILE_TYPE: 'PDF document',
      WORD_DOCUMENT_FILE_TYPE: 'Word document',
      POWERPOINT_PRESENTATION_FILE_TYPE: 'PowerPoint presentation',
      EXCEL_FILE_TYPE: 'Excel spreadsheet'
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
  onExecute: new MockEventSource()
};

chrome.tabs = {
  create: function(createOptions) {
    window.open(createOptions.url);
  }
};

chrome.metricsPrivate = {
  recordMediumCount: function() {},
  recordSmallCount: function() {},
  recordTime: function() {},
  recordUserAction: function() {},
  recordValue: function() {}
};

chrome.mediaPlayerPrivate = {

  onPlaylistChanged: new MockEventSource(),

  play: function(urls, position) {
    this.playlist_ = { items: urls, position: position };

    if (this.popup_) {
      this.onPlaylistChanged.notify();
      return;
    }

    // Using global document is OK for the test harness.
    this.popup_ = document.createElement('iframe');
    this.popup_.scrolling = 'no';
    this.popup_.style.cssText = 'position:absolute; border:none; z-index:10;' +
        'width:280px; height:93px; right:10px; bottom:80px;' +
        '-webkit-transition: height 200ms ease';

    document.body.appendChild(this.popup_);

    this.popup_.onload = function() {
      var win = this.popup_.contentWindow;
      win.chrome = chrome;
      win.AudioPlayer.load();
    }.bind(this);

    this.popup_.src = "mediaplayer.html?no_auto_load";
  },

  getPlaylist: function(callback) {
    callback(this.playlist_);
  },

  setWindowHeight: function(height) {
    this.popup_.style.height = height + 'px';
  },

  closeWindow: function() {
    this.popup_.parentNode.removeChild(this.popup_);
    this.popup_ = null;
  }
};

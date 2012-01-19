// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Setting the src of an img to an empty string can crash the browser, so we
// use an empty 1x1 gif instead.
const EMPTY_IMAGE_URI = 'data:image/gif;base64,'
        + 'R0lGODlhAQABAPABAP///wAAACH5BAEKAAAALAAAAAABAAEAAAICRAEAOw%3D%3D';

/**
 * FileManager constructor.
 *
 * FileManager objects encapsulate the functionality of the file selector
 * dialogs, as well as the full screen file manager application (though the
 * latter is not yet implemented).
 *
 * @param {HTMLElement} dialogDom The DOM node containing the prototypical
 *     dialog UI.
 * @param {DOMFileSystem} filesystem The HTML5 filesystem object representing
 *     the root filesystem for the new FileManager.
 */
function FileManager(dialogDom) {
  this.dialogDom_ = dialogDom;
  this.filesystem_ = null;
  this.params_ = location.search ?
                 JSON.parse(decodeURIComponent(location.search.substr(1))) :
                 {};

  this.listType_ = null;

  this.selection = null;

  this.clipboard_ = null;  // Current clipboard, or null if empty.

  this.butterTimer_ = null;
  this.currentButter_ = null;

  // True if we should filter out files that start with a dot.
  this.filterFiles_ = true;
  this.subscribedOnDirectoryChanges_ = false;

  this.commands_ = {};

  this.document_ = dialogDom.ownerDocument;
  this.dialogType_ = this.params_.type || FileManager.DialogType.FULL_PAGE;

  metrics.recordEnum('Create', this.dialogType_,
      [FileManager.DialogType.SELECT_FOLDER,
      FileManager.DialogType.SELECT_SAVEAS_FILE,
      FileManager.DialogType.SELECT_OPEN_FILE,
      FileManager.DialogType.SELECT_OPEN_MULTI_FILE,
      FileManager.DialogType.FULL_PAGE]);

  // TODO(dgozman): This will be changed to LocaleInfo.
  this.locale_ = new v8Locale(navigator.language);

  this.initFileSystem_();
  this.initDom_();
  this.initDialogType_();
  this.dialogDom_.style.opacity = '1';
}

FileManager.prototype = {
  __proto__: cr.EventTarget.prototype
};

// Anonymous "namespace".
(function() {

  // Private variables and helper functions.

  /**
   * Location of the FAQ about the downloads directory.
   */
  const DOWNLOADS_FAQ_URL = 'http://www.google.com/support/chromeos/bin/' +
      'answer.py?hl=en&answer=1061547';

  /**
   * Mnemonics for the recurse parameter of the copyFiles method.
   */
  const CP_RECURSE = true;
  const CP_NO_RECURSE = false;

  /**
   * Maximum amount of thumbnails in the preview pane.
   */
  const MAX_PREVIEW_THUMBAIL_COUNT = 4;

  /**
   * Maximum width or height of an image what pops up when the mouse hovers
   * thumbnail in the bottom panel (in pixels).
   */
  const IMAGE_HOVER_PREVIEW_SIZE = 200;

  /**
   * Translated strings.
   */
  var localStrings;

  const fileTypes = {
    // Images
    'jpeg': {type: 'image', name: 'IMAGE_FILE_TYPE', subtype: 'JPEG'},
    'jpg':  {type: 'image', name: 'IMAGE_FILE_TYPE', subtype: 'JPEG'},
    'bmp':  {type: 'image', name: 'IMAGE_FILE_TYPE', subtype: 'BMP'},
    'gif':  {type: 'image', name: 'IMAGE_FILE_TYPE', subtype: 'GIF'},
    'ico':  {type: 'image', name: 'IMAGE_FILE_TYPE', subtype: 'ICO'},
    'png':  {type: 'image', name: 'IMAGE_FILE_TYPE', subtype: 'PNG'},
    'webp': {type: 'image', name: 'IMAGE_FILE_TYPE', subtype: 'WebP'},

    // Video
    '3gp':  {type: 'video', name: 'VIDEO_FILE_TYPE', subtype: '3GP'},
    'avi':  {type: 'video', name: 'VIDEO_FILE_TYPE', subtype: 'AVI'},
    'mov':  {type: 'video', name: 'VIDEO_FILE_TYPE', subtype: 'QuickTime'},
    'mp4':  {type: 'video', name: 'VIDEO_FILE_TYPE', subtype: 'MPEG'},
    'mpg':  {type: 'video', name: 'VIDEO_FILE_TYPE', subtype: 'MPEG'},
    'mpeg': {type: 'video', name: 'VIDEO_FILE_TYPE', subtype: 'MPEG'},
    'mpg4': {type: 'video', name: 'VIDEO_FILE_TYPE', subtype: 'MPEG'},
    'mpeg4': {type: 'video', name: 'VIDEO_FILE_TYPE', subtype: 'MPEG'},
    'ogm':  {type: 'video', name: 'VIDEO_FILE_TYPE', subtype: 'OGG'},
    'ogv':  {type: 'video', name: 'VIDEO_FILE_TYPE', subtype: 'OGG'},
    'ogx':  {type: 'video', name: 'VIDEO_FILE_TYPE', subtype: 'OGG'},
    'webm': {type: 'video', name: 'VIDEO_FILE_TYPE', subtype: 'WebM'},

    // Audio
    'flac': {type: 'audio', name: 'AUDIO_FILE_TYPE', subtype: 'FLAC'},
    'mp3':  {type: 'audio', name: 'AUDIO_FILE_TYPE', subtype: 'MP3'},
    'm4a':  {type: 'audio', name: 'AUDIO_FILE_TYPE', subtype: 'MPEG'},
    'oga':  {type: 'audio', name: 'AUDIO_FILE_TYPE', subtype: 'OGG'},
    'ogg':  {type: 'audio', name: 'AUDIO_FILE_TYPE', subtype: 'OGG'},
    'wav':  {type: 'audio', name: 'AUDIO_FILE_TYPE', subtype: 'WAV'},

    // Text
    'pod': {type: 'text', name: 'PLAIN_TEXT_FILE_TYPE', subtype: 'POD'},
    'rst': {type: 'text', name: 'PLAIN_TEXT_FILE_TYPE', subtype: 'RST'},
    'txt': {type: 'text', name: 'PLAIN_TEXT_FILE_TYPE', subtype: 'TXT'},
    'log': {type: 'text', name: 'PLAIN_TEXT_FILE_TYPE', subtype: 'LOG'},

    // Others
    'zip': {type: 'archive', name: 'ZIP_ARCHIVE_FILE_TYPE'},

    'pdf': {type: 'text', icon: 'pdf', name: 'PDF_DOCUMENT_FILE_TYPE',
            subtype: 'PDF'},
    'html': {type: 'text', icon: 'html', name: 'HTML_DOCUMENT_FILE_TYPE',
             subtype: 'HTML'},
    'htm': {type: 'text', icon: 'html', name: 'HTML_DOCUMENT_FILE_TYPE',
            subtype: 'HTML'}
  };

  const previewArt = {
    'audio': 'images/filetype_large_audio.png',
    // TODO(sidor): Find better icon here.
    'device': 'images/filetype_large_folder.png',
    'folder': 'images/filetype_large_folder.png',
    'unknown': 'images/filetype_large_generic.png',
    // TODO(sidor): Find better icon here.
    'unreadable': 'images/filetype_large_folder.png',
    'image': 'images/filetype_large_image.png',
    'video': 'images/filetype_large_video.png'
  };

  /**
   * Regexp for archive files. Used to show mount-archive task.
   */
  const ARCHIVES_REGEXP = /.zip$/;

  /**
   * Item for the Grid View.
   * @constructor
   */
  function GridItem(fileManager, entry) {
    var li = fileManager.document_.createElement('li');
    GridItem.decorate(li, fileManager, entry);
    return li;
  }

  GridItem.prototype = {
    __proto__: cr.ui.ListItem.prototype,

    get label() {
      return this.querySelector('filename-label').textContent;
    },
    set label(value) {
      // Grid sets it to entry. Ignore.
    }
  };

  GridItem.decorate = function(li, fileManager, entry) {
    li.__proto__ = GridItem.prototype;
    fileManager.decorateThumbnail_(li, entry);
  };


  /**
   * Return a translated string.
   *
   * Wrapper function to make dealing with translated strings more concise.
   * Equivilant to localStrings.getString(id).
   *
   * @param {string} id The id of the string to return.
   * @return {string} The translated string.
   */
  function str(id) {
    return localStrings.getString(id);
  }

  /**
   * Return a translated string with arguments replaced.
   *
   * Wrapper function to make dealing with translated strings more concise.
   * Equivilant to localStrings.getStringF(id, ...).
   *
   * @param {string} id The id of the string to return.
   * @param {...string} The values to replace into the string.
   * @return {string} The translated string with replaced values.
   */
  function strf(id, var_args) {
    return localStrings.getStringF.apply(localStrings, arguments);
  }

  /**
   * Checks if |parent_path| is parent file path of |child_path|.
   *
   * @param {string} parent_path The parent path.
   * @param {string} child_path The child path.
   */
  function isParentPath(parent_path, child_path) {
    if (!parent_path || parent_path.length == 0 ||
        !child_path || child_path.length == 0)
      return false;

    if (parent_path[parent_path.length -1] != '/')
      parent_path += '/';

    if (child_path[child_path.length -1] != '/')
      child_path += '/';

    return child_path.indexOf(parent_path) == 0;
  }

  /**
   * Returns parent folder path of file path.
   *
   * @param {string} path The file path.
   */
  function getParentPath(path) {
    var parent = path.replace(/[\/]?[^\/]+[\/]?$/,'');
    if (parent.length == 0)
      parent = '/';
    return parent;
  }

 /**
  * Normalizes path not to start with /
  *
  * @param {string} path The file path.
  */
  function normalizeAbsolutePath(x) {
    if (x[0] == '/')
      return x.slice(1);
    else
      return x;
  }


  /**
   * Call an asynchronous method on dirEntry, batching multiple callers.
   *
   * This batches multiple callers into a single invocation, calling all
   * interested parties back when the async call completes.
   *
   * The Entry method to be invoked should take two callbacks as parameters
   * (one for success and one for failure), and it should invoke those
   * callbacks with a single parameter representing the result of the call.
   * Example methods are Entry.getMetadata() and FileEntry.file().
   *
   * Warning: Because this method caches the first result, subsequent changes
   * to the entry will not be visible to callers.
   *
   * Error results are never cached.
   *
   * @param {DirectoryEntry} dirEntry The DirectoryEntry to apply the method
   *     to.
   * @param {string} methodName The name of the method to dispatch.
   * @param {function(*)} successCallback The function to invoke if the method
   *     succeeds.  The result of the method will be the one parameter to this
   *     callback.
   * @param {function(*)} opt_errorCallback The function to invoke if the
   *     method fails.  The result of the method will be the one parameter to
   *     this callback.  If not provided, the default errorCallback will throw
   *     an exception.
   */
  function batchAsyncCall(entry, methodName, successCallback,
                          opt_errorCallback) {
    var resultCache = methodName + '_resultCache_';

    if (entry[resultCache]) {
      // The result cache for this method already exists.  Just invoke the
      // successCallback with the result of the previuos call.
      // Callback via a setTimeout so the sync/async semantics don't change
      // based on whether or not the value is cached.
      setTimeout(function() { successCallback(entry[resultCache]) }, 0);
      return;
    }

    if (!opt_errorCallback) {
      opt_errorCallback = util.ferr('Error calling ' + methodName + ' for: ' +
                                    entry.fullPath);
    }

    var observerList = methodName + '_observers_';

    if (entry[observerList]) {
      // The observer list already exists, indicating we have a pending call
      // out to this method.  Add this caller to the list of observers and
      // bail out.
      entry[observerList].push([successCallback, opt_errorCallback]);
      return;
    }

    entry[observerList] = [[successCallback, opt_errorCallback]];

    function onComplete(success, result) {
      if (success)
        entry[resultCache] = result;

      for (var i = 0; i < entry[observerList].length; i++) {
        entry[observerList][i][success ? 0 : 1](result);
      }

      delete entry[observerList];
    };

    entry[methodName](function(rv) { onComplete(true, rv) },
                      function(rv) { onComplete(false, rv) });
  }

  /**
   * Invoke callback in sync/async manner.
   * @param {function(*)?} callback The callback. If null, nothing is called.
   * @param {boolean} sync True iff the callback should be called synchronously.
   * @param {*} callback_args... The rest are callback arguments.
   */
  function invokeCallback(callback, sync, callback_args) {
    if (!callback)
      return;
    var args = Array.prototype.slice.call(arguments, 2);
    if (sync) {
      callback.apply(null, args);
    } else {
      setTimeout(function() { callback.apply(null, args); }, 0);
    }
  }

  /**
   * Get the size of a file, caching the result.
   *
   * When this method completes, the fileEntry object will get a
   * 'cachedSize_' property (if it doesn't already have one) containing the
   * size of the file in bytes.
   *
   * @param {Entry} entry An HTML5 Entry object.
   * @param {function(Entry)} successCallback The function to invoke once the
   *     file size is known.
   * @param {function=} opt_errorCallback Error callback.
   * @param {boolean=} opt_sync True, if callback should be called sync instead
   *     of async.
   */
  function cacheEntrySize(entry, successCallback, opt_errorCallback, opt_sync) {
    if (entry.isDirectory) {
      // No size for a directory, -1 ensures it's sorted before 0 length files.
      entry.cachedSize_ = -1;
    }

    if ('cachedSize_' in entry) {
      invokeCallback(successCallback, !!opt_sync, entry);
      return;
    }

    batchAsyncCall(entry, 'file', function(file) {
      entry.cachedSize_ = file.size;
      if (successCallback)
        successCallback(entry);
    }, opt_errorCallback);
  }

  /**
   * Get the mtime of a file, caching the result.
   *
   * When this method completes, the fileEntry object will get a
   * 'cachedMtime_' property (if it doesn't already have one) containing the
   * last modified time of the file as a Date object.
   *
   * @param {Entry} entry An HTML5 Entry object.
   * @param {function(Entry)} successCallback The function to invoke once the
   *     mtime is known.
   * @param {function=} opt_errorCallback Error callback.
   * @param {boolean=} opt_sync True, if callback should be called sync instead
   *     of async.
   */
  function cacheEntryDate(entry, successCallback, opt_errorCallback, opt_sync) {
    if ('cachedMtime_' in entry) {
      invokeCallback(successCallback, !!opt_sync, entry);
      return;
    }

    if (entry.isFile) {
      batchAsyncCall(entry, 'file', function(file) {
        entry.cachedMtime_ = file.lastModifiedDate;
        if (successCallback)
          successCallback(entry);
      });
    } else {
      batchAsyncCall(entry, 'getMetadata', function(metadata) {
        entry.cachedMtime_ = metadata.modificationTime;
        if (successCallback)
          successCallback(entry);
      }, opt_errorCallback);
    }
  }

  function removeChildren(element) {
    element.textContent = '';
  };

  // Public statics.

  /**
   * List of dialog types.
   *
   * Keep this in sync with FileManagerDialog::GetDialogTypeAsString, except
   * FULL_PAGE which is specific to this code.
   *
   * @enum {string}
   */
  FileManager.DialogType = {
    SELECT_FOLDER: 'folder',
    SELECT_SAVEAS_FILE: 'saveas-file',
    SELECT_OPEN_FILE: 'open-file',
    SELECT_OPEN_MULTI_FILE: 'open-multi-file',
    FULL_PAGE: 'full-page'
  };

  FileManager.DialogType.isModal = function(type) {
    return type == FileManager.DialogType.SELECT_FOLDER ||
        type == FileManager.DialogType.SELECT_SAVEAS_FILE ||
        type == FileManager.DialogType.SELECT_OPEN_FILE ||
        type == FileManager.DialogType.SELECT_OPEN_MULTI_FILE;
  };

  FileManager.ListType = {
    DETAIL: 'detail',
    THUMBNAIL: 'thumb'
  };

  /**
   * Load translated strings.
   */
  FileManager.initStrings = function(callback) {
    chrome.fileBrowserPrivate.getStrings(function(strings) {
      localStrings = new LocalStrings(strings);
      if (callback)
        callback();
    });
  };

  // Instance methods.

  /**
   * Request local file system, resolve roots and init_ after that.
   * @private
   */
  FileManager.prototype.initFileSystem_ = function() {
    util.installFileErrorToString();
    metrics.startInterval('Load.FileSystem');

    var self = this;

    // The list of active mount points to distinct them from other directories.
    chrome.fileBrowserPrivate.getMountPoints(function(mountPoints) {
      self.mountPoints_ = mountPoints;
      onDone();
    });

    function onDone() {
      if (self.mountPoints_ && self.filesystem_)
        self.init_();
    }

    chrome.fileBrowserPrivate.requestLocalFileSystem(function(filesystem) {
      metrics.recordInterval('Load.FileSystem');
      self.filesystem_ = filesystem;
      onDone();
    });
  };

  /**
   * Continue initializing the file manager after resolving roots.
   */
  FileManager.prototype.init_ = function() {
    metrics.startInterval('Load.DOM');
    this.initCommands_();

    // TODO(rginda): 6/22/11: Remove this test when createDateTimeFormat is
    // available in all chrome trunk builds.
    if ('createDateTimeFormat' in this.locale_) {
      this.shortDateFormatter_ =
        this.locale_.createDateTimeFormat({'dateType': 'medium'});
    } else {
      this.shortDateFormatter_ = {
        format: function(d) {
          return (d.getMonth() + 1) + '/' + d.getDate() + '/' + d.getFullYear();
        }
      };
    }

    // TODO(rginda): 6/22/11: Remove this test when createCollator is
    // available in all chrome trunk builds.
    if ('createCollator' in this.locale_) {
      this.collator_ = this.locale_.createCollator({
        'numeric': true, 'ignoreCase': true, 'ignoreAccents': true});
    } else {
      this.collator_ = {
        compare: function(a, b) {
          if (a > b) return 1;
          if (a < b) return -1;
          return 0;
        }
      };
    }

    // Optional list of file types.
    this.fileTypes_ = this.params_.typeList;

    this.showCheckboxes_ =
        (this.dialogType_ == FileManager.DialogType.FULL_PAGE ||
         this.dialogType_ == FileManager.DialogType.SELECT_OPEN_MULTI_FILE);

    this.table_.list.startBatchUpdates();
    this.grid_.startBatchUpdates();

    this.initFileList_();
    this.initDialogs_();

    this.copyManager_ = new FileCopyManager();
    this.copyManager_.addEventListener('copy-progress',
                                       this.onCopyProgress_.bind(this));

    window.addEventListener('popstate', this.onPopState_.bind(this));
    window.addEventListener('unload', this.onUnload_.bind(this));

    this.directoryModel_.addEventListener('directory-changed',
                                          this.onDirectoryChanged_.bind(this));
    this.addEventListener('selection-summarized',
                          this.onSelectionSummarized_.bind(this));

    // The list of archives requested to mount. We will show contents once
    // archive is mounted, but only for mounts from within this filebrowser tab.
    this.mountRequests_ = [];
    this.unmountRequests_ = [];
    chrome.fileBrowserPrivate.onMountCompleted.addListener(
        this.onMountCompleted_.bind(this));

    chrome.fileBrowserPrivate.onFileChanged.addListener(
        this.onFileChanged_.bind(this));

    var self = this;

    // The list of callbacks to be invoked during the directory rescan after
    // all paste tasks are complete.
    this.pasteSuccessCallbacks_ = [];

    this.setupCurrentDirectory_();

    this.summarizeSelection_();

    this.directoryModel_.fileList.sort('cachedMtime_', 'desc');

    this.refocus();

    this.createMetadataProvider_();

    this.table_.list.endBatchUpdates();
    this.grid_.endBatchUpdates();

    metrics.recordInterval('Load.DOM');
    metrics.recordInterval('Load.Total');
  };

  /**
   * One-time initialization of commands.
   */
  FileManager.prototype.initCommands_ = function() {
    var commands = this.dialogDom_.querySelectorAll('command');
    for (var i = 0; i < commands.length; i++) {
      var command = commands[i];
      cr.ui.Command.decorate(command);
      this.commands_[command.id] = command;
    }

    this.fileContextMenu_ = this.dialogDom_.querySelector('.file-context-menu');
    cr.ui.Menu.decorate(this.fileContextMenu_);

    this.document_.addEventListener('canExecute',
                                    this.onCanExecute_.bind(this));
    this.document_.addEventListener('command', this.onCommand_.bind(this));
  }

  /**
   * One-time initialization of dialogs.
   */
  FileManager.prototype.initDialogs_ = function() {
    var d = cr.ui.dialogs;
    d.BaseDialog.OK_LABEL = str('OK_LABEL');
    d.BaseDialog.CANCEL_LABEL = str('CANCEL_LABEL');
    this.alert = new d.AlertDialog(this.dialogDom_);
    this.confirm = new d.ConfirmDialog(this.dialogDom_);
    this.prompt = new d.PromptDialog(this.dialogDom_);
  };

  /**
   * One-time initialization of various DOM nodes.
   */
  FileManager.prototype.initDom_ = function() {
    // Cache nodes we'll be manipulating.
    this.previewThumbnails_ =
        this.dialogDom_.querySelector('.preview-thumbnails');
    this.previewPanel_ = this.dialogDom_.querySelector('.preview-panel');
    this.previewFilename_ = this.dialogDom_.querySelector('.preview-filename');
    this.previewSummary_ = this.dialogDom_.querySelector('.preview-summary');
    this.filenameInput_ = this.dialogDom_.querySelector('.filename-input');
    this.taskButtons_ = this.dialogDom_.querySelector('.task-buttons');
    this.okButton_ = this.dialogDom_.querySelector('.ok');
    this.cancelButton_ = this.dialogDom_.querySelector('.cancel');
    this.deleteButton_ = this.dialogDom_.querySelector('.delete-button');
    this.table_ = this.dialogDom_.querySelector('.detail-table');
    this.grid_ = this.dialogDom_.querySelector('.thumbnail-grid');

    cr.ui.Table.decorate(this.table_);
    cr.ui.Grid.decorate(this.grid_);

    this.downloadsWarning_ =
        this.dialogDom_.querySelector('.downloads-warning');
    var html = util.htmlUnescape(str('DOWNLOADS_DIRECTORY_WARNING'));
    this.downloadsWarning_.lastElementChild.innerHTML = html;
    var link = this.downloadsWarning_.querySelector('a');
    link.addEventListener('click', this.onDownloadsWarningClick_.bind(this));

    this.document_.addEventListener('keydown', this.onKeyDown_.bind(this));

    this.renameInput_ = this.document_.createElement('input');
    this.renameInput_.className = 'rename';

    this.renameInput_.addEventListener(
        'keydown', this.onRenameInputKeyDown_.bind(this));
    this.renameInput_.addEventListener(
        'blur', this.onRenameInputBlur_.bind(this));

    this.filenameInput_.addEventListener(
        'keyup', this.onFilenameInputKeyUp_.bind(this));
    this.filenameInput_.addEventListener(
        'focus', this.onFilenameInputFocus_.bind(this));

    var listContainer = this.dialogDom_.querySelector('.list-container');
    listContainer.addEventListener('keydown', this.onListKeyDown_.bind(this));
    listContainer.addEventListener('keypress', this.onListKeyPress_.bind(this));
    this.okButton_.addEventListener('click', this.onOk_.bind(this));
    this.cancelButton_.addEventListener('click', this.onCancel_.bind(this));

    this.dialogDom_.querySelector('div.open-sidebar').addEventListener(
        'click', this.onToggleSidebar_.bind(this));
    this.dialogDom_.querySelector('div.close-sidebar').addEventListener(
        'click', this.onToggleSidebar_.bind(this));
    this.dialogContainer_ = this.dialogDom_.querySelector('.dialog-container');

    this.dialogDom_.querySelector('button.detail-view').addEventListener(
        'click', this.onDetailViewButtonClick_.bind(this));
    this.dialogDom_.querySelector('button.thumbnail-view').addEventListener(
        'click', this.onThumbnailViewButtonClick_.bind(this));

    this.dialogDom_.ownerDocument.defaultView.addEventListener(
        'resize', this.onResize_.bind(this));

    var ary = this.dialogDom_.querySelectorAll('[visibleif]');
    for (var i = 0; i < ary.length; i++) {
      var expr = ary[i].getAttribute('visibleif');
      if (!eval(expr))
        ary[i].style.display = 'none';
    }

    // Populate the static localized strings.
    i18nTemplate.process(this.document_, localStrings.templateData);
  };

  /**
   * Constructs table and grid (heavy operation).
   **/
  FileManager.prototype.initFileList_ = function() {
    // Always sharing the data model between the detail/thumb views confuses
    // them.  Instead we maintain this bogus data model, and hook it up to the
    // view that is not in use.
    this.emptyDataModel_ = new cr.ui.ArrayDataModel([]);
    this.emptySelectionModel_ = new cr.ui.ListSelectionModel();

    var sigleSelection =
        this.dialogType_ == FileManager.DialogType.SELECT_OPEN_FILE ||
        this.dialogType_ == FileManager.DialogType.SELECT_OPEN_FOLDER ||
        this.dialogType_ == FileManager.DialogType.SELECT_SAVEAS_FILE;

    this.directoryModel_ = new DirectoryModel(this.filesystem_.root,
                                              sigleSelection);

    var dataModel = this.directoryModel_.fileList;
    var collator = this.collator_;
    dataModel.setCompareFunction('name', function(a, b) {
      return collator.compare(a.name, b.name);
    });
    dataModel.setCompareFunction('cachedMtime_',
                                 this.compareMtime_.bind(this));
    dataModel.setCompareFunction('cachedSize_',
                                 this.compareSize_.bind(this));
    dataModel.setCompareFunction('type',
                                 this.compareType_.bind(this));

    dataModel.addEventListener('splice',
                               this.onDataModelSplice_.bind(this));

    this.directoryModel_.fileListSelection.addEventListener(
        'change', this.onSelectionChanged_.bind(this));

    this.directoryModel_.autoSelectIndex =
        this.dialogType_ == FileManager.DialogType.SELECT_SAVEAS_FILE ? -1 : 0;

    // TODO(serya): temporary solution.
    this.directoryModel_.cacheEntryDate = cacheEntryDate;
    this.directoryModel_.cacheEntrySize = cacheEntrySize;
    this.directoryModel_.cacheEntryFileType =
        this.cacheEntryFileType.bind(this);
    this.directoryModel_.cacheEntryIconType =
        this.cacheEntryIconType.bind(this);

    this.initTable_();
    this.initGrid_();
    this.initRootsList_();

    var listType = FileManager.ListType.DETAIL;
    if (FileManager.DialogType.isModal(this.dialogType_))
      listType = window.localStorage['listType-' + this.dialogType_] ||
          FileManager.ListType.DETAIL;
    this.setListType(listType);

    this.textSearchState_ = {text: '', date: new Date()};
  };

  FileManager.prototype.initRootsList_ = function() {
    this.rootsList_ = this.dialogDom_.querySelector('.roots-list');
    cr.ui.List.decorate(this.rootsList_);

    var self = this;
    this.rootsList_.itemConstructor = function(entry) {
      return self.renderRoot_(entry);
    };

    this.rootsList_.selectionModel = this.directoryModel_.rootsListSelection;

    // TODO(dgozman): add "Add a drive" item.
    this.rootsList_.dataModel = this.directoryModel_.rootsList;
    this.directoryModel_.updateRoots();
  };

  /**
   * Get the icon type for a given Entry.
   *
   * @param {Entry} entry An Entry subclass (FileEntry or DirectoryEntry).
   * @return {string}
   */
  FileManager.prototype.getIconType = function(entry) {
    if (!('cachedIconType_' in entry))
      entry.cachedIconType_ = this.computeIconType_(entry);
    return entry.cachedIconType_;
  };

  /**
   * Extract extension from the file name and cat it to to lower case.
   *
   * @param {string} name.
   * @return {strin}
   */
  function getFileExtension(name) {
    var extIndex = name.lastIndexOf('.');
    if (extIndex < 0)
      return '';

    return name.substr(extIndex + 1).toLowerCase();
  }

  FileManager.prototype.computeIconType_ = function(entry) {
    // TODO(dgozman): refactor this to use proper icons in left panel,
    // and do not depend on mountPoints.
    var deviceNumber = this.getDeviceNumber(entry);
    if (deviceNumber != undefined) {
      if (this.mountPoints_[deviceNumber].mountCondition == '')
        return 'device';
      var mountCondition = this.mountPoints_[deviceNumber].mountCondition;
      if (mountCondition == 'unknown_filesystem' ||
          mountCondition == 'unsupported_filesystem')
        return 'unreadable';
    }
    if (entry.isDirectory)
      return 'folder';

    var extension = getFileExtension(entry.name);
    if (fileTypes[extension])
      return fileTypes[extension].icon || fileTypes[extension].type;
    return undefined;
  };

  /**
   * Get the file type for a given Entry.
   *
   * @param {Entry} entry An Entry subclass (FileEntry or DirectoryEntry).
   * @return {string} One of the values from FileManager.fileTypes, 'FOLDER',
   *                  or undefined.
   */
  FileManager.prototype.getFileType = function(entry) {
    if (!('cachedFileType_' in entry))
      entry.cachedFileType_ = this.computeFileType_(entry);
    return entry.cachedFileType_;
  };

  FileManager.prototype.computeFileType_ = function(entry) {
    if (entry.isDirectory) {
      var deviceNumber = this.getDeviceNumber(entry);
      // The type field is used for sorting. Starting dot maked devices and
      // directories to precede files.
      if (deviceNumber != undefined)
        return {name: 'DEVICE', type: '.device'};
      return {name: 'FOLDER', type: '.folder'};
    }

    var extension = getFileExtension(entry.name);
    if (extension in fileTypes)
      return fileTypes[extension];

    return {};
  };


  /**
   * Get the icon type of a file, caching the result.
   *
   * When this method completes, the entry object will get a
   * 'cachedIconType_' property (if it doesn't already have one) containing the
   * icon type of the file as a string.
   *
   * @param {Entry} entry An HTML5 Entry object.
   * @param {function(Entry)} successCallback The function to invoke once the
   *     icon type is known.
   */
  FileManager.prototype.cacheEntryIconType = function(entry, successCallback) {
    this.getIconType(entry);
    if (successCallback)
      setTimeout(function() { successCallback(entry) }, 0);
  };

  FileManager.prototype.onDataModelSplice_ = function(event) {
    var checkbox = this.document_.querySelector('#select-all-checkbox');
    if (checkbox)
      this.updateSelectAllCheckboxState_(checkbox);
  };

  /**
   * Get the file type of the entry, caching the result.
   *
   * When this method completes, the entry object will get a
   * 'cachedIconType_' property (if it doesn't already have one) containing the
   * icon type of the file as a string.
   *
   * @param {Entry} entry An HTML5 Entry object.
   * @param {function(Entry)} successCallback The function to invoke once the
   *     file size is known.
   * @param {boolean=} opt_sync If true, we are safe to do synchronous callback.
   */
  FileManager.prototype.cacheEntryFileType = function(
      entry, successCallback, opt_sync) {
    this.getFileType(entry);
    invokeCallback(successCallback, !!opt_sync, entry);
  };

  /**
   * Compare by mtime first, then by name.
   */
  FileManager.prototype.compareMtime_ = function(a, b) {
    if (a.cachedMtime_ > b.cachedMtime_)
      return 1;

    if (a.cachedMtime_ < b.cachedMtime_)
      return -1;

    return this.collator_.compare(a.name, b.name);
  };

  /**
   * Compare by size first, then by name.
   */
  FileManager.prototype.compareSize_ = function(a, b) {
    if (a.cachedSize_ > b.cachedSize_)
      return 1;

    if (a.cachedSize_ < b.cachedSize_)
      return -1;

    return this.collator_.compare(a.name, b.name);
  };

  /**
   * Compare by type first, then by subtype and then by name.
   */
  FileManager.prototype.compareType_ = function(a, b) {
    // Files of unknows type follows all the others.
    var result = this.collator_.compare(a.cachedFileType_.type || 'Z',
                                        b.cachedFileType_.type || 'Z');
    if (result != 0)
      return result;

    // If types are same both subtypes are defined of both are undefined.
    result = this.collator_.compare(a.cachedFileType_.subtype || '',
                                    b.cachedFileType_.subtype || '');
    if (result != 0)
      return result;

    return this.collator_.compare(a.name, b.name);
  };

  FileManager.prototype.refocus = function() {
    if (this.dialogType_ == FileManager.DialogType.SELECT_SAVEAS_FILE)
      this.filenameInput_.focus();
    else
      this.currentList_.focus();
  };

  FileManager.prototype.showButter = function(message, opt_options) {
    var butter = this.document_.createElement('div');
    butter.className = 'butter-bar';
    butter.style.top = '-30px';
    this.dialogDom_.appendChild(butter);

    var self = this;

    setTimeout(function () {
      if (self.currentButter_)
        self.hideButter();

      self.currentButter_ = butter;
      self.currentButter_.style.top = '15px';

      self.updateButter(message, opt_options);
    });

    return butter;
  };

  FileManager.prototype.showButterError = function(message, opt_options) {
    var butter = this.showButter(message, opt_options);
    butter.classList.add('butter-error');
    return butter;
  };

  FileManager.prototype.updateButter = function(message, opt_options) {
    if (!opt_options)
      opt_options = {};

    var timeout;
    if ('timeout' in opt_options) {
      timeout = opt_options.timeout;
    } else {
      timeout = 10 * 1000;
    }

    if (this.butterTimer_)
      clearTimeout(this.butterTimer_);

    if (timeout) {
      var self = this;
      this.butterTimer_ = setTimeout(function() {
          self.hideButter();
          self.butterTimer_ == null;
      }, timeout);
    }

    var butter = this.currentButter_;
    butter.textContent = message;

    if ('actions' in opt_options) {
      for (var label in opt_options.actions) {
        var link = this.document_.createElement('a');
        link.textContent = label;
        link.setAttribute('href', 'javascript://' + label);
        link.addEventListener('click', function () {
            opt_options.actions[label]();
            return false;
        });
        butter.appendChild(link);
      }
    }

    butter.style.left = ((this.dialogDom_.clientWidth -
                          butter.clientWidth) / 2) + 'px';
  };

  FileManager.prototype.hideButter = function() {
    if (this.currentButter_) {
      this.currentButter_.style.top = '50px';
      this.currentButter_.style.opacity = '0';

      var butter = this.currentButter_;
      setTimeout(function() {
          butter.parentNode.removeChild(butter);
      }, 1000);

      this.currentButter_ = null;
    }
  };

  /**
   * "Save a file" dialog is supposed to have a combo box with available
   * file types. Selecting an item filters files by extension and specifies how
   * file should be saved.
   * @return {intener} Index of selected type from this.fileTypes_ + 1. 0
   *                   means value is not specified.
   */
  FileManager.prototype.getSelectedFilterIndex_= function(fileName) {
    // TODO(serya): Implement the combo box
    // For now try to guess choice by file extension.
    if (!this.fileTypes_ || this.fileTypes_.length == 0) return 0;

    var extension = /\.[^\.]+$/.exec(fileName);
    extension = extension ? extension[0].substring(1).toLowerCase() : "";
    var result = 0;  // Use first type by default.
    for (var i = 0; i < this.fileTypes_.length; i++) {
      if (this.fileTypes_[i].extensions.indexOf(extension)) {
        result = i;
      }
    }
    return result + 1;  // 1-based index.
  };

  /**
   * Force the canExecute events to be dispatched.
   */
  FileManager.prototype.updateCommands_ = function() {
    for (var key in this.commands_) {
      this.commands_[key].canExecuteChange();
    }
  };

  /**
   * Invoked to decide whether the "copy" command can be executed.
   */
  FileManager.prototype.onCanExecute_ = function(event) {
    event.canExecute = this.canExecute_(event.command.id);
  };

  /**
   * @param {string} commandId Command identifier.
   * @return {boolean} True if the command can be executed for current
   *                   selection.
   */
  FileManager.prototype.canExecute_ = function(commandId) {
    var readonly = this.directoryModel_.readonly;
    var path = this.directoryModel_.currentEntry.fullPath;
    switch (commandId) {
      case 'cut':
        return !readonly;

      case 'copy':
        return path != '/';

      case 'paste':
        return (this.clipboard_ &&
               !readonly);

      case 'rename':
        return (// Initialized to the point where we have a current directory
                !readonly &&
                // Rename not in progress.
                !this.isRenamingInProgress() &&
                // Only one file selected.
                this.selection &&
                this.selection.totalCount == 1);

      case 'delete':
        return (// Initialized to the point where we have a current directory
                !readonly &&
                // Rename not in progress.
                !this.isRenamingInProgress() &&
                this.selection &&
                this.selection.totalCount > 0);

      case 'newfolder':
        return !readonly &&
               (this.dialogType_ == FileManager.DialogType.SELECT_SAVEAS_FILE ||
                this.dialogType_ == FileManager.DialogType.FULL_PAGE);
    }
  };

  FileManager.prototype.updateCommonActionButtons_ = function() {
    if (this.deleteButton_)
      this.deleteButton_.disabled = !this.canExecute_('delete');
  };

  FileManager.prototype.setListType = function(type) {
    if (type && type == this.listType_)
      return;

    if (FileManager.DialogType.isModal(this.dialogType_))
      window.localStorage['listType-' + this.dialogType_] = type;

    this.table_.list.startBatchUpdates();
    this.grid_.startBatchUpdates();

    // TODO(dzvorygin): style.display and dataModel setting order shouldn't
    // cause any UI bugs. Currently, the only right way is first to set display
    // style and only then set dataModel.

    if (type == FileManager.ListType.DETAIL) {
      this.table_.dataModel = this.directoryModel_.fileList;
      this.table_.selectionModel = this.directoryModel_.fileListSelection;
      this.table_.style.display = '';
      this.grid_.style.display = 'none';
      this.grid_.selectionModel = this.emptySelectionModel_;
      this.grid_.dataModel = this.emptyDataModel_;
      this.table_.style.display = '';
      /** @type {cr.ui.List} */
      this.currentList_ = this.table_.list;
      this.dialogDom_.querySelector('button.detail-view').disabled = true;
      this.dialogDom_.querySelector('button.thumbnail-view').disabled = false;
    } else if (type == FileManager.ListType.THUMBNAIL) {
      this.grid_.dataModel = this.directoryModel_.fileList;
      this.grid_.selectionModel = this.directoryModel_.fileListSelection;
      this.grid_.style.display = '';
      this.table_.style.display = 'none';
      this.table_.selectionModel = this.emptySelectionModel_;
      this.table_.dataModel = this.emptyDataModel_;
      this.grid_.style.display = '';
      /** @type {cr.ui.List} */
      this.currentList_ = this.grid_;
      this.dialogDom_.querySelector('button.thumbnail-view').disabled = true;
      this.dialogDom_.querySelector('button.detail-view').disabled = false;
    } else {
      throw new Error('Unknown list type: ' + type);
    }

    this.listType_ = type;
    this.onResize_();

    this.table_.list.endBatchUpdates();
    this.grid_.endBatchUpdates();
  };

  /**
   * Initialize the file thumbnail grid.
   */
  FileManager.prototype.initGrid_ = function() {
    var self = this;
    this.grid_.itemConstructor = GridItem.bind(null, this);

    this.grid_.addEventListener(
        'dblclick', this.onDetailDoubleClick_.bind(this));
    cr.ui.contextMenuHandler.addContextMenuProperty(this.grid_);
    this.grid_.contextMenu = this.fileContextMenu_;
    this.grid_.addEventListener('mousedown',
                                this.onGridOrTableMouseDown_.bind(this));
  };

  /**
   * Initialize the file list table.
   */
  FileManager.prototype.initTable_ = function() {
    var columns = [
        new cr.ui.table.TableColumn('name', str('NAME_COLUMN_LABEL'),
                                    64),
        new cr.ui.table.TableColumn('cachedSize_',
                                    str('SIZE_COLUMN_LABEL'), 15.5),
        new cr.ui.table.TableColumn('type',
                                    str('TYPE_COLUMN_LABEL'), 15.5),
        new cr.ui.table.TableColumn('cachedMtime_',
                                    str('DATE_COLUMN_LABEL'), 21)
    ];

    columns[0].renderFunction = this.renderName_.bind(this);
    columns[1].renderFunction = this.renderSize_.bind(this);
    columns[2].renderFunction = this.renderType_.bind(this);
    columns[3].renderFunction = this.renderDate_.bind(this);

    if (this.showCheckboxes_) {
      columns[0].headerRenderFunction =
          this.renderNameColumnHeader_.bind(this, columns[0].name);
    }

    this.table_.columnModel = new cr.ui.table.TableColumnModel(columns);

    this.table_.addEventListener(
        'dblclick', this.onDetailDoubleClick_.bind(this));

    this.table_.columnModel = new cr.ui.table.TableColumnModel(columns);

    cr.ui.contextMenuHandler.addContextMenuProperty(this.table_);
    this.table_.contextMenu = this.fileContextMenu_;

    this.table_.addEventListener('mousedown',
                                 this.onGridOrTableMouseDown_.bind(this));
  };

  FileManager.prototype.onCopyProgress_ = function(event) {
    var status = this.copyManager_.getStatus();

    if (event.reason == 'PROGRESS') {
      if (status.totalItems > 1 && status.completedItems < status.totalItems) {
        // If we're copying more than one file, and we're not done, update
        // the user on the current status, and give an option to cancel.
        var self = this;
        var options = {timeout:0, actions:{}};
        options.actions[str('CANCEL_LABEL')] = function cancelPaste() {
          self.copyManager_.requestCancel();
        };
        this.updateButter(strf('PASTE_SOME_PROGRESS', status.completedItems + 1,
                               status.totalItems), options);
      }
      return;
    }

    if (event.reason == 'SUCCESS') {
      this.showButter(str('PASTE_COMPLETE'));
      if (this.clipboard_.isCut)
        this.clipboard_ = null;
      this.updateCommands_();
      self = this;
      this.directoryModel_.rescan(function() {
        var callback;
        while (callback = self.pasteSuccessCallbacks_.shift()) {
          try {
            callback();
          } catch (ex) {
            console.error('Caught exception while inovking callback: ' +
                          callback, ex);
          }
        }
      });

    } else if (event.reason == 'ERROR') {
      switch (event.error.reason) {
        case 'TARGET_EXISTS':
          var name = event.error.data.name;
          if (event.error.data.isDirectory)
            name += '/';
          this.showButterError(strf('PASTE_TARGET_EXISTS_ERROR', name));
          break;

        case 'FILESYSTEM_ERROR':
          this.showButterError(
              strf('PASTE_FILESYSTEM_ERROR',
                   util.getFileErrorMnemonic(event.error.data.code)));
          break;

        default:
          this.showButterError(strf('PASTE_UNEXPECTED_ERROR', event.error));
          break;
      }
      this.directoryModel_.rescan();
    } else if (event.reason == 'CANCELLED') {
      this.showButter(str('PASTE_CANCELLED'));
      this.directoryModel_.rescan();
    } else {
      console.log('Unknown event reason: ' + event.reason);
      this.directoryModel_.rescan();
    }

  };
  /**
   * Respond to a command being executed.
   */
  FileManager.prototype.onCommand_ = function(event) {
    switch (event.command.id) {
      case 'cut':
        this.cutSelectionToClipboard();
        return;

      case 'copy':
        this.copySelectionToClipboard();
        return;

      case 'paste':
        this.pasteFromClipboard();
        return;

      case 'rename':
        var index = this.currentList_.selectionModel.selectedIndex;
        var item = this.currentList_.getListItemByIndex(index);
        if (item)
          this.initiateRename_(item);
        return;

      case 'delete':
        this.deleteEntries(this.selection.entries);
        return;

      case 'newfolder':
        this.onNewFolderCommand_(event);
        return;
    }
  };

  /**
   * Respond to the back and forward buttons.
   */
  FileManager.prototype.onPopState_ = function(event) {
    // TODO(serya): We should restore selected items here.
    this.setupCurrentDirectory_();
  };

  FileManager.prototype.requestResize_ = function(timeout) {
    setTimeout(this.onResize_.bind(this), timeout || 0);
  };

  /**
   * Resize details and thumb views to fit the new window size.
   */
  FileManager.prototype.onResize_ = function() {
    this.table_.style.height = this.grid_.style.height =
      this.grid_.parentNode.clientHeight + 'px';
    this.table_.list_.style.height = (this.table_.clientHeight - 1 -
                                      this.table_.header_.clientHeight) + 'px';

    if (this.listType_ == FileManager.ListType.THUMBNAIL) {
      var g = this.grid_;
      g.startBatchUpdates();
      setTimeout(function() {
        g.columns = 0;
        g.redraw();
        g.endBatchUpdates();
      }, 0);
    } else {
      this.table_.redraw();
    }

    this.rootsList_.style.height =
        this.rootsList_.parentNode.clientHeight + 'px';
    this.rootsList_.redraw();
    this.truncateBreadcrumbs_();
  };

  FileManager.prototype.resolvePath = function(
      path, resultCallback, errorCallback) {
    return util.resolvePath(this.filesystem_.root, path, resultCallback,
                            errorCallback);
  };

  /**
   * Restores current directory and may be a selected item after page load (or
   * reload) or popping a state (after click on back/forward). If location.hash
   * is present it means that the user has navigated somewhere and that place
   * will be restored. defaultPath primarily is used with save/open dialogs.
   * Default path may also contain a file name. Freshly opened file manager
   * window has neither.
   */
  FileManager.prototype.setupCurrentDirectory_ = function() {
    if (location.hash) {
      // Location hash has the highest priority.
      var path = decodeURI(location.hash.substr(1));
      this.directoryModel_.setupPath(path);
      return;
    } else if (this.params_.defaultPath) {
      var pathResolvedCallback;
      if (this.dialogType_ == FileManager.DialogType.SELECT_SAVEAS_FILE) {
        pathResolvedCallback = function(basePath, leafName) {
          this.filenameInput_.value = leafName;
          this.selectDefaultPathInFilenameInput_();
        }.bind(this);
      }
      this.directoryModel_.setupPath(this.params_.defaultPath,
                                     pathResolvedCallback);
    } else {
      this.directoryModel_.setupDefaultPath();
    }
  };

  /**
   * Tweak the UI to become a particular kind of dialog, as determined by the
   * dialog type parameter passed to the constructor.
   */
  FileManager.prototype.initDialogType_ = function() {
    var defaultTitle;
    var okLabel = str('OPEN_LABEL');

    switch (this.dialogType_) {
      case FileManager.DialogType.SELECT_FOLDER:
        defaultTitle = str('SELECT_FOLDER_TITLE');
        break;

      case FileManager.DialogType.SELECT_OPEN_FILE:
        defaultTitle = str('SELECT_OPEN_FILE_TITLE');
        break;

      case FileManager.DialogType.SELECT_OPEN_MULTI_FILE:
        defaultTitle = str('SELECT_OPEN_MULTI_FILE_TITLE');
        break;

      case FileManager.DialogType.SELECT_SAVEAS_FILE:
        defaultTitle = str('SELECT_SAVEAS_FILE_TITLE');
        okLabel = str('SAVE_LABEL');
        break;

      case FileManager.DialogType.FULL_PAGE:
        break;

      default:
        throw new Error('Unknown dialog type: ' + this.dialogType_);
    }

    this.okButton_.textContent = okLabel;

    dialogTitle = this.params_.title || defaultTitle;
    this.dialogDom_.querySelector('.dialog-title').textContent = dialogTitle;
  };

  /**
   * Render (and wire up) a checkbox to be used in either a detail or a
   * thumbnail list item.
   */
  FileManager.prototype.renderCheckbox_ = function(entry) {
    var input = this.document_.createElement('input');
    input.setAttribute('type', 'checkbox');
    input.setAttribute('tabindex', -1);
    input.className = 'file-checkbox';
    input.addEventListener('mousedown',
                           this.onCheckboxMouseDownUp_.bind(this));
    input.addEventListener('mouseup',
                           this.onCheckboxMouseDownUp_.bind(this));
    input.addEventListener('click',
                           this.onCheckboxClick_.bind(this));

    if (this.selection && this.selection.entries.indexOf(entry) != -1) {
      // Our DOM nodes get discarded as soon as we're scrolled out of view,
      // so we have to make sure the check state is correct when we're brought
      // back to life.
      input.checked = true;
    }

    return input;
  };

  FileManager.prototype.renderNameColumnHeader_ = function(name, table) {
    var input = this.document_.createElement('input');
    input.setAttribute('type', 'checkbox');
    input.setAttribute('tabindex', -1);
    input.id = 'select-all-checkbox';
    this.updateSelectAllCheckboxState_(input);

    input.addEventListener('click', function(event) {
      if (input.checked)
        table.selectionModel.selectAll();
      else
        table.selectionModel.unselectAll();
      event.preventDefault();
      event.stopPropagation();
    });

    var fragment = this.document_.createDocumentFragment();
    fragment.appendChild(input);
    fragment.appendChild(this.document_.createTextNode(name));
    return fragment;
  };

  /**
   * Update check and disable states of the 'Select all' checkbox.
   */
  FileManager.prototype.updateSelectAllCheckboxState_ = function(checkbox) {
    var dm = this.directoryModel_.fileList;
    checkbox.checked = this.selection && dm.length > 0 &&
                       dm.length == this.selection.totalCount;
    checkbox.disabled = dm.length == 0;
  };

  /**
   * Update the thumbnail image to fit/fill the square container.
   *
   * Using webkit center packing does not align the image properly, so we need
   * to wait until the image loads and its proportions are known, then manually
   * position it at the center.
   *
   * @param {CSSStyleDeclaration} style Style object of the image.
   * @param {number} width Width of the image.
   * @param {number} height Height of the image.
   * @param {boolean} fill True: the image should fill the entire container,
   *                       false: the image should fully fit into the container.
   */
  FileManager.prototype.centerImage_ = function(style, width, height, fill) {
    function percent(fraction) {
      return Math.round(fraction * 100 * 10) / 10 + '%';  // Round to 0.1%
    }

    // First try vertical fit or horizontal fill.
    var fractionX = width / height;
    var fractionY = 1;
    if ((fractionX < 1) == !!fill) {  // Vertical fill or horizontal fit.
      fractionY = 1 / fractionX;
      fractionX = 1;
    }

    style.width = percent(fractionX);
    style.height = percent(fractionY);
    style.left = percent((1 - fractionX) / 2);
    style.top = percent((1 - fractionY) / 2);
  };

  FileManager.prototype.applyImageTransformation_ = function(box, transform) {
    if (transform) {
      box.style.webkitTransform =
          'scaleX(' + transform.scaleX + ') ' +
          'scaleY(' + transform.scaleY + ') ' +
          'rotate(' + transform.rotate90 * 90 + 'deg)';
    }
  };

  /**
   * Create a box containing a centered thumbnail image.
   *
   * @param {Entry} entry
   * @param {boolean} True if fill, false if fit.
   * @param {function(HTMLElement)} opt_imageLoadCallback Callback called when
   *                                the image has been loaded before inserting
   *                                it into the DOM.
   * @return {HTMLDivElement}
   */
  FileManager.prototype.renderThumbnailBox_ = function(entry, fill,
                                                       opt_imageLoadCallback) {
    var box = this.document_.createElement('div');
    box.className = 'img-container';
    var img = this.document_.createElement('img');
    var self = this;
    this.getThumbnailURL(entry, function(iconType, url, transform) {
      img.onload = function() {
        self.centerImage_(img.style, img.width, img.height, fill);
        if (opt_imageLoadCallback)
          opt_imageLoadCallback(img, transform);
        box.appendChild(img);
      };
      img.src = url;
      self.applyImageTransformation_(box, transform);
    });
    return box;
  };

  FileManager.prototype.decorateThumbnail_ = function(li, entry) {
    li.className = 'thumbnail-item';

    if (this.showCheckboxes_)
      li.appendChild(this.renderCheckbox_(entry));

    li.appendChild(this.renderThumbnailBox_(entry, false));
    li.appendChild(this.renderFileNameLabel_(entry));
  };

  /**
   * Render the type column of the detail table.
   *
   * Invoked by cr.ui.Table when a file needs to be rendered.
   *
   * @param {Entry} entry The Entry object to render.
   * @param {string} columnId The id of the column to be rendered.
   * @param {cr.ui.Table} table The table doing the rendering.
   */
  FileManager.prototype.renderIconType_ = function(entry, columnId, table) {
    var icon = this.document_.createElement('div');
    icon.className = 'detail-icon';
    this.getIconType(entry);
    icon.setAttribute('iconType', entry.cachedIconType_);
    return icon;
  };

  /**
   * Return the localized name for the root.
   * @param {string} path The full path of the root (starting with slash).
   * @return {string} The localized name.
   */
  FileManager.prototype.getRootLabel_ = function(path) {
    if (path == '/' + DirectoryModel.DOWNLOADS_DIRECTORY)
      return str('CHROMEBOOK_DIRECTORY_LABEL');

    if (path == '/' + DirectoryModel.ARCHIVE_DIRECTORY)
      return str('ARCHIVE_DIRECTORY_LABEL');
    if (isParentPath('/' + DirectoryModel.ARCHIVE_DIRECTORY, path))
      return path.substring(DirectoryModel.ARCHIVE_DIRECTORY.length + 2);

    if (path == '/' + DirectoryModel.REMOVABLE_DIRECTORY)
      return str('REMOVABLE_DIRECTORY_LABEL');
    if (isParentPath('/' + DirectoryModel.REMOVABLE_DIRECTORY, path))
      return path.substring(DirectoryModel.REMOVABLE_DIRECTORY.length + 2);

    return path;
  };

  FileManager.prototype.renderRoot_ = function(entry) {
    var li = this.document_.createElement('li');
    li.className = 'root-item';

    var rootType = DirectoryModel.getRootType(entry.fullPath);

    var div = this.document_.createElement('div');
    div.className = 'root-label';
    div.setAttribute('icon', rootType);
    div.textContent = this.getRootLabel_(entry.fullPath);
    li.appendChild(div);

    if (rootType == DirectoryModel.RootType.ARCHIVE ||
        rootType == DirectoryModel.RootType.REMOVABLE) {
      var spacer = this.document_.createElement('div');
      spacer.className = 'spacer';
      li.appendChild(spacer);

      var eject = this.document_.createElement('img');
      eject.className = 'root-eject';
      // TODO(serya): Move to CSS.
      eject.setAttribute('src', chrome.extension.getURL('images/eject.png'));
      eject.addEventListener('click', this.onEjectClick_.bind(this, entry));
      li.appendChild(eject);
    }

    cr.defineProperty(li, 'lead', cr.PropertyKind.BOOL_ATTR);
    cr.defineProperty(li, 'selected', cr.PropertyKind.BOOL_ATTR);
    return li;
  };

  /**
   * Handler for eject button clicked.
   * @param {Entry} entry Entry to eject.
   * @param {Event} event The event.
   */
  FileManager.prototype.onEjectClick_ = function(entry, event) {
    this.unmountRequests_.push(entry.toURL());
    chrome.fileBrowserPrivate.removeMount(entry.fullPath);
  };

  /**
   * Render the Name column of the detail table.
   *
   * Invoked by cr.ui.Table when a file needs to be rendered.
   *
   * @param {Entry} entry The Entry object to render.
   * @param {string} columnId The id of the column to be rendered.
   * @param {cr.ui.Table} table The table doing the rendering.
   */
  FileManager.prototype.renderName_ = function(entry, columnId, table) {
    var label = this.document_.createElement('div');
    if (this.showCheckboxes_)
      label.appendChild(this.renderCheckbox_(entry));
    label.appendChild(this.renderIconType_(entry, columnId, table));
    label.entry = entry;
    label.className = 'detail-name';
    label.appendChild(this.renderFileNameLabel_(entry));
    return label;
  };

  /**
   * Render filename label for grid and list view.
   * @param {Entry} entry The Entry object to render.
   * @return {HTMLDivElement} The label.
   */
  FileManager.prototype.renderFileNameLabel_ = function(entry) {
    // Filename need to be in a '.filename-label' container for correct
    // work of inplace renaming.
    var fileName = this.document_.createElement('div');
    fileName.className = 'filename-label';

    fileName.textContent = this.directoryModel_.currentEntry.name == '' ?
        this.getRootLabel_(entry.name) : entry.name;
    return fileName;
  };

  /**
   * Render the Size column of the detail table.
   *
   * @param {Entry} entry The Entry object to render.
   * @param {string} columnId The id of the column to be rendered.
   * @param {cr.ui.Table} table The table doing the rendering.
   */
  FileManager.prototype.renderSize_ = function(entry, columnId, table) {
    var div = this.document_.createElement('div');

    div.textContent = '...';
    cacheEntrySize(entry, function(entry) {
      if (entry.cachedSize_ == -1) {
        div.textContent = '';
      } else {
        div.textContent = util.bytesToSi(entry.cachedSize_);
      }
    }, null, true);

    return div;
  };

  /**
   * Render the Type column of the detail table.
   *
   * @param {Entry} entry The Entry object to render.
   * @param {string} columnId The id of the column to be rendered.
   * @param {cr.ui.Table} table The table doing the rendering.
   */
  FileManager.prototype.renderType_ = function(entry, columnId, table) {
    var div = this.document_.createElement('div');

    this.cacheEntryFileType(entry, function(entry) {
      var info = entry.cachedFileType_;
      if (info.name) {
        if (info.subtype)
          div.textContent = strf(info.name, info.subtype);
        else
          div.textContent = str(info.name);
      } else
        div.textContent = '';
    }, true);

    return div;
  };

  /**
   * Render the Date column of the detail table.
   *
   * @param {Entry} entry The Entry object to render.
   * @param {string} columnId The id of the column to be rendered.
   * @param {cr.ui.Table} table The table doing the rendering.
   */
  FileManager.prototype.renderDate_ = function(entry, columnId, table) {
    var div = this.document_.createElement('div');

    div.textContent = '...';

    var self = this;
    cacheEntryDate(entry, function(entry) {
      if (self.directoryModel_.isSystemDirectoy &&
          entry.cachedMtime_.getTime() == 0) {
        // Mount points for FAT volumes have this time associated with them.
        // We'd rather display nothing than this bogus date.
        div.textContent = '';
      } else {
        div.textContent = self.shortDateFormatter_.format(entry.cachedMtime_);
      }
    }, null, true);

    return div;
  };

  /**
   * Compute summary information about the current selection.
   *
   * This method dispatches the 'selection-summarized' event when it completes.
   * Depending on how many of the selected files already have known sizes, the
   * dispatch may happen immediately, or after a number of async calls complete.
   */
  FileManager.prototype.summarizeSelection_ = function() {
    var selection = this.selection = {
      entries: [],
      urls: [],
      totalCount: 0,
      fileCount: 0,
      directoryCount: 0,
      bytes: 0,
      iconType: null,
      indexes: this.currentList_.selectionModel.selectedIndexes
    };

    this.previewSummary_.textContent = str('COMPUTING_SELECTION');
    removeChildren(this.taskButtons_);
    removeChildren(this.previewThumbnails_);

    if (!selection.indexes.length) {
      this.updateCommonActionButtons_();
      this.updatePreviewPanelVisibility_();
      cr.dispatchSimpleEvent(this, 'selection-summarized');
      return;
    }

    var fileCount = 0;
    var byteCount = 0;
    var pendingFiles = [];
    var thumbnailCount = 0;

    for (var i = 0; i < selection.indexes.length; i++) {
      var entry = this.directoryModel_.fileList.item(selection.indexes[i]);
      if (!entry)
        continue;

      selection.entries.push(entry);
      selection.urls.push(entry.toURL());

      if (selection.iconType == null) {
        selection.iconType = this.getIconType(entry);
      } else if (selection.iconType != 'unknown') {
        var iconType = this.getIconType(entry);
        if (selection.iconType != iconType)
          selection.iconType = 'unknown';
      }

      if (thumbnailCount < MAX_PREVIEW_THUMBAIL_COUNT && entry.isFile) {
        var box = this.document_.createElement('div');
        var imageLoadCalback = thumbnailCount == 0 &&
                               this.initThumbnailZoom_.bind(this, box);
        var thumbnail = this.renderThumbnailBox_(entry, true, imageLoadCalback);
        box.appendChild(thumbnail);
        box.style.zIndex = MAX_PREVIEW_THUMBAIL_COUNT + 1 - i;

        this.previewThumbnails_.appendChild(box);
        thumbnailCount++;
      }

      selection.totalCount++;

      if (entry.isFile) {
        selection.fileCount += 1;
        if (!('cachedSize_' in entry)) {
          // Any file that hasn't been rendered may be missing its cachedSize_
          // property.  For example, visit a large file list, and press ctrl-a
          // to select all.  In this case, we need to asynchronously get the
          // sizes for these files before telling the world the selection has
          // been summarized.  See the 'computeNextFile' logic below.
          pendingFiles.push(entry);
          continue;
        } else {
          selection.bytes += entry.cachedSize_;
        }
      } else {
        selection.directoryCount += 1;
      }
    }

    // Now this.selection is complete. Update buttons.
    this.updateCommonActionButtons_();
    this.updatePreviewPanelVisibility_();

    var self = this;

    function cacheNextFile(fileEntry) {
      if (fileEntry) {
        // We're careful to modify the 'selection', rather than 'self.selection'
        // here, just in case the selection has changed since this summarization
        // began.
        selection.bytes += fileEntry.cachedSize_;
      }

      if (pendingFiles.length) {
        cacheEntrySize(pendingFiles.pop(), cacheNextFile);
      } else {
        self.dispatchEvent(new cr.Event('selection-summarized'));
      }
    }

    if (this.dialogType_ == FileManager.DialogType.FULL_PAGE) {
      removeChildren(this.taskButtons_);
      // Some internal tasks cannot be defined in terms of file patterns,
      // so we pass selection to check for them manually.
      if (selection.directoryCount == 0 && selection.fileCount > 0) {
        // Only files, not directories, are supported for external tasks.
        chrome.fileBrowserPrivate.getFileTasks(
            selection.urls,
            this.onTasksFound_.bind(this, selection));
      } else {
        // There may be internal tasks for directories.
        this.onTasksFound_(selection, []);
      }
    }

    cacheNextFile();
  };

  /**
   * Initialize a thumbnail in the bottom pannel to pop up on mouse over.
   * Image's assumed to be just loaded and not inserted into the DOM.
   *
   * @param {HTMLElement} box Element what's going to contain the image.
   * @param {HTMLElement} img Loaded image.
   */
  FileManager.prototype.initThumbnailZoom_ = function(box, img, transform) {
    var width = img.width;
    var height = img.height;
    const THUMBNAIL_SIZE = 45;

    if (width < THUMBNAIL_SIZE * 2 && height < THUMBNAIL_SIZE * 2)
      return;

    var scale = Math.min(1,
        IMAGE_HOVER_PREVIEW_SIZE / Math.max(width, height));

    var largeImage = this.document_.createElement('img');
    largeImage.src = img.src;
    var largeImageBox = this.document_.createElement('div');
    largeImageBox.className = 'popup';

    var imageWidth = Math.round(width * scale);
    var imageHeight = Math.round(height * scale);

    var boxWidth = Math.max(THUMBNAIL_SIZE, imageWidth);
    var boxHeight = Math.max(THUMBNAIL_SIZE, imageHeight);

    if (transform && transform.rotate90 % 2 == 1) {
      var t = boxWidth;
      boxWidth = boxHeight;
      boxHeight = t;
    }

    var style = largeImageBox.style;
    style.width = boxWidth + 'px';
    style.height = boxHeight + 'px';
    style.top = (-boxHeight + THUMBNAIL_SIZE) + 'px';

    var style = largeImage.style;
    style.width = imageWidth + 'px';
    style.height = imageHeight + 'px';
    style.left = (boxWidth - imageWidth) / 2 + 'px';
    style.top = (boxHeight - imageHeight) / 2 + 'px';
    style.position = 'relative';

    this.applyImageTransformation_(largeImage, transform);

    largeImageBox.appendChild(largeImage);
    box.insertBefore(largeImageBox, box.firstChild);
  };

  FileManager.prototype.updatePreviewPanelVisibility_ = function() {
    var panel = this.previewPanel_;
    var state = panel.getAttribute('visibility');
    var mustBeVisible = (this.selection.totalCount > 0);
    var self = this;

    switch (state) {
      case 'visible':
        if (!mustBeVisible)
          startHiding();
        break;

      case 'hiding':
        if (mustBeVisible)
          stopHidingAndShow();
        break;

      case 'hidden':
        if (mustBeVisible)
          show();
    }

    function stopHidingAndShow() {
      clearTimeout(self.hidingTimeout_);
      self.hidingTimeout_ = 0;
      setVisibility('visible');
    }

    function startHiding() {
      setVisibility('hiding');
      self.hidingTimeout_ = setTimeout(function() {
          self.hidingTimeout_ = 0;
          setVisibility('hidden');
          self.onResize_();
        }, 250);
    }

    function show() {
      setVisibility('visible');
      self.onResize_();
    }

    function setVisibility(visibility) {
      panel.setAttribute('visibility', visibility);
    }
  };


  FileManager.prototype.createMetadataProvider_ = function() {
    // Subclass MetadataProvider to notify tests when the initialization
    // is complete.

    var fileManager = this;

    function TestAwareMetadataProvider () {
      MetadataProvider.apply(this, arguments);
    }

    TestAwareMetadataProvider.prototype = {
      __proto__: MetadataProvider.prototype,

      onInitialized_: function() {
        MetadataProvider.prototype.onInitialized_.apply(this, arguments);

        // We're ready to run.  Tests can monitor for this state with
        // ExtensionTestMessageListener listener("worker-initialized");
        // ASSERT_TRUE(listener.WaitUntilSatisfied());
        // Automated tests need to wait for this, otherwise we crash in
        // browser_test cleanup because the worker process still has
        // URL requests in-flight.
        chrome.test.sendMessage('worker-initialized');
        // PyAuto tests monitor this state by polling this variable
        fileManager.workerInitialized_ = true;
      }
    };

    this.metadataProvider_ = new TestAwareMetadataProvider();
  };

  /**
   * Callback called when tasks for selected files are determined.
   * @param {Object} selection Selection is passed here, since this.selection
   *     can change before tasks were found, and we should be accurate.
   * @param {Array.<Task>} tasksList The tasks list.
   */
  FileManager.prototype.onTasksFound_ = function(selection, tasksList) {
    removeChildren(this.taskButtons_);

    for (var i = 0; i < tasksList.length; i++) {
      var task = tasksList[i];

      // Tweak images, titles of internal tasks.
      var task_parts = task.taskId.split('|');
      if (task_parts[0] == this.getExtensionId_()) {
        task.internal = true;
        if (task_parts[1] == 'play') {
          // TODO(serya): This hack needed until task.iconUrl is working
          //             (see GetFileTasksFileBrowserFunction::RunImpl).
          task.iconUrl =
              chrome.extension.getURL('images/icon_play_16x16.png');
          task.title = str('PLAY_MEDIA').replace("&", "");
        } else if (task_parts[1] == 'enqueue') {
          task.iconUrl =
              chrome.extension.getURL('images/icon_add_to_queue_16x16.png');
          task.title = str('ENQUEUE');
        } else if (task_parts[1] == 'mount-archive') {
          task.iconUrl =
              chrome.extension.getURL('images/icon_mount_archive_16x16.png');
          task.title = str('MOUNT_ARCHIVE');
        } else if (task_parts[1] == 'gallery') {
          task.iconUrl =
              chrome.extension.getURL('images/icon_preview_16x16.png');
          task.title = str('GALLERY');
          task.allTasks = tasksList;
          this.galleryTask_ = task;
        }
      }
      this.renderTaskButton_(task);
    }

    // These are done in separate functions, as the checks require
    // asynchronous function calls.
    this.maybeRenderFormattingTask_(selection);
  };

  FileManager.prototype.renderTaskButton_ = function(task) {
    var button = this.document_.createElement('button');
    button.addEventListener('click',
        this.onTaskButtonClicked_.bind(this, task));
    button.className = 'task-button';

    var img = this.document_.createElement('img');
    img.src = task.iconUrl;

    button.appendChild(img);
    var label = this.document_.createElement('div');
    label.appendChild(this.document_.createTextNode(task.title))
    button.appendChild(label);

    this.taskButtons_.appendChild(button);
  };

  /**
   * Checks whether formatting task should be displayed and if the answer is
   * affirmative renders it. Includes asynchronous calls, so it's splitted into
   * three parts.
   * @param {Object} selection Selected files object.
   */
  FileManager.prototype.maybeRenderFormattingTask_ = function(selection) {
    // Not to make unnecessary getMountPoints() call we doublecheck if there is
    // only one selected entry.
    if (selection.entries.length != 1)
      return;
    var self = this;
    function onMountPointsFound(mountPoints) {
      self.mountPoints_ = mountPoints;
      function onVolumeMetadataFound(volumeMetadata) {
        if (volumeMetadata.deviceType == "flash") {
          if (self.selection.entries.length != 1 ||
              normalizeAbsolutePath(self.selection.entries[0].fullPath) !=
              normalizeAbsolutePath(volumeMetadata.mountPath)) {
            return;
          }
          var task = {
            taskId: self.getExtensionId_() + '|format-device',
            iconUrl: chrome.extension.getURL('images/filetype_generic.png'),
            title: str('FORMAT_DEVICE'),
            internal: true
          };
          self.renderTaskButton_(task);
        }
      }

      if (selection.entries.length != 1)
        return;
      var selectedPath = selection.entries[0].fullPath;
      for (var i = 0; i < mountPoints.length; i++) {
        if (mountPoints[i].mountType == "device" &&
            normalizeAbsolutePath(mountPoints[i].mountPath) ==
            normalizeAbsolutePath(selectedPath)) {
          chrome.fileBrowserPrivate.getVolumeMetadata(mountPoints[i].sourceUrl,
              onVolumeMetadataFound);
          return;
        }
      }
    }

    chrome.fileBrowserPrivate.getMountPoints(onMountPointsFound);
  };

  FileManager.prototype.getExtensionId_ = function() {
    return chrome.extension.getURL('').split('/')[2];
  };

  FileManager.prototype.onDownloadsWarningClick_ = function(event) {
    chrome.tabs.create({url: DOWNLOADS_FAQ_URL});
    if (this.dialogType_ != FileManager.DialogType.FULL_PAGE) {
      this.onCancel_();
    }
  };

  FileManager.prototype.onTaskButtonClicked_ = function(task, event) {
    this.dispatchFileTask_(task, this.selection.urls);
  };

  FileManager.prototype.dispatchFileTask_ = function(task, urls) {
    if (task.internal) {
      // For internal tasks call the handler directly to avoid being handled
      // multiple times.
      var taskId = task.taskId.split('|')[1];
      this.onFileTaskExecute_(taskId, {urls: urls, task: task});
      return;
    }
    chrome.fileBrowserPrivate.executeTask(task.taskId, urls);
  };

  /**
   * Event handler called when some volume was mounted or unmouted.
   */
  FileManager.prototype.onMountCompleted_ = function(event) {
    var self = this;
    chrome.fileBrowserPrivate.getMountPoints(function(mountPoints) {
      self.mountPoints_ = mountPoints;
      var changeDirectoryTo = null;

      if (event.eventType == 'mount') {
        // Mount request finished - remove it.
        var index = self.mountRequests_.indexOf(event.sourceUrl);
        if (index != -1) {
          self.mountRequests_.splice(index, 1);
          // Go to mounted directory, if request was initiated from this tab.
          if (event.status == 'success')
            changeDirectoryTo = event.mountPath;
        }
      }

      if (event.eventType == 'unmount') {
        // Unmount request finished - remove it.
        var index = self.unmountRequests_.indexOf(event.sourceUrl);
        if (index != -1)
          self.unmountRequests_.splice(index, 1);
      }

      if (event.eventType == 'mount' && event.status != 'success' &&
          event.mountType == 'file') {
        // Report mount error.
        var fileName = event.sourceUrl.substr(
            event.sourceUrl.lastIndexOf('/') + 1);
        self.alert.show(strf('ARCHIVE_MOUNT_FAILED', fileName,
                             event.status));
      }

      if (event.eventType == 'unmount' && event.status != 'success') {
        // Report unmount error.
        // TODO(dgozman): introduce string and show alert here.
      }

      if (event.eventType == 'unmount' && event.status == 'success' &&
          event.mountPath == self.directoryModel_.rootPath) {
        // Current durectory just unmounted. Move to the 'Downloads'.
        changeDirectoryTo = '/' + DirectoryModel.DOWNLOADS_DIRECTORY;
      }

      // In the case of success, roots are changed and should be rescanned.
      if (event.status == 'success')
        self.directoryModel_.updateRoots(changeDirectoryTo);
    });
  };

  /**
   * Event handler called when some internal task should be executed.
   */
  FileManager.prototype.onFileTaskExecute_ = function(id, details) {
    var urls = details.urls;
    if (id == 'play' || id == 'enqueue') {
      chrome.fileBrowserPrivate.viewFiles(urls, id);
    } else if (id == 'mount-archive') {
      for (var index = 0; index < urls.length; ++index) {
        // Url in MountCompleted event won't be escaped, so let's make sure
        // we don't use escaped one in mountRequests_.
        var unescapedUrl = unescape(urls[index]);
        this.mountRequests_.push(unescapedUrl);
        chrome.fileBrowserPrivate.addMount(unescapedUrl, 'file', {});
      }
    } else if (id == 'format-device') {
      this.confirm.show(str('FORMATTING_WARNING'), function() {
        chrome.fileBrowserPrivate.formatDevice(urls[0]);
      });
    } else if (id == 'gallery') {
      // Pass to gallery all possible tasks except the gallery itself.
      var noGallery = [];
      for (var index = 0; index < details.task.allTasks.length; index++) {
        var task = details.task.allTasks[index];
        if (task.taskId != this.getExtensionId_() + '|gallery') {
          // Add callback, so gallery can execute the task.
          task.execute = this.dispatchFileTask_.bind(this, task);
          noGallery.push(task);
        }
      }
      this.openGallery_(urls, noGallery);
    }
  };

  FileManager.prototype.getDeviceNumber = function(entry) {
    if (!entry.isDirectory) return undefined;
    for (var i = 0; i < this.mountPoints_.length; i++) {
      if (normalizeAbsolutePath(entry.fullPath) ==
          normalizeAbsolutePath(this.mountPoints_[i].mountPath)) {
        return i;
      }
    }
    return undefined;
  };

  FileManager.prototype.openGallery_ = function(urls, shareActions) {
    var self = this;

    var galleryFrame = this.document_.createElement('iframe');
    galleryFrame.className = 'overlay-pane';
    galleryFrame.scrolling = 'no';

    var selectedUrl;
    if (urls.length == 1) {
      // Single item selected. Pass to the Gallery as a selected.
      selectedUrl = urls[0];
      // Pass every image in the directory so that it shows up in the ribbon.
      urls = [];
      var dm = this.directoryModel_.fileList;
      for (var i = 0; i != dm.length; i++) {
        var entry = dm.item(i);
        if (this.getFileType(entry).type == 'image') {
          urls.push(entry.toURL());
        }
      }
    } else {
      // Multiple selection. Pass just those items, select the first entry.
      selectedUrl = urls[0];
    }

    galleryFrame.onload = function() {
      self.document_.title = str('GALLERY');
      galleryFrame.contentWindow.ImageUtil.metrics = metrics;
      galleryFrame.contentWindow.Gallery.open(
          self.directoryModel_.currentEntry,
          urls,
          selectedUrl,
          function () {
            // TODO(kaznacheev): keep selection.
            self.dialogDom_.removeChild(galleryFrame);
            self.document_.title = self.directoryModel_.currentEntry.fullPath;
            self.refocus();
          },
          self.metadataProvider_,
          shareActions,
          str);
    };

    galleryFrame.src = 'js/image_editor/gallery.html';
    this.dialogDom_.appendChild(galleryFrame);
    galleryFrame.focus();
  };

  /**
   * Update the breadcrumb display to reflect the current directory.
   */
  FileManager.prototype.updateBreadcrumbs_ = function() {
    var bc = this.dialogDom_.querySelector('.breadcrumbs');
    removeChildren(bc);

    var rootPath = this.directoryModel_.rootPath;
    var relativePath = this.directoryModel_.currentEntry.fullPath.
                       substring(rootPath.length).replace(/\/$/, '');

    var pathNames = relativePath.replace(/\/$/, '').split('/');
    if (pathNames[0] == '')
      pathNames.splice(0, 1);

    // We need a first breadcrumb for root, so placing last name from
    // rootPath as first name of relativePath.
    var rootPathNames = rootPath.replace(/\/$/, '').split('/');
    pathNames.splice(0, 0, rootPathNames[rootPathNames.length - 1]);
    rootPathNames.splice(rootPathNames.length - 1, 1);
    var path = rootPathNames.join('/') + '/';
    var doc = this.document_;

    for (var i = 0; i < pathNames.length; i++) {
      var pathName = pathNames[i];
      path += pathName;

      var div = doc.createElement('div');

      div.className = 'breadcrumb-path';
      if (i == 0) {
        // Need to say "File shelf" instead of "Chromebook" in the breadcrumb
        // after select Chromebook in the left panel.
        div.textContent = pathName == DirectoryModel.DOWNLOADS_DIRECTORY ?
            str('CHROMEBOOK_DIRECTORY_BREADCRUMB_LABEL') :
            this.getRootLabel_(path);
      } else {
        div.textContent = pathName;
      }

      path = path + '/';
      div.path = path;
      div.addEventListener('click', this.onBreadcrumbClick_.bind(this));

      if (i == 0) {
        div.classList.add('root-label');
        div.setAttribute('icon', DirectoryModel.getRootType(rootPath));

        // Wrapper with zero margins.
        var wrapper = doc.createElement('div');
        wrapper.appendChild(div);
        bc.appendChild(wrapper);
      } else {
        bc.appendChild(div);
      }

      if (i == pathNames.length - 1) {
        div.classList.add('breadcrumb-last');
      } else {
        var spacer = doc.createElement('div');
        spacer.className = 'separator';
        bc.appendChild(spacer);
      }
    }
    this.truncateBreadcrumbs_();
  };

  FileManager.prototype.isRenamingInProgress = function() {
    return !!this.renameInput_.currentEntry;
  };

  /**
   * Updates breadcrumbs widths in order to truncate it properly.
   */
  FileManager.prototype.truncateBreadcrumbs_ = function() {
    var bc = this.dialogDom_.querySelector('.breadcrumbs');
    if (!bc.firstChild)
     return;

    // Assume style.width == clientWidth (items have no margins or paddings).

    for (var item = bc.firstChild; item; item = item.nextSibling) {
      item.removeAttribute('style');
      item.removeAttribute('collapsed');
    }

    var containerWidth = bc.clientWidth;

    var pathWidth = 0;
    var currentWidth = 0;
    var lastSeparator;
    for (var item = bc.firstChild; item; item = item.nextSibling) {
      if (item.className == 'separator') {
        pathWidth += currentWidth;
        currentWidth = item.clientWidth;
        lastSeparator = item;
      } else {
        currentWidth += item.clientWidth;
      }
    }
    if (pathWidth + currentWidth <= containerWidth)
      return;
    if (!lastSeparator) {
      bc.lastChild.style.width = Math.min(currentWidth, containerWidth) + 'px';
      return;
    }
    var lastCrumbSeparatorWidth = lastSeparator.clientWidth;
    // Current directory name may occupy up to 70% of space or even more if the
    // path is short.
    var maxPathWidth = Math.max(Math.round(containerWidth * 0.3),
                                containerWidth - currentWidth);
    maxPathWidth = Math.min(pathWidth, maxPathWidth);

    var parentCrumb = lastSeparator.previousSibling;
    var collapsedWidth = 0;
    if (parentCrumb && pathWidth - maxPathWidth > parentCrumb.clientWidth) {
      // At least one crumb is hidden completely (or almost completely).
      // Show sign of hidden crumbs like this:
      // root > some di... > ... > current directory.
      parentCrumb.setAttribute('collapsed', '');
      collapsedWidth = Math.min(maxPathWidth, parentCrumb.clientWidth);
      maxPathWidth -= collapsedWidth;
      if (parentCrumb.clientWidth != collapsedWidth)
        parentCrumb.style.width = collapsedWidth + 'px';

      lastSeparator = parentCrumb.previousSibling;
      if (!lastSeparator)
        return;
      collapsedWidth += lastSeparator.clientWidth;
      maxPathWidth = Math.max(0, maxPathWidth - lastSeparator.clientWidth);
    }

    pathWidth = 0;
    for (var item = bc.firstChild; item != lastSeparator;
         item = item.nextSibling) {
      // TODO(serya): Mixing access item.clientWidth and modifying style and
      // attributes could cause multiple layout reflows.
      if (pathWidth + item.clientWidth <= maxPathWidth) {
        pathWidth += item.clientWidth;
      } else if (pathWidth == maxPathWidth) {
        item.style.width = '0';
      } else if (item.classList.contains('separator')) {
        // Do not truncate separator. Instead let the last crumb be longer.
        item.style.width = '0';
        maxPathWidth = pathWidth;
      } else {
        // Truncate the last visible crumb.
        item.style.width = (maxPathWidth - pathWidth) + 'px';
        pathWidth = maxPathWidth;
      }
    }

    currentWidth = Math.min(currentWidth,
                            containerWidth - pathWidth - collapsedWidth);
    bc.lastChild.style.width = (currentWidth - lastCrumbSeparatorWidth) + 'px';
  };

  FileManager.prototype.formatMetadataValue_ = function(obj) {
    if (typeof obj.type == 'undefined') {
      return obj.value;
    } else if (obj.type == 'duration') {


      var totalSeconds = Math.floor(obj.value / 1000);
      var hours = Math.floor(totalSeconds / 60 / 60);

      var fmtSkeleton;

      // Print hours if available
      //TODO: dzvorygin use better skeletons when documentation become available
      if (hours > 0) {
        fmtSkeleton = 'hh:mm:ss';
      } else {
        fmtSkeleton = 'mm:ss';
      }

      // Convert duration to milliseconds since time start
      var date = new Date(parseInt(obj.value));

      var fmt = this.locale_.createDateTimeFormat({skeleton:fmtSkeleton});

      return fmt.format(date);
    }
  };

  FileManager.prototype.getThumbnailURL = function(entry, callback) {
    if (!entry)
      return;

    var iconType = this.getIconType(entry);

    function returnStockIcon() {
      callback(iconType, previewArt[iconType] || previewArt['unknown']);
    }

    var MAX_PIXEL_COUNT = 1 << 21;  // 2 Mpix
    var MAX_FILE_SIZE = 1 << 20;  // 1 Mb

    // This is a clone of a method in gallery.js which we cannot call because
    // it is only available when the gallery is loaded in to an iframe.
    // TODO(kaznacheev): Extract a common js file.
    // Keep in sync manually until then.
    function canUseImageForThumbnail(entry, metadata) {
      var fileSize = metadata.fileSize || entry.cachedSize_;
      return (fileSize && fileSize <= MAX_FILE_SIZE) ||
          (metadata.width && metadata.height &&
          (metadata.width * metadata.height <= MAX_PIXEL_COUNT));
    }

    this.metadataProvider_.fetch(entry.toURL(), function (metadata) {
      if (metadata.thumbnailURL) {
        callback(iconType, metadata.thumbnailURL, metadata.thumbnailTransform);
      } else if (iconType == 'image') {
        cacheEntrySize(entry, function() {
          if (canUseImageForThumbnail(entry, metadata)) {
            callback(iconType, entry.toURL(), metadata.imageTransform);
          } else {
            returnStockIcon();
          }
        }, returnStockIcon);
      } else {
        returnStockIcon();
      }
    });
  };

  FileManager.prototype.getDescription = function(entry, callback) {
    if (!entry)
      return;

    this.metadataProvider_.fetch(entry.toURL(), function(metadata) {
      callback(metadata.description);
    });
  };

  FileManager.prototype.focusCurrentList_ = function() {
    if (this.listType_ == FileManager.ListType.DETAIL)
      this.table_.focus();
    else  // this.listType_ == FileManager.ListType.THUMBNAIL)
      this.grid_.focus();
  };

  /**
   * Return full path of the current directory or null.
   */
  FileManager.prototype.getCurrentDirectory = function() {
    return this.directoryModel_ &&
        this.directoryModel_.currentEntry.fullPath;
  };

  FileManager.prototype.deleteEntries = function(entries, force, opt_callback) {
    if (!force) {
      var self = this;
      var msg;
      if (entries.length == 1) {
        msg = strf('CONFIRM_DELETE_ONE', entries[0].name);
      } else {
        msg = strf('CONFIRM_DELETE_SOME', entries.length);
      }

      this.confirm.show(msg, this.deleteEntries.bind(
          this, entries, true, opt_callback));
      return;
    }

    this.directoryModel_.deleteEntries(entries, opt_callback);
  };

  /**
   * Create the clipboard object from the current selection.
   *
   * We're not going through the system clipboard yet.
   */
  FileManager.prototype.copySelectionToClipboard = function(successCallback) {
    if (!this.selection || this.selection.totalCount == 0)
      return;

    this.clipboard_ = {
      isCut: false,
      sourceDirEntry: this.directoryModel_.currentEntry,
      entries: [].concat(this.selection.entries)
    };

    this.updateCommands_();
    this.showButter(str('SELECTION_COPIED'));
  };

  /**
   * Create the clipboard object from the current selection, marking it as a
   * cut operation.
   *
   * We're not going through the system clipboard yet.
   */
  FileManager.prototype.cutSelectionToClipboard = function(successCallback) {
    if (!this.selection || this.selection.totalCount == 0)
      return;

    this.clipboard_ = {
      isCut: true,
      sourceDirEntry: this.directoryModel_.currentEntry,
      entries: [].concat(this.selection.entries)
    };

    this.updateCommands_();
    this.showButter(str('SELECTION_CUT'));
  };

  /**
   * Queue up a file copy operation based on the current clipboard.
   */
  FileManager.prototype.pasteFromClipboard = function(successCallback) {
    if (!this.clipboard_)
      return null;

    this.showButter(str('PASTE_STARTED'), {timeout: 0});

    this.pasteSuccessCallbacks_.push(successCallback);
    this.copyManager_.queueCopy(this.clipboard_.sourceDirEntry,
                                this.directoryModel_.currentEntry,
                                this.clipboard_.entries,
                                this.clipboard_.isCut);
  };

  /**
   * Update the selection summary UI when the selection summarization completes.
   */
  FileManager.prototype.onSelectionSummarized_ = function() {
    var selection = this.selection;
    var bytes = util.bytesToSi(selection.bytes);
    var text = '';
    if (selection.totalCount == 0) {
      text = str('NOTHING_SELECTED');
    } else if (selection.fileCount == 1 && selection.directoryCount == 0) {
      text = selection.entries[0].name + ', ' + bytes;
    } else if (selection.fileCount == 0 && selection.directoryCount == 1) {
      text = selection.entries[0].name;
    } else if (selection.directoryCount == 0) {
      text = strf('MANY_FILES_SELECTED', selection.fileCount, bytes);
    } else if (selection.fileCount == 0) {
      text = strf('MANY_DIRECTORIES_SELECTED', selection.directoryCount);
    } else {
      text = strf('MANY_ENTRIES_SELECTED', selection.totalCount, bytes);
    }
    this.previewSummary_.textContent = text;
  };

  /**
   * Handle a click event on a breadcrumb element.
   *
   * @param {Event} event The click event.
   */
  FileManager.prototype.onBreadcrumbClick_ = function(event) {
    this.directoryModel_.changeDirectory(event.srcElement.path);
  };

  FileManager.prototype.onCheckboxMouseDownUp_ = function(event) {
    // If exactly one file is selected and its checkbox is *not* clicked,
    // then this should be treated as a "normal" click (ie. the previous
    // selection should be cleared).
    if (this.selection.totalCount == 1 && this.selection.entries[0].isFile) {
      var selectedIndex = this.selection.indexes[0];
      var listItem = this.currentList_.getListItemByIndex(selectedIndex);
      var checkbox = listItem.querySelector('input[type="checkbox"]');
      if (!checkbox.checked)
        return;
    }

    // Otherwise, treat clicking on a checkbox the same as a ctrl-click.
    // The default properties of event.ctrlKey make it read-only, but
    // don't prevent deletion, so we delete first, then set it true.
    delete event.ctrlKey;
    event.ctrlKey = true;
  };

  FileManager.prototype.onCheckboxClick_ = function(event) {
    if (event.shiftKey) {
      // Something about the timing of shift-clicks causes the checkbox
      // to get selected and then very quickly unselected.  It appears that
      // we forcibly select the checkbox as part of onSelectionChanged, and
      // then the default action of this click event fires and toggles the
      // checkbox back off.
      //
      // Since we're going to force checkboxes into the correct state for any
      // multi-selection, we can prevent this shift click from toggling the
      // checkbox and avoid the trouble.
      event.preventDefault();
    }
  };

  FileManager.prototype.selectDefaultPathInFilenameInput_ = function() {
    var input = this.filenameInput_;
    input.focus();
    var selectionEnd = input.value.lastIndexOf('.');
    if (selectionEnd == -1) {
      input.select();
    } else {
      input.selectionStart = 0;
      input.selectionEnd = selectionEnd;
    }
    // Clear, so we never do this again.
    this.params_.defaultPath = '';
  };

  /**
   * Update the UI when the selection model changes.
   *
   * @param {cr.Event} event The change event.
   */
  FileManager.prototype.onSelectionChanged_ = function(event) {
    this.summarizeSelection_();

    if (this.dialogType_ == FileManager.DialogType.SELECT_SAVEAS_FILE) {
      // If this is a save-as dialog, copy the selected file into the filename
      // input text box.

      if (this.selection &&
          this.selection.totalCount == 1 &&
          this.selection.entries[0].isFile &&
          this.filenameInput_.value != this.selection.entries[0].name) {
        this.filenameInput_.value = this.selection.entries[0].name;
      }
    }

    this.updateOkButton_();

    var self = this;
    setTimeout(function() { self.onSelectionChangeComplete_(event) }, 0);
  };

  /**
   * Handle selection change related tasks that won't run properly during
   * the actual selection change event.
   */
  FileManager.prototype.onSelectionChangeComplete_ = function(event) {
    // Inform tests it's OK to click buttons now.
    chrome.test.sendMessage('selection-change-complete');

    if (!this.showCheckboxes_)
      return;

    for (var i = 0; i < event.changes.length; i++) {
      // Turn off any checkboxes for items that are no longer selected.
      var selectedIndex = event.changes[i].index;
      var listItem = this.currentList_.getListItemByIndex(selectedIndex);
      if (!listItem) {
        // When changing directories, we get notified about list items
        // that are no longer there.
        continue;
      }

      if (!event.changes[i].selected) {
        var checkbox = listItem.querySelector('input[type="checkbox"]');
        checkbox.checked = false;
      }
    }

    if (this.selection.totalCount > 0) {
      // If more than one file is selected, make sure all checkboxes are lit
      // up.
      for (var i = 0; i < this.selection.entries.length; i++) {
        var selectedIndex = this.selection.indexes[i];
        var listItem = this.currentList_.getListItemByIndex(selectedIndex);
        if (listItem)
          listItem.querySelector('input[type="checkbox"]').checked = true;
      }
    }
    var selectAllCheckbox =
        this.document_.getElementById('select-all-checkbox');
    if (selectAllCheckbox)
      this.updateSelectAllCheckboxState_(selectAllCheckbox);
  };

  FileManager.prototype.updateOkButton_ = function(event) {
    var selectable;

    if (this.dialogType_ == FileManager.DialogType.SELECT_FOLDER) {
      selectable = this.selection.directoryCount == 1 &&
          this.selection.fileCount == 0;
    } else if (this.dialogType_ == FileManager.DialogType.SELECT_OPEN_FILE) {
      selectable = (this.selection.directoryCount == 0 &&
                    this.selection.fileCount == 1);
    } else if (this.dialogType_ ==
               FileManager.DialogType.SELECT_OPEN_MULTI_FILE) {
      selectable = (this.selection.directoryCount == 0 &&
                    this.selection.fileCount >= 1);
    } else if (this.dialogType_ == FileManager.DialogType.SELECT_SAVEAS_FILE) {
      if (this.directoryModel_.readonly) {
        // Nothing can be saved in to the root or media/ directories.
        selectable = false;
      } else {
        selectable = !!this.filenameInput_.value;
      }
    } else if (this.dialogType_ == FileManager.DialogType.FULL_PAGE) {
      // No "select" buttons on the full page UI.
      selectable = true;
    } else {
      throw new Error('Unknown dialog type');
    }

    this.okButton_.disabled = !selectable;
    return selectable;
  };

  /**
   * Handle a double-click event on an entry in the detail list.
   *
   * @param {Event} event The click event.
   */
  FileManager.prototype.onDetailDoubleClick_ = function(event) {
    if (this.isRenamingInProgress()) {
      // Don't pay attention to double clicks during a rename.
      return;
    }

    if (this.selection.totalCount != 1) {
      console.log('Invalid selection');
      return;
    }
    var entry = this.selection.entries[0];

    if (entry.isDirectory) {
      return this.onDirectoryAction(entry);
    }

    if (!this.okButton_.disabled)
      this.onOk_();

  };

  FileManager.prototype.onDirectoryAction = function(entry) {
    var deviceNumber = this.getDeviceNumber(entry);
    if (deviceNumber != undefined &&
        this.mountPoints_[deviceNumber].mountCondition ==
        'unknown_filesystem') {
      return this.showButter(str('UNKNOWN_FILESYSTEM_WARNING'));
    } else if (deviceNumber != undefined &&
               this.mountPoints_[deviceNumber].mountCondition ==
               'unsupported_filesystem') {
      return this.showButter(str('UNSUPPORTED_FILESYSTEM_WARNING'));
    } else {
      return this.directoryModel_.changeDirectory(entry.fullPath);
    }
  };

  /**
   * Show or hide the "Low disk space" warning.
   * @param {boolean} show True if the box need to be shown.
   */
  FileManager.prototype.showLowDiskSpaceWarning_ = function(show) {
    if (show) {
      if (this.downloadsWarning_.hasAttribute('hidden')) {
        this.downloadsWarning_.removeAttribute('hidden');
        this.requestResize_(0);
      }
    } else {
      if (!this.downloadsWarning_.hasAttribute('hidden')) {
        this.downloadsWarning_.setAttribute('hidden', '');
        this.requestResize_(100);
      }
    }
  };

  /**
   * Update the UI when the current directory changes.
   *
   * @param {cr.Event} event The directory-changed event.
   */
  FileManager.prototype.onDirectoryChanged_ = function(event) {
    this.updateCommands_();
    this.updateOkButton_();
    this.updateBreadcrumbs_();

    // Updated when a user clicks on the label of a file, used to detect
    // when a click is eligible to trigger a rename.  Can be null, or
    // an object with 'path' and 'date' properties.
    this.lastLabelClick_ = null;

    var dirEntry = event.newDirEntry;
    var location = document.location.origin + document.location.pathname + '#' +
                   encodeURI(dirEntry.fullPath);
    if (event.initial)
      history.replaceState(undefined, dirEntry.fullPath, location);
    else
      history.pushState(undefined, dirEntry.fullPath, location);

    this.checkFreeSpace_(this.getCurrentDirectory());

    // TODO(dgozman): title may be better than this.
    this.document_.title = this.getCurrentDirectory();

    var self = this;

    if (this.subscribedOnDirectoryChanges_) {
      chrome.fileBrowserPrivate.removeFileWatch(event.previousDirEntry.toURL(),
          function(result) {
            if (!result) {
              console.log('Failed to remove file watch');
            }
          });
    }

    if (event.newDirEntry.fullPath != '/') {
      this.subscribedOnDirectoryChanges_ = true;
      chrome.fileBrowserPrivate.addFileWatch(event.newDirEntry.toURL(),
        function(result) {
          if (!result) {
            console.log('Failed to add file watch');
          }
      });
    }
  };

  FileManager.prototype.findListItemForEvent_ = function(event) {
    var node = event.srcElement;
    var list = this.currentList_;
    // Assume list items are direct children of the list.
    if (node == list)
      return null;
    while (node) {
      var parent = node.parentNode;
      if (parent == list && node instanceof cr.ui.ListItem)
        return node;
      node = parent;
    }
    return null;
  };

  FileManager.prototype.onGridOrTableMouseDown_ = function(event) {
    this.updateCommands_();

    var item = this.findListItemForEvent_(event);
    if (!item)
      return;
    if (this.allowRenameClick_(event, item)) {
      event.preventDefault();
      this.initiateRename_(item);
    }
  };

  /**
   * Unload handler for the page.  May be called manually for the file picker
   * dialog, because it closes by calling extension API functions that do not
   * return.
   */
  FileManager.prototype.onUnload_ = function() {
    if (this.subscribedOnDirectoryChanges_) {
      this.subscribedOnDirectoryChanges_ = false;
      chrome.fileBrowserPrivate.removeFileWatch(
          this.directoryModel_.currentEntry.toURL(),
          function(result) {
            if (!result) {
              console.log('Failed to remove file watch');
            }
          });
    }
  };

  FileManager.prototype.onFileChanged_ = function(event) {
    // We receive a lot of events even in folders we are not interested in.
    if (event.fileUrl == this.directoryModel_.currentEntry.toURL())
      this.directoryModel_.rescanLater();
  };

  /**
   * Determine whether or not a click should initiate a rename.
   *
   * Renames can happen on mouse click if the user clicks on a label twice,
   * at least a half second apart.
   * @param {MouseEvent} event Click on the item.
   * @param {cr.ui.ListItem} item Clicked item.
   */
  FileManager.prototype.allowRenameClick_ = function(event, item) {
    var dir = this.directoryModel_.currentEntry;
    if (this.dialogType_ != FileManager.DialogType.FULL_PAGE ||
        this.directoryModel_.readonly) {
      // Renaming only enabled for full-page mode, outside of the root
      // directory.
      return false;
    }

    // Didn't click on the label.
    if (!event.srcElement.classList.contains('filename-label'))
      return false;

    // Wrong button or using a keyboard modifier.
    if (event.button != 0 || event.shiftKey || event.metaKey || event.altKey) {
      this.lastLabelClick_ = null;
      return false;
    }

    var now = new Date();
    var lastLabelClick = this.lastLabelClick_;
    this.lastLabelClick_ = {index: item.listIndex, date: now};

    if (this.isRenamingInProgress())
      return false;

    if (lastLabelClick && lastLabelClick.index == item.listIndex) {
      var delay = now - lastLabelClick.date;
      if (delay > 500 && delay < 2000) {
        this.lastLabelClick_ = null;
        return true;
      }
    }

    return false;
  };

  FileManager.prototype.initiateRename_ = function(item) {
    var label = item.querySelector('.filename-label');
    var input = this.renameInput_;

    input.value = label.textContent;
    label.parentNode.setAttribute('renaming', '');
    label.parentNode.appendChild(input);
    input.focus();
    var selectionEnd = input.value.lastIndexOf('.');
    if (selectionEnd == -1) {
      input.select();
    } else {
      input.selectionStart = 0;
      input.selectionEnd = selectionEnd;
    }

    // This has to be set late in the process so we don't handle spurious
    // blur events.
    input.currentEntry = this.currentList_.dataModel.item(item.listIndex);
  };

  FileManager.prototype.onRenameInputKeyDown_ = function(event) {
    if (!this.isRenamingInProgress())
      return;

    switch (event.keyCode) {
      case 27:  // Escape
        this.cancelRename_();
        event.preventDefault();
        break;

      case 13:  // Enter
        this.commitRename_();
        event.preventDefault();
        break;
    }

    // Do not move selection in list during rename.
    if (event.keyIdentifier == 'Up' || event.keyIdentifier == 'Down') {
      event.stopPropagation();
    }
  };

  FileManager.prototype.onRenameInputBlur_ = function(event) {
    if (this.isRenamingInProgress())
      this.cancelRename_();
  };

  FileManager.prototype.commitRename_ = function() {
    var entry = this.renameInput_.currentEntry;
    var newName = this.renameInput_.value;

    if (newName == entry.name) {
      this.cancelRename_();
      return;
    }

    if (!this.validateFileName_(newName))
      return;

    function onError(err) {
      this.alert.show(strf('ERROR_RENAMING', entry.name,
                           util.getFileErrorMnemonic(err.code)));
    }

    this.cancelRename_();

    this.directoryModel_.doesExist(newName, function(exists, isFile) {
      if (!exists) {
        this.directoryModel_.renameEntry(entry, newName, onError.bind(this));
      } else {
        var message = isFile ? 'FILE_ALREADY_EXISTS' :
                               'DIRECTORY_ALREADY_EXISTS';
        this.alert.show(strf(message, newName));
      }
    }.bind(this));
  };

  FileManager.prototype.cancelRename_ = function() {
    this.renameInput_.currentEntry = null;
    this.lastLabelClick_ = null;

    var parent = this.renameInput_.parentNode;
    if (parent) {
      parent.removeAttribute('renaming');
      parent.removeChild(this.renameInput_);
    }
    this.refocus();
  };

  FileManager.prototype.onFilenameInputKeyUp_ = function(event) {
    var enabled = this.updateOkButton_();
    if (enabled && event.keyCode == 13 /* Enter */)
      this.onOk_();
  };

  FileManager.prototype.onFilenameInputFocus_ = function(event) {
    var input = this.filenameInput_;

    // On focus we want to select everything but the extension, but
    // Chrome will select-all after the focus event completes.  We
    // schedule a timeout to alter the focus after that happens.
    setTimeout(function() {
        var selectionEnd = input.value.lastIndexOf('.');
        if (selectionEnd == -1) {
          input.select();
        } else {
          input.selectionStart = 0;
          input.selectionEnd = selectionEnd;
        }
    }, 0);
  };

  FileManager.prototype.onToggleSidebar_ = function(event) {
    if (this.dialogContainer_.hasAttribute('sidebar')) {
      this.dialogContainer_.removeAttribute('sidebar');
    } else {
      this.dialogContainer_.setAttribute('sidebar', 'sidebar');
    }
    // TODO(dgozman): make table header css-resizable.
    setTimeout(this.onResize_.bind(this), 300);
  };

  FileManager.prototype.onNewFolderCommand_ = function(event) {
    var self = this;

    function onNameSelected(name) {
      var valid = self.validateFileName_(name, function() {
        promptForName(name);
      });

      if (!valid) {
        // Validation failed.  User will be prompted for a new name after they
        // dismiss the validation error dialog.
        return;
      }

      self.createNewFolder(name);
    }

    function promptForName(suggestedName) {
      self.prompt.show(str('NEW_FOLDER_PROMPT'), suggestedName, onNameSelected);
    }

    promptForName(str('DEFAULT_NEW_FOLDER_NAME'));
  };

  FileManager.prototype.createNewFolder = function(name, opt_callback) {
    metrics.recordUserAction('CreateNewFolder');

    var self = this;

    function onError(err) {
      self.alert.show(strf('ERROR_CREATING_FOLDER', name,
                           util.getFileErrorMnemonic(err.code)));
    }

    var onSuccess = opt_callback || function() {};
    this.directoryModel_.createDirectory(name, onSuccess, onError);
  };

  FileManager.prototype.onDetailViewButtonClick_ = function(event) {
    this.setListType(FileManager.ListType.DETAIL);
  };

  FileManager.prototype.onThumbnailViewButtonClick_ = function(event) {
    this.setListType(FileManager.ListType.THUMBNAIL);
  };

  /**
   * KeyDown event handler for the document.
   */
  FileManager.prototype.onKeyDown_ = function(event) {
    if (event.srcElement === this.renameInput_) {
      // Ignore keydown handler in the rename input box.
      return;
    }

    switch (event.keyCode) {
      case 27:  // Escape => Cancel dialog.
        if (this.copyManager_.getStatus().totalFiles != 0) {
          // If there is a copy in progress, ESC will cancel it.
          event.preventDefault();
          this.copyManager_.requestCancel();
          return;
        }

        if (this.butterTimer_) {
          // Allow the user to manually dismiss timed butter messages.
          event.preventDefault();
          this.hideButter();
          return;
        }

        if (this.dialogType_ != FileManager.DialogType.FULL_PAGE) {
          // If there is nothing else for ESC to do, then cancel the dialog.
          event.preventDefault();
          this.onCancel_();
        }
        break;

      case 190:  // Ctrl-. => Toggle filter files.
        if (event.ctrlKey) {
          this.filterFiles_ = !this.filterFiles_;
          this.directoryModel_.rescan();
        }
        break;
    }
  };

  /**
   * KeyDown event handler for the div.list-container element.
   */
  FileManager.prototype.onListKeyDown_ = function(event) {
    if (event.srcElement.tagName == 'INPUT') {
      // Ignore keydown handler in the rename input box.
      return;
    }

    switch (event.keyCode) {
      case 8:  // Backspace => Up one directory.
        event.preventDefault();
        var path = this.getCurrentDirectory();
        if (path && !DirectoryModel.isRootPath(path)) {
          var path = path.replace(/\/[^\/]+$/, '');
          this.directoryModel_.changeDirectory(path);
        }
        break;

      case 13:  // Enter => Change directory or complete dialog.
        if (this.selection.totalCount == 1 &&
            this.selection.entries[0].isDirectory &&
            this.dialogType_ != FileManager.SELECT_FOLDER) {
          event.preventDefault();
          this.onDirectoryAction(this.selection.entries[0]);
        } else if (!this.okButton_.disabled) {
          event.preventDefault();
          this.onOk_();
        }
        break;

      case 32:  // Ctrl-Space => New Folder.
        if ((this.dialogType_ == 'saveas-file' ||
             this.dialogType_ == 'full-page') && event.ctrlKey) {
          event.preventDefault();
          this.onNewFolderCommand_();
        }
        break;

      case 88:  // Ctrl-X => Cut.
        this.updateCommands_();
        if (!this.commands_['cut'].disabled) {
          event.preventDefault();
          this.commands_['cut'].execute();
        }
        break;

      case 67:  // Ctrl-C => Copy.
        this.updateCommands_();
        if (!this.commands_['copy'].disabled) {
          event.preventDefault();
          this.commands_['copy'].execute();
        }
        break;

      case 86:  // Ctrl-V => Paste.
        this.updateCommands_();
        if (!this.commands_['paste'].disabled) {
          event.preventDefault();
          this.commands_['paste'].execute();
        }
        break;

      case 69:  // Ctrl-E => Rename.
        this.updateCommands_();
        if (!this.commands_['rename'].disabled) {
          event.preventDefault();
          this.commands_['rename'].execute();
        }
        break;

      case 46:  // Delete.
        this.updateCommands_();
        if (!this.commands_['delete'].disabled) {
          event.preventDefault();
          this.commands_['delete'].execute();
        }
        break;
    }
  };

  /**
   * KeyPress event handler for the div.list-container element.
   */
  FileManager.prototype.onListKeyPress_ = function(event) {
    if (event.srcElement.tagName == 'INPUT') {
      // Ignore keypress handler in the rename input box.
      return;
    }

    if (event.ctrlKey || event.metaKey || event.altKey)
      return;

    var now = new Date();
    var char = String.fromCharCode(event.charCode).toLowerCase();
    var text = now - this.textSearchState_.date > 1000 ? '' :
        this.textSearchState_.text;
    this.textSearchState_ = {text: text + char, date: now};

    this.doTextSearch_();
  };

  /**
   * Performs a 'text search' - selects a first list entry with name
   * starting with entered text (case-insensitive).
   */
  FileManager.prototype.doTextSearch_ = function() {
    var text = this.textSearchState_.text;
    if (!text)
      return;

    var dm = this.directoryModel_.fileList;
    for (var index = 0; index < dm.length; ++index) {
      var name = dm.item(index).name;
      if (name.substring(0, text.length).toLowerCase() == text) {
        this.currentList_.selectionModel.selectedIndexes = [index];
        return;
      }
    }

    this.textSearchState_.text = '';
  };

  /**
   * Handle a click of the cancel button.  Closes the window.
   * TODO(jamescook): Make unload handler work automatically, crbug.com/104811
   *
   * @param {Event} event The click event.
   */
  FileManager.prototype.onCancel_ = function(event) {
    chrome.fileBrowserPrivate.cancelDialog();
    this.onUnload_();
    window.close();
  };

  /**
   * Selects a file.  Closes the window.
   * TODO(jamescook): Make unload handler work automatically, crbug.com/104811
   *
   * @param {string} fileUrl The filename as a URL.
   * @param {number} filterIndex The integer file filter index.
   */
  FileManager.prototype.selectFile_ = function(fileUrl, filterIndex) {
    chrome.fileBrowserPrivate.selectFile(fileUrl, filterIndex);
    this.onUnload_();
    window.close();
  };

  /**
   * Selects multiple files.  Closes the window.
   * TODO(jamescook): Make unload handler work automatically, crbug.com/104811
   *
   * @param {Array.<string>} fileUrls Array of filename URLs.
   */
  FileManager.prototype.selectFiles_ = function(fileUrls) {
    chrome.fileBrowserPrivate.selectFiles(fileUrls);
    this.onUnload_();
    window.close();
  };

  /**
   * Handle a click of the ok button.
   *
   * The ok button has different UI labels depending on the type of dialog, but
   * in code it's always referred to as 'ok'.
   *
   * @param {Event} event The click event.
   */
  FileManager.prototype.onOk_ = function(event) {
    var currentDirUrl = this.directoryModel_.currentEntry.toURL();

    if (currentDirUrl.charAt(currentDirUrl.length - 1) != '/')
      currentDirUrl += '/';

    var self = this;
    if (this.dialogType_ == FileManager.DialogType.SELECT_SAVEAS_FILE) {
      // Save-as doesn't require a valid selection from the list, since
      // we're going to take the filename from the text input.
      var filename = this.filenameInput_.value;
      if (!filename)
        throw new Error('Missing filename!');
      if (!this.validateFileName_(filename))
        return;

      function resolveCallback(victim) {
        if (victim instanceof FileError) {
          // File does not exist.  Closes the window and does not return.
          self.selectFile_(
              currentDirUrl + encodeURIComponent(filename),
              self.getSelectedFilterIndex_(filename));
        }

        if (victim.isDirectory) {
          // Do not allow to overwrite directory.
          self.alert.show(strf('DIRECTORY_ALREADY_EXISTS', filename));
        } else {
          self.confirm.show(strf('CONFIRM_OVERWRITE_FILE', filename),
                            function() {
                              // User selected Ok from the confirm dialog.
                              self.selectFile_(
                                  currentDirUrl + encodeURIComponent(filename),
                                  self.getSelectedFilterIndex_(filename));
                            });
        }
        return;
      }

      this.resolvePath(this.getCurrentDirectory() + '/' + filename,
          resolveCallback, resolveCallback);
      return;
    }

    var files = [];
    var selectedIndexes = this.currentList_.selectionModel.selectedIndexes;

    // All other dialog types require at least one selected list item.
    // The logic to control whether or not the ok button is enabled should
    // prevent us from ever getting here, but we sanity check to be sure.
    if (!selectedIndexes.length)
      throw new Error('Nothing selected!');

    var dm = this.directoryModel_.fileList;
    for (var i = 0; i < selectedIndexes.length; i++) {
      var entry = dm.item(selectedIndexes[i]);
      if (!entry) {
        console.log('Error locating selected file at index: ' + i);
        continue;
      }

      files.push(currentDirUrl + encodeURIComponent(entry.name));
    }

    // Multi-file selection has no other restrictions.
    if (this.dialogType_ == FileManager.DialogType.SELECT_OPEN_MULTI_FILE) {
      // Closes the window and does not return.
      this.selectFiles_(files);
      return;
    }

    // In full screen mode, open all files for vieweing.
    if (this.dialogType_ == FileManager.DialogType.FULL_PAGE) {
      if (this.galleryTask_) {
        var urls = [];
        for (i = 0; i < selectedIndexes.length; i++) {
          var entry = dm.item(selectedIndexes[i]);
          if (this.getFileType(entry).type != 'image')
            break;
          urls.push(entry.toURL());
        }
        if (urls.length == selectedIndexes.length) {  // Selection is all images
          this.dispatchFileTask_(this.galleryTask_, urls);
          return;
        }
      }
      chrome.fileBrowserPrivate.viewFiles(files, "default", function(success) {
        if (success || files.length != 1)
          return;
        // Run the default task if the browser wasn't able to handle viewing.
        chrome.fileBrowserPrivate.getFileTasks(
            files,
            function(tasksList) {
              // Run the top task from the list when double click can't
              // be handled by the browser internally.
              if (tasksList && tasksList.length == 1) {
                var task = tasksList[0];
                var task_parts = task.taskId.split('|');
                if (task_parts[0] == self.getExtensionId_())
                  task.internal = true;

                self.dispatchFileTask_(task, files);
              } else {
                var fileUrl = files[0];
                self.alert.showWithTitle(
                    unescape(fileUrl.substr(fileUrl.lastIndexOf('/') + 1)),
                    str('ERROR_VIEWING_FILE'),
                    function() {});
              }
            });
      });
      // Window stays open.
      return;
    }

    // Everything else must have exactly one.
    if (files.length > 1)
      throw new Error('Too many files selected!');

    var selectedEntry = dm.item(selectedIndexes[0]);

    if (this.dialogType_ == FileManager.DialogType.SELECT_FOLDER) {
      if (!selectedEntry.isDirectory)
        throw new Error('Selected entry is not a folder!');
    } else if (this.dialogType_ == FileManager.DialogType.SELECT_OPEN_FILE) {
      if (!selectedEntry.isFile)
        throw new Error('Selected entry is not a file!');
    }

    // Closes the window and does not return.
    this.selectFile_(files[0], this.getSelectedFilterIndex_(files[0]));
  };

  /**
   * Verifies the user entered name for file or folder to be created or
   * renamed to. Name restrictions must correspond to File API restrictions
   * (see DOMFilePath::isValidPath). Curernt WebKit implementation is
   * out of date (spec is
   * http://dev.w3.org/2009/dap/file-system/file-dir-sys.html, 8.3) and going to
   * be fixed. Shows message box if the name is invalid.
   *
   * @param {name} name New file or folder name.
   * @param {function} onAccept Function to invoke when user accepts the
   *    message box.
   * @return {boolean} True if name is vaild.
   */
  FileManager.prototype.validateFileName_ = function(name, onAccept) {
    var msg;
    var testResult = /[\/\\\<\>\:\?\*\"\|]/.exec(name);
    if (testResult) {
      msg = strf('ERROR_INVALID_CHARACTER', testResult[0]);
    } else if (/^\s*$/i.test(name)) {
      msg = str('ERROR_WHITESPACE_NAME');
    } else if (/^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$/i.test(name)) {
      msg = str('ERROR_RESERVED_NAME');
    } else if (this.filterFiles_ && name[0] == '.') {
      msg = str('ERROR_HIDDEN_NAME');
    }

    if (msg) {
      console.log('no no no');
      this.alert.show(msg, onAccept);
      return false;
    }

    return true;
  };

  /**
   * Start or stop monitoring free space depending on the new value of current
   * directory path. In case the space is low shows a warning box.
   * @param {string} currentPath New path to the current directory.
   */
  FileManager.prototype.checkFreeSpace_ = function(currentPath) {
    var dir = DirectoryModel.DOWNLOADS_DIRECTORY;
    if (currentPath.substr(1, dir.length) == dir) {
      // Setup short timeout if currentPath just changed.
      if (this.checkFreeSpaceTimer_)
        clearTimeout(this.checkFreeSpaceTimer_);

      // Right after changing directory the bottom pannel usually hides.
      // Simultaneous animation looks unpleasent. Also showing the warning is
      // lower priority task. So delay it.
      this.checkFreeSpaceTimer_ = setTimeout(doCheck, 500);
    } else {
      if (this.checkFreeSpaceTimer_) {
        clearTimeout(this.checkFreeSpaceTimer_);
        this.checkFreeSpaceTimer_ = 0;

        // Hide warning immediately.
        this.showLowDiskSpaceWarning_(false);
      }
    }

    var self = this;
    function doCheck() {
      var path = '/' + dir;
      self.resolvePath(path, function(downloadsDirEntry) {
        chrome.fileBrowserPrivate.getSizeStats(downloadsDirEntry.toURL(),
          function(sizeStats) {
            // sizeStats is undefined if some error occur.
            var lowDiskSpace = false;
            if (sizeStats && sizeStats.totalSizeKB > 0) {
              var ratio = sizeStats.remainingSizeKB / sizeStats.totalSizeKB;

              lowDiskSpace = ratio < 0.2;
              self.showLowDiskSpaceWarning_(lowDiskSpace);
            }

            // If disk space is low check it more often. User can delete files
            // manually and we should not bother her with warning in this case.
            setTimeout(doCheck, lowDiskSpace ? 1000 * 5 : 1000 * 30);
          });
      }, onError);

      function onError(err) {
        console.log('Error while checking free space: ' + err);
        setTimeout(doCheck, 1000 * 60);
      }
    }
  }
})();

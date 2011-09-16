// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Setting the src of an img to an empty string can crash the browser, so we
// use an empty 1x1 gif instead.
const EMPTY_IMAGE_URI = 'data:image/gif;base64,'
        + 'R0lGODlhAQABAPABAP///wAAACH5BAEKAAAALAAAAAABAAEAAAICRAEAOw%3D%3D';

var g_slideshow_data = null;

const GALLERY_ENABLED = true;

// If directory files changes too often, don't rescan directory more than once
// per specified interval
const SIMULTANEOUS_RESCAN_INTERVAL = 1000;

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
 * @param {Object} params A map of parameter names to values controlling the
 *     appearance of the FileManager.  Names are:
 *     - type: A value from FileManager.DialogType defining what kind of
 *       dialog to present.  Defaults to FULL_PAGE.
 *     - title: The title for the dialog.  Defaults to a localized string based
 *       on the dialog type.
 *     - defaultPath: The default path for the dialog.  The default path should
 *       end with a trailing slash if it represents a directory.
 */
function FileManager(dialogDom, filesystem, rootEntries) {
  console.log('Init FileManager: ' + dialogDom);

  this.dialogDom_ = dialogDom;
  this.rootEntries_ = rootEntries;
  this.filesystem_ = filesystem;
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
  this.pendingRescanQueue_ = [];
  this.rescanRunning_ = false;

  this.commands_ = {};

  this.document_ = dialogDom.ownerDocument;
  this.dialogType_ = this.params_.type || FileManager.DialogType.FULL_PAGE;

  this.alert = new cr.ui.dialogs.AlertDialog(this.dialogDom_);
  this.confirm = new cr.ui.dialogs.ConfirmDialog(this.dialogDom_);
  this.prompt = new cr.ui.dialogs.PromptDialog(this.dialogDom_);

  // TODO(dgozman): This will be changed to LocaleInfo.
  this.locale_ = new v8Locale(navigator.language);

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

  // DirectoryEntry representing the current directory of the dialog.
  this.currentDirEntry_ = null;

  this.copyManager_ = new FileCopyManager();
  this.copyManager_.addEventListener('copy-progress',
                                     this.onCopyProgress_.bind(this));

  window.addEventListener('popstate', this.onPopState_.bind(this));
  window.addEventListener('unload', this.onUnload_.bind(this));

  this.addEventListener('directory-changed',
                        this.onDirectoryChanged_.bind(this));
  this.addEventListener('selection-summarized',
                        this.onSelectionSummarized_.bind(this));

  // The list of archives requested to mount. We will show contents once
  // archive is mounted, but only for mounts from within this filebrowser tab.
  this.mountRequests_ = [];
  chrome.fileBrowserPrivate.onMountCompleted.addListener(
      this.onMountCompleted_.bind(this));

  chrome.fileBrowserPrivate.onFileChanged.addListener(
    this.onFileChanged_.bind(this));

  var self = this;

  // The list of callbacks to be invoked during the directory rescan after
  // all paste tasks are complete.
  this.pasteSuccessCallbacks_ = [];

  // The list of active mount points to distinct them from other directories.
  chrome.fileBrowserPrivate.getMountPoints(function(mountPoints) {
      self.mountPoints_ = mountPoints;
  });

  chrome.fileBrowserHandler.onExecute.addListener(
    this.onFileTaskExecute_.bind(this));

  this.initCommands_();
  this.initDom_();
  this.initDialogType_();
  this.setupCurrentDirectory_();

  this.summarizeSelection_();
  this.updatePreview_();

  this.dataModel_.sort('cachedMtime_', 'desc');

  this.refocus();

  this.createMetadataProvider_();
}

FileManager.prototype = {
  __proto__: cr.EventTarget.prototype
};

// Anonymous "namespace".
(function() {

  // Private variables and helper functions.

  /**
   * Unicode codepoint for 'BLACK RIGHT-POINTING SMALL TRIANGLE'.
   */
  const RIGHT_TRIANGLE = '\u25b8';

  /**
   * The DirectoryEntry.fullPath value of the directory containing externally
   * mounted removable storage volumes.
   */
  const REMOVABLE_DIRECTORY = '/removable';

  /**
   * The DirectoryEntry.fullPath value of the directory containing externally
   * mounted archive file volumes.
   */
  const ARCHIVE_DIRECTORY = '/archive';

  /**
   * The DirectoryEntry.fullPath value of the downloads directory.
   */
  const DOWNLOADS_DIRECTORY = '/Downloads';

  /**
   * Height of the downloads folder warning, in px.
   */
  const DOWNLOADS_WARNING_HEIGHT = '57px';

  /**
   * Location of the FAQ about the downloads directory.
   */
  const DOWNLOADS_FAQ_URL = 'http://www.google.com/support/chromeos/bin/' +
      'answer.py?hl=en&answer=1061547';

  /**
   * Mnemonics for the second parameter of the changeDirectory method.
   */
  const CD_WITH_HISTORY = true;
  const CD_NO_HISTORY = false;

  /**
   * Mnemonics for the recurse parameter of the copyFiles method.
   */
  const CP_RECURSE = true;
  const CP_NO_RECURSE = false;

  /**
   * Translated strings.
   */
  var localStrings;

  /**
   * Map of icon types to regular expressions and functions
   *
   * The first regexp/function to match the entry name determines the icon type
   * assigned to dom elements for a file. Order of evaluation is not
   * defined, so don't depend on it.
   */
  const iconTypes = {
    'audio': /\.(flac|mp3|m4a|oga|ogg|wav)$/i,
    'html': /\.(html?)$/i,
    'image': /\.(bmp|gif|jpe?g|ico|png|webp)$/i,
    'pdf' : /\.(pdf)$/i,
    'text': /\.(pod|rst|txt|log)$/i,
    'video': /\.(3gp|avi|mov|mp4|m4v|mpe?g4?|ogm|ogv|ogx|webm)$/i,
    'device': function(self, entry) {
      var deviceNumber = self.getDeviceNumber(entry);
      if (deviceNumber != undefined)
        return (self.mountPoints_[deviceNumber].mountCondition == '');
      return false;
    },
    'unreadable': function(self, entry) {
      var deviceNumber = self.getDeviceNumber(entry);
      if (deviceNumber != undefined) {
        return self.mountPoints_[deviceNumber].mountCondition ==
               'unknown_filesystem' ||
               self.mountPoints_[deviceNumber].mountCondition ==
               'unsupported_filesystem';
      }
      return false;
    }
  };

  const previewArt = {
    'audio': 'images/filetype_large_audio.png',
    // TODO(sidor): Find better icon here.
    'device': 'images/filetype_large_folder.png',
    'folder': 'images/filetype_large_folder.png',
    'unknown': 'images/filetype_large_generic.png',
    // TODO(sidor): Find better icon here.
    'unreadable': 'images/filetype_large_folder.png',
    'video': 'images/filetype_large_video.png'
  };

  /**
   * Regexp for archive files. Used to show mount-archive task.
   */
  const ARCHIVES_REGEXP = /.zip$/;

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
   * Get the size of a file, caching the result.
   *
   * When this method completes, the fileEntry object will get a
   * 'cachedSize_' property (if it doesn't already have one) containing the
   * size of the file in bytes.
   *
   * @param {Entry} entry An HTML5 Entry object.
   * @param {function(Entry)} successCallback The function to invoke once the
   *     file size is known.
   */
  function cacheEntrySize(entry, successCallback, opt_errorCallback) {
    if (entry.isDirectory) {
      // No size for a directory, -1 ensures it's sorted before 0 length files.
      entry.cachedSize_ = -1;
    }

    if ('cachedSize_' in entry) {
      if (successCallback) {
        // Callback via a setTimeout so the sync/async semantics don't change
        // based on whether or not the value is cached.
        setTimeout(function() { successCallback(entry) }, 0);
      }
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
   */
  function cacheEntryDate(entry, successCallback, opt_errorCallback) {
    if ('cachedMtime_' in entry) {
      if (successCallback) {
        // Callback via a setTimeout so the sync/async semantics don't change
        // based on whether or not the value is cached.
        setTimeout(function() { successCallback(entry) }, 0);
      }
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

  function isSystemDirEntry(dirEntry) {
    return dirEntry.fullPath == '/' ||
        dirEntry.fullPath == REMOVABLE_DIRECTORY ||
        dirEntry.fullPath == ARCHIVE_DIRECTORY;
  }

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
   * One-time initialization of various DOM nodes.
   */
  FileManager.prototype.initDom_ = function() {
    // Cache nodes we'll be manipulating.
    this.previewImage_ = this.dialogDom_.querySelector('.preview-img');
    this.previewFilename_ = this.dialogDom_.querySelector('.preview-filename');
    this.previewSummary_ = this.dialogDom_.querySelector('.preview-summary');
    this.previewMetadata_ = this.dialogDom_.querySelector('.preview-metadata');
    this.filenameInput_ = this.dialogDom_.querySelector('.filename-input');
    this.taskButtons_ = this.dialogDom_.querySelector('.task-buttons');
    this.okButton_ = this.dialogDom_.querySelector('.ok');
    this.cancelButton_ = this.dialogDom_.querySelector('.cancel');
    this.newFolderButton_ = this.dialogDom_.querySelector('.new-folder');
    this.copyButton_ = this.dialogDom_.querySelector('.clipboard-copy');
    this.pasteButton_ = this.dialogDom_.querySelector('.clipboard-paste');

    this.downloadsWarning_ =
        this.dialogDom_.querySelector('.downloads-warning');
    var html = util.htmlUnescape(str('DOWNLOADS_DIRECTORY_WARNING'));
    this.downloadsWarning_.lastElementChild.innerHTML = html;
    var link = this.downloadsWarning_.querySelector('a');
    link.addEventListener('click', this.onDownloadsWarningClick_.bind(this));

    this.document_.addEventListener('keydown', this.onKeyDown_.bind(this));

    this.descriptionTable_ =
        this.dialogDom_.querySelector('.preview-metadata-table');

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

    this.dialogDom_.querySelector('button.new-folder').addEventListener(
        'click', this.onNewFolderButtonClick_.bind(this));

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

    // Always sharing the data model between the detail/thumb views confuses
    // them.  Instead we maintain this bogus data model, and hook it up to the
    // view that is not in use.
    this.emptyDataModel_ = new cr.ui.ArrayDataModel([]);

    this.dataModel_ = new cr.ui.ArrayDataModel([]);
    var collator = this.collator_;
    this.dataModel_.setCompareFunction('name', function(a, b) {
      return collator.compare(a.name, b.name);
    });
    this.dataModel_.setCompareFunction('cachedMtime_',
                                       this.compareMtime_.bind(this));
    this.dataModel_.setCompareFunction('cachedSize_',
                                       this.compareSize_.bind(this));
    this.dataModel_.prepareSort = this.prepareSort_.bind(this);

    if (this.dialogType_ == FileManager.DialogType.SELECT_OPEN_FILE ||
        this.dialogType_ == FileManager.DialogType.SELECT_OPEN_FOLDER ||
        this.dialogType_ == FileManager.DialogType.SELECT_SAVEAS_FILE) {
      this.selectionModelClass_ = cr.ui.ListSingleSelectionModel;
    } else {
      this.selectionModelClass_ = cr.ui.ListSelectionModel;
    }

    this.initTable_();
    this.initGrid_();

    this.setListType(FileManager.ListType.DETAIL);

    this.onResize_();
    this.dialogDom_.style.opacity = '1';

    this.textSearchState_ = {text: '', date: new Date()};
  };

  /**
   * Get the icon type for a given Entry.
   *
   * @param {Entry} entry An Entry subclass (FileEntry or DirectoryEntry).
   * @return {string} One of the keys from FileManager.iconTypes, or
   *     'unknown'.
   */
  FileManager.prototype.getIconType = function(entry) {
    if (entry.cachedIconType_)
      return entry.cachedIconType_;

    var rv = 'unknown';
   if (entry.isDirectory)
      rv = 'folder';
    for (var name in iconTypes) {
      var value = iconTypes[name];

      if (value instanceof RegExp) {
        if (value.test(entry.name))  {
          rv = name;
          break;
        }
      } else if (typeof value == 'function') {
        try {
          if (value(this,entry)) {
            rv = name;
            break;
          }
        } catch (ex) {
          console.error('Caught exception while evaluating iconType: ' +
                        name, ex);
        }
      } else {
        console.log('Unexpected value in iconTypes[' + name + ']: ' + value);
      }
    }
    entry.cachedIconType_ = rv;
    return rv;
  };


  /**
   * Get the icon type of a file, caching the result.
   *
   * When this method completes, the fileEntry object will get a
   * 'cachedIconType_' property (if it doesn't already have one) containing the
   * icon type of the file as a string.
   *
   * The successCallback is always invoked synchronously, since this does not
   * actually require an async call.  You should not depend on this, as it may
   * change if we were to start reading magic numbers (for example).
   *
   * @param {Entry} entry An HTML5 Entry object.
   * @param {function(Entry)} successCallback The function to invoke once the
   *     icon type is known.
   */
  FileManager.prototype.cacheEntryIconType = function(entry, successCallback) {
    this.getIconType(entry);
    if (successCallback)
      setTimeout(function() { successCallback(entry) }, 0);
  }

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

  FileManager.prototype.refocus = function() {
    this.document_.querySelector('[tabindex="0"]').focus();
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
    switch (event.command.id) {
      case 'cut':
        event.canExecute =
          (this.currentDirEntry_ &&
           !isSystemDirEntry(this.currentDirEntry_));
        break;

      case 'copy':
        event.canExecute =
          (this.currentDirEntry_ &&
           this.currentDirEntry_.fullPath != '/');
        if (this.copyButton_)
          this.copyButton_.disabled = !event.canExecute;
        break;

      case 'paste':
        event.canExecute =
          (this.clipboard_ &&
           this.clipboard_.sourceDirEntry.fullPath !=
           this.currentDirEntry_.fullPath &&
           !isSystemDirEntry(this.currentDirEntry_));
        if (this.pasteButton_)
          this.pasteButton_.disabled = !event.canExecute;
        break;

      case 'rename':
        event.canExecute =
          (// Initialized to the point where we have a current directory
           this.currentDirEntry_ &&
           // Rename not in progress.
           !this.renameInput_.currentEntry &&
           // Only one file selected.
           this.selection &&
           this.selection.totalCount == 1 &&
           !isSystemDirEntry(this.currentDirEntry_));
         break;

      case 'delete':
        event.canExecute =
          (// Initialized to the point where we have a current directory
           this.currentDirEntry_ &&
           // Rename not in progress.
           !this.renameInput_.currentEntry &&
           !isSystemDirEntry(this.currentDirEntry_));
         break;
    }
  };

  FileManager.prototype.setListType = function(type) {
    if (type && type == this.listType_)
      return;

    if (type == FileManager.ListType.DETAIL) {
      var selectedIndexes = this.grid_.selectionModel.selectedIndexes;
      this.table_.dataModel = this.dataModel_;
      this.table_.style.display = '';
      this.grid_.style.display = 'none';
      this.grid_.dataModel = this.emptyDataModel_;
      this.currentList_ = this.table_;
      this.dialogDom_.querySelector('button.detail-view').disabled = true;
      this.dialogDom_.querySelector('button.thumbnail-view').disabled = false;
      this.table_.selectionModel.selectedIndexes = selectedIndexes;
    } else if (type == FileManager.ListType.THUMBNAIL) {
      var selectedIndexes = this.table_.selectionModel.selectedIndexes;
      this.grid_.dataModel = this.dataModel_;
      this.grid_.style.display = '';
      this.table_.style.display = 'none';
      this.table_.dataModel = this.emptyDataModel_;
      this.currentList_ = this.grid_;
      this.dialogDom_.querySelector('button.thumbnail-view').disabled = true;
      this.dialogDom_.querySelector('button.detail-view').disabled = false;
      this.grid_.selectionModel.selectedIndexes = selectedIndexes;
    } else {
      throw new Error('Unknown list type: ' + type);
    }

    this.listType_ = type;
    this.onResize_();
  };

  /**
   * Initialize the file thumbnail grid.
   */
  FileManager.prototype.initGrid_ = function() {
    this.grid_ = this.dialogDom_.querySelector('.thumbnail-grid');
    cr.ui.Grid.decorate(this.grid_);

    var self = this;
    this.grid_.itemConstructor = function(entry) {
      return self.renderThumbnail_(entry);
    };

    this.grid_.selectionModel = new this.selectionModelClass_();

    this.grid_.addEventListener(
        'dblclick', this.onDetailDoubleClick_.bind(this));
    this.grid_.selectionModel.addEventListener(
        'change', this.onSelectionChanged_.bind(this));
    this.grid_.selectionModel.addEventListener(
        'leadIndexChange', this.onLeadIndexChanged_.bind(this));
    cr.ui.contextMenuHandler.addContextMenuProperty(this.grid_);
    this.grid_.contextMenu = this.fileContextMenu_;
    this.grid_.addEventListener('mousedown',
                                this.onGridMouseDown_.bind(this));
  };

  /**
   * Initialize the file list table.
   */
  FileManager.prototype.initTable_ = function() {
    var checkWidth = this.showCheckboxes_ ? 5 : 0;

    var columns = [
        new cr.ui.table.TableColumn('cachedIconType_', '',
                                    5.4 + checkWidth),
        new cr.ui.table.TableColumn('name', str('NAME_COLUMN_LABEL'),
                                    64 - checkWidth),
        new cr.ui.table.TableColumn('cachedSize_',
                                    str('SIZE_COLUMN_LABEL'), 15.5),
        new cr.ui.table.TableColumn('cachedMtime_',
                                    str('DATE_COLUMN_LABEL'), 21)
    ];

    columns[0].renderFunction = this.renderIconType_.bind(this);
    columns[1].renderFunction = this.renderName_.bind(this);
    columns[2].renderFunction = this.renderSize_.bind(this);
    columns[3].renderFunction = this.renderDate_.bind(this);

    this.table_ = this.dialogDom_.querySelector('.detail-table');
    cr.ui.Table.decorate(this.table_);

    this.table_.selectionModel = new this.selectionModelClass_();
    this.table_.columnModel = new cr.ui.table.TableColumnModel(columns);

    this.table_.addEventListener(
        'dblclick', this.onDetailDoubleClick_.bind(this));
    this.table_.selectionModel.addEventListener(
        'change', this.onSelectionChanged_.bind(this));
    this.table_.selectionModel.addEventListener(
        'leadIndexChange', this.onLeadIndexChanged_.bind(this));

    cr.ui.contextMenuHandler.addContextMenuProperty(this.table_);
    this.table_.contextMenu = this.fileContextMenu_;

    this.table_.addEventListener('mousedown',
                                 this.onTableMouseDown_.bind(this));
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
      this.rescanDirectory_(function() {
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
      this.rescanDirectory_();
    } else if (event.reason == 'CANCELLED') {
      this.showButter(str('PASTE_CANCELLED'));
      this.rescanDirectory_();
    } else {
      console.log('Unknown event reason: ' + event.reason);
      this.rescanDirectory_();
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
        var leadIndex = this.currentList_.selectionModel.leadIndex;
        var li = this.currentList_.getListItemByIndex(leadIndex);
        var label = li.querySelector('.filename-label');
        if (!label) {
          console.warn('Unable to find label for rename of index: ' +
                       leadIndex);
          return;
        }

        this.initiateRename_(label);
        return;

      case 'delete':
        this.deleteEntries(this.selection.entries);
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
    var self = this;
    setTimeout(function() { self.onResize_() }, timeout || 0);
  };

  /**
   * Resize details and thumb views to fit the new window size.
   */
  FileManager.prototype.onResize_ = function() {
    this.table_.style.height = this.grid_.style.height =
      this.grid_.parentNode.clientHeight + 'px';
    this.table_.style.width = this.grid_.style.width =
      this.grid_.parentNode.clientWidth + 'px';

    this.table_.list_.style.width = this.table_.parentNode.clientWidth + 'px';
    this.table_.list_.style.height = (this.table_.clientHeight - 1 -
                                      this.table_.header_.clientHeight) + 'px';

    if (this.listType_ == FileManager.ListType.THUMBNAIL) {
      var self = this;
      setTimeout(function() {
          self.grid_.columns = 0;
          self.grid_.redraw();
      }, 0);
    } else {
      this.currentList_.redraw();
    }
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
      this.changeDirectory(path, CD_NO_HISTORY);
      return;
    } else if (this.params_.defaultPath) {
      this.setupPath_(this.params_.defaultPath);
    } else {
      this.setupDefaultPath_();
    }
  };

  FileManager.prototype.setupDefaultPath_ = function() {
    // No preset given, find a good place to start.
    // Check for removable devices, if there are none, go to Downloads.
    var removableDirectoryEntry = this.rootEntries_.filter(function(rootEntry) {
      return rootEntry.fullPath == REMOVABLE_DIRECTORY;
    })[0];
    if (!removableDirectoryEntry) {
      this.changeDirectory(DOWNLOADS_DIRECTORY, CD_NO_HISTORY);
      return;
    }

    var foundRemovable = false;
    util.forEachDirEntry(removableDirectoryEntry, function(result) {
      if (result) {
        foundRemovable = true;
      } else {  // Done enumerating, and we know the answer.
        this.changeDirectory(foundRemovable ? '/' : DOWNLOADS_DIRECTORY,
                             CD_NO_HISTORY);
      }
    }.bind(this));
  };

  FileManager.prototype.setupPath_ = function(path) {
    // Split the dirname from the basename.
    var ary = path.match(/^(.*?)(?:\/([^\/]+))?$/);
    if (!ary) {
      console.warn('Unable to split default path: ' + path);
      self.changeDirectory('/', CD_NO_HISTORY);
      return;
    }

    var baseName = ary[1];
    var leafName = ary[2];

    var self = this;

    function onBaseFound(baseDirEntry) {
      if (!leafName) {
        // Default path is just a directory, cd to it and we're done.
        self.changeDirectoryEntry(baseDirEntry, CD_NO_HISTORY);
        return;
      }

      function onLeafFound(leafEntry) {
        if (leafEntry.isDirectory) {
          self.changeDirectoryEntry(leafEntry, CD_NO_HISTORY);
          return;
        }

        // Leaf is an existing file, cd to its parent directory and select it.
        self.changeDirectoryEntry(baseDirEntry, CD_NO_HISTORY, leafEntry.name);
      }

      function onLeafError(err) {
        // Set filename first so OK button will update in changeDirectoryEntry.
        self.filenameInput_.value = leafName;
        if (err = FileError.NOT_FOUND_ERR) {
          // Leaf does not exist, it's just a suggested file name.
          self.changeDirectoryEntry(baseDirEntry, CD_NO_HISTORY);
        } else {
          console.log('Unexpected error resolving default leaf: ' + err);
          self.changeDirectoryEntry('/', CD_NO_HISTORY);
        }
      }

      self.resolvePath(path, onLeafFound, onLeafError);
    }

    function onBaseError(err) {
      // Set filename first so OK button will update in changeDirectory.
      self.filenameInput_.value = leafName;
      console.log('Unexpected error resolving default base "' +
                  baseName + '": ' + err);
      self.changeDirectory('/', CD_NO_HISTORY);
    }

    if (baseName) {
      this.filesystem_.root.getDirectory(
          baseName, {create: false}, onBaseFound, onBaseError);
    } else {
      onBaseFound(this.filesystem_.root);
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
   * Cache necessary data before a sort happens.
   *
   * This is called by the table code before a sort happens, so that we can
   * go fetch data for the sort field that we may not have yet.
   */
  FileManager.prototype.prepareSort_ = function(field, callback) {
    var cacheFunction;

    if (field == 'name' || field == 'cachedMtime_') {
      // Mtime is the tie-breaker for a name sort, so we need to resolve
      // it for both mtime and name sorts.
      cacheFunction = cacheEntryDate;
    } else if (field == 'cachedSize_') {
      cacheFunction = cacheEntrySize;
    } else if (field == 'cachedIconType_') {
      cacheFunction = this.cacheEntryIconType.bind(this);
    } else {
      callback();
      return;
    }

    function checkCount() {
      if (uncachedCount == 0) {
        // Callback via a setTimeout so the sync/async semantics don't change
        // based on whether or not the value is cached.
        setTimeout(callback, 0);
      }
    }

    var dataModel = this.dataModel_;
    var uncachedCount = dataModel.length;

    for (var i = uncachedCount - 1; i >= 0 ; i--) {
      var entry = dataModel.item(i);
      if (field in entry) {
        uncachedCount--;
      } else {
        cacheFunction(entry, function() {
          uncachedCount--;
          checkCount();
        });
      }
    }

    checkCount();
  }

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
  }

  FileManager.prototype.renderThumbnail_ = function(entry) {
    var li = this.document_.createElement('li');
    li.className = 'thumbnail-item';

    if (this.showCheckboxes_)
      li.appendChild(this.renderCheckbox_(entry));

    var div = this.document_.createElement('div');
    div.className = 'img-container';
    li.appendChild(div);

    var img = this.document_.createElement('img');
    this.getThumbnailURL(entry, function(type, url) { img.src = url });
    div.appendChild(img);

    div = this.document_.createElement('div');
    div.className = 'filename-label';
    var labelText = entry.name;
    if (this.currentDirEntry_.name == '')
      labelText = this.getLabelForRootPath_(labelText);

    div.textContent = labelText;
    div.entry = entry;

    li.appendChild(div);

    cr.defineProperty(li, 'lead', cr.PropertyKind.BOOL_ATTR);
    cr.defineProperty(li, 'selected', cr.PropertyKind.BOOL_ATTR);
    return li;
  }

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
    var div = this.document_.createElement('div');
    div.className = 'detail-icon-container';

    if (this.showCheckboxes_)
      div.appendChild(this.renderCheckbox_(entry));

    var icon = this.document_.createElement('div');
    icon.className = 'detail-icon';
    entry.cachedIconType_ = this.getIconType(entry);
    icon.setAttribute('iconType', entry.cachedIconType_);
    div.appendChild(icon);

    return div;
  };

  FileManager.prototype.getLabelForRootPath_ = function(path) {
    // This hack lets us localize the top level directories.
    if (path == 'Downloads')
      return str('DOWNLOADS_DIRECTORY_LABEL');

    if (path == 'archive')
      return str('ARCHIVE_DIRECTORY_LABEL');

    if (path == 'removable')
      return str('REMOVABLE_DIRECTORY_LABEL');

    return path || str('ROOT_DIRECTORY_LABEL');
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
    label.entry = entry;
    label.className = 'detail-name filename-label';
    if (this.currentDirEntry_.name == '') {
      label.textContent = this.getLabelForRootPath_(entry.name);
    } else {
      label.textContent = entry.name;
    }

    return label;
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
    div.className = 'detail-size';

    div.textContent = '...';
    cacheEntrySize(entry, function(entry) {
      if (entry.cachedSize_ == -1) {
        div.textContent = '';
      } else {
        div.textContent = util.bytesToSi(entry.cachedSize_);
      }
    });

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
    div.className = 'detail-date';

    div.textContent = '...';

    var self = this;
    cacheEntryDate(entry, function(entry) {
      if (isSystemDirEntry(self.currentDirEntry_) &&
          entry.cachedMtime_.getTime() == 0) {
        // Mount points for FAT volumes have this time associated with them.
        // We'd rather display nothing than this bogus date.
        div.textContent = '';
      } else {
        div.textContent = self.shortDateFormatter_.format(entry.cachedMtime_);
      }
    });

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
    this.taskButtons_.innerHTML = '';

    if (!selection.indexes.length) {
      cr.dispatchSimpleEvent(this, 'selection-summarized');
      return;
    }

    var fileCount = 0;
    var byteCount = 0;
    var pendingFiles = [];

    for (var i = 0; i < selection.indexes.length; i++) {
      var entry = this.dataModel_.item(selection.indexes[i]);
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
      this.taskButtons_.innerHTML = '';
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
    this.taskButtons_.innerHTML = '';

    for (var i = 0; i < tasksList.length; i++) {
      var task = tasksList[i];

      // Tweak images, titles of internal tasks.
      var task_parts = task.taskId.split('|');
      if (task_parts[0] == this.getExtensionId_()) {
        task.internal = true;
        if (task_parts[1] == 'preview') {
          // TODO(serya): This hack needed until task.iconUrl get working
          //              (see GetFileTasksFileBrowserFunction::RunImpl).
          task.iconUrl =
              chrome.extension.getURL('images/icon_preview_16x16.png');
          task.title = str('PREVIEW_IMAGE');
        } else if (task_parts[1] == 'play') {
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
          if (str('ENABLE_ARCHIVES') != 'true')
            continue;
        } else if (task_parts[1] == 'gallery') {
          task.iconUrl =
              chrome.extension.getURL('images/icon_preview_16x16.png');
          task.title = str('GALLERY');
          if (!GALLERY_ENABLED) continue;  // Skip the button creation.
        }
      }
      this.renderTaskButton_(task);
    }

    // These are done in separate functions, as the checks require
    // asynchronous function calls.
    this.maybeRenderUnmountTask_(selection);
    this.maybeRenderFormattingTask_(selection);
  };

  FileManager.prototype.renderTaskButton_ = function(task) {
    var button = this.document_.createElement('button');
    button.addEventListener('click', this.onTaskButtonClicked_.bind(this));
    button.className = 'task-button';
    button.task = task;

    var img = this.document_.createElement('img');
    img.src = task.iconUrl;

    button.appendChild(img);
    button.appendChild(this.document_.createTextNode(task.title));

    this.taskButtons_.appendChild(button);
  };

  /**
   * Checks whether unmount task should be displayed and if the answer is
   * affirmative renders it.
   * @param {Object} selection Selected files object.
   */
  FileManager.prototype.maybeRenderUnmountTask_ = function(selection) {
    for (var index = 0; index < selection.urls.length; ++index) {
      // Each url should be a mount point.
      var path = selection.entries[index].fullPath;
      var found = false;
      for (var i = 0; i < this.mountPoints_.length; i++) {
        var mountPath = this.mountPoints_[i].mountPath;
        if (mountPath[0] != '/') {
          mountPath = '/' + mountPath;
        }
        if (mountPath == path && this.mountPoints_[i].mountType == 'file') {
          found = true;
          break;
        }
      }
      if (!found)
        return;
    }
    this.renderTaskButton_({
        taskId: this.getExtensionId_() + '|unmount-archive',
        iconUrl:
            chrome.extension.getURL('images/icon_unmount_archive_16x16.png'),
        title: str('UNMOUNT_ARCHIVE')
    });
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
            title: str('FORMAT_DEVICE')
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

  FileManager.prototype.onTaskButtonClicked_ = function(event) {
    var task = event.srcElement.task;
    if (task.internal) {
      // For internal tasks call the handler directly to avoid being handled
      // multiple times.
      var taskId = task.taskId.split('|')[1];
      this.onFileTaskExecute_(taskId, {entries: this.selection.entries});
      return;
    }
    chrome.fileBrowserPrivate.executeTask(task.taskId,
                                          this.selection.urls);
  };

  /**
   * Event handler called when some volume was mounted or unmouted.
   */
  FileManager.prototype.onMountCompleted_ = function(event) {
    var self = this;
    chrome.fileBrowserPrivate.getMountPoints(function(mountPoints) {
      self.mountPoints_ = mountPoints;
      if (event.eventType == 'mount') {
        for (var index = 0; index < self.mountRequests_.length; ++index) {
          if (self.mountRequests_[index] == event.sourceUrl) {
            self.mountRequests_.splice(index, 1);
            if (event.status == 'success') {
              self.changeDirectory(event.mountPath);
            } else {
              // Report mount error.
              if (event.mountType == 'file') {
                var fileName = event.sourceUrl.substr(
                    event.sourceUrl.lastIndexOf('/') + 1);
                self.alert.show(strf('ARCHIVE_MOUNT_FAILED', fileName,
                                     event.status));
              }
            }
            return;
          }
        }
      }

      if (event.eventType == 'unmount' && event.status == 'success' &&
          self.currentDirEntry_ &&
          isParentPath(event.mountPath, self.currentDirEntry_.fullPath)) {
        self.changeDirectory(getParentPath(event.mountPath));
        return;
      }

      var rescanDirectoryNeeded = (event.status == 'success');
      for (var i = 0; i < mountPoints.length; i++) {
        if (event.sourceUrl == mountPoints[i].sourceUrl &&
            mountPoints[i].mountCondition != '') {
          rescanDirectoryNeeded = true;
        }
      }
      // TODO(dgozman): rescan directory, only if it contains mounted points,
      // when mounts location will be decided.
      if (rescanDirectoryNeeded)
        self.rescanDirectory_(null, 300);
    });
  };

  /**
   * Event handler called when some internal task should be executed.
   */
  FileManager.prototype.onFileTaskExecute_ = function(id, details) {
    var urls = details.entries.map(function(entry) {
        return entry.toURL();
    });
    if (id == 'preview') {
      g_slideshow_data = urls;
      chrome.tabs.create({url: "slideshow.html"});
    } else if (id == 'play' || id == 'enqueue') {
      chrome.fileBrowserPrivate.viewFiles(urls, id);
    } else if (id == 'mount-archive') {
      for (var index = 0; index < urls.length; ++index) {
        this.mountRequests_.push(urls[index]);
        chrome.fileBrowserPrivate.addMount(urls[index], 'file', {});
      }
    } else if (id == 'unmount-archive') {
      for (var index = 0; index < urls.length; ++index) {
        chrome.fileBrowserPrivate.removeMount(urls[index]);
      }
    } else if (id == 'format-device') {
      this.confirm.show(str('FORMATTING_WARNING'), function() {
        chrome.fileBrowserPrivate.formatDevice(urls[0]);
      });
    } else if (id == 'gallery') {
      this.openGallery_(urls);
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

  FileManager.prototype.openGallery_ = function(urls) {
    var self = this;

    var galleryFrame = this.document_.createElement('iframe');
    galleryFrame.className = 'overlay-pane';
    galleryFrame.scrolling = 'no';

    galleryFrame.onload = function() {
      galleryFrame.contentWindow.Gallery.open(
          self.currentDirEntry_,
          urls,
          function () {
            // TODO(kaznacheev): keep selection.
            self.rescanDirectoryNow_();  // Make sure new files show up.
            self.dialogDom_.removeChild(galleryFrame);
          },
          self.metadataProvider_);
    };

    galleryFrame.src = 'js/image_editor/gallery.html';
    this.dialogDom_.appendChild(galleryFrame);
  };

  /**
   * Update the breadcrumb display to reflect the current directory.
   */
  FileManager.prototype.updateBreadcrumbs_ = function() {
    var bc = this.dialogDom_.querySelector('.breadcrumbs');
    bc.innerHTML = '';

    var fullPath = this.currentDirEntry_.fullPath.replace(/\/$/, '');
    var pathNames = fullPath.split('/');
    var path = '';

    for (var i = 0; i < pathNames.length; i++) {
      var pathName = pathNames[i];
      path += pathName + '/';

      var div = this.document_.createElement('div');
      div.className = 'breadcrumb-path';
      if (i <= 1) {
        // i == 0: root directory itself, i == 1: the files it contains.
        div.textContent = this.getLabelForRootPath_(pathName);
      } else {
        div.textContent = pathName;
      }

      div.path = path;
      div.addEventListener('click', this.onBreadcrumbClick_.bind(this));

      bc.appendChild(div);

      if (i == pathNames.length - 1) {
        div.classList.add('breadcrumb-last');
      } else {
        var spacer = this.document_.createElement('div');
        spacer.className = 'breadcrumb-spacer';
        spacer.textContent = RIGHT_TRIANGLE;
        bc.appendChild(spacer);
      }
    }
  };

  /**
   * Update the preview panel to display a given entry.
   *
   * The selection summary line is handled by the onSelectionSummarized handler
   * rather than this function, because summarization may not complete quickly.
   */
  FileManager.prototype.updatePreview_ = function() {
    // Clear the preview image first, in case the thumbnail takes long to load.
    // Do not set url to empty string in plugins because it crashes browser,
    // instead we use empty 1x1 gif
    this.previewImage_.src = EMPTY_IMAGE_URI;

    // Also clear description in case metadata takes some time to load
    this.clearDescription_();

    // The thumbnail class styles the preview for image thumbnails, which are
    // treated a little differently than the stock icons for non-thumbnail
    // preview images.
    this.previewImage_.classList.remove('thumbnail');

    // The multiple-selected class indicates that more than one entry is
    // selcted.
    this.previewImage_.classList.remove('multiple-selected');

    var leadEntry = this.getLeadEntry();
    if (!leadEntry) {
      this.previewFilename_.textContent = '';
      return;
    }
    var previewName = leadEntry.name;
    if (this.currentDirEntry_.name == '')
      previewName = this.getLabelForRootPath_(previewName);

    this.previewFilename_.textContent = previewName;

    var iconType = this.getIconType(leadEntry);
    if (iconType == 'image') {
      if (fileManager.selection.totalCount > 1)
        this.previewImage_.classList.add('multiple-selected');
    }

    var self = this;

    this.getThumbnailURL(leadEntry, function(iconType, url) {
      if (self.getLeadEntry() != leadEntry) {
        // Selection has changed since we asked, nevermind.
        return;
      }

      if (url) {
        self.previewImage_.src = url;
        if (iconType == 'image')
          self.previewImage_.classList.add('thumbnail');
      } else {
        self.previewImage_.src = previewArt['unknown'];
      }
    });

    this.getDescription(leadEntry, function(desc) {
      self.clearDescription_();

      if (self.getLeadEntry() != leadEntry) {
        return;
      }

      if (desc) {

        var descriptionBody =
            self.descriptionTable_.ownerDocument.createElement('tbody');

        var doc = self.descriptionTable_.ownerDocument;

        for (var key in desc) {
          descriptionBody.appendChild(
            util.createElement(doc, 'tr',
                util.createElement(doc, 'th', str(desc[key].key)),
                util.createElement(doc, 'td',
                                   self.formatMetadataValue_(desc[key]))
            )
          );
        }

        self.descriptionTable_.appendChild(descriptionBody);
      }
    });
  };

  FileManager.prototype.getLeadEntry = function() {
    var leadIndex = this.currentList_.selectionModel.leadIndex;
    if (leadIndex < 0)
      return null;

    return this.dataModel_.item(leadIndex);
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


  FileManager.prototype.clearDescription_ = function() {
    while (this.descriptionTable_.hasChildNodes()) {
      this.descriptionTable_.removeChild(this.descriptionTable_.firstChild);
    }
  };

  FileManager.prototype.setPreviewMetadata = function(metadata) {
    this.previewMetadata_.textContent = '';
    if (!(metadata && metadata.ifd))
      return;

    var self = this;

    function addProperty(id, v) {
      var dom = self.document_.createElement('div');
      dom.className = 'metadata-item';
      var label = self.document_.createElement('div');
      label.className = 'metadata-label';
      label.textContent = str(id) || id;
      dom.appendChild(label);
      var value = self.document_.createElement('div');
      value.className = 'metadata-value';
      value.textContent = v;
      dom.appendChild(value);

      self.previewMetadata_.appendChild(dom);
    }

    // TODO(rginda): Split this function into metadata specific routines when
    // we add new metadata types.
    // TODO(rginda): Add const names for these numerics.
    var exifDir = metadata.ifd.exif;
    if (0xa002 in exifDir && 0xa003 in exifDir) {
      addProperty('DIMENSIONS_LABEL',
                  strf('DIMENSIONS_FORMAT', exifDir[0xa002].value,
                       exifDir[0xa003].value));
    }
  };

  FileManager.prototype.getThumbnailURL = function(entry, callback) {
    if (!entry)
      return;

    var iconType = this.getIconType(entry);

    this.metadataProvider_.fetch(entry.toURL(), function (metadata) {
      var url = metadata.thumbnailURL;
      if (!url) {
        if (iconType == 'image') {
          url = entry.toURL();
        } else {
          url = previewArt[iconType] || previewArt['unknown'];
        }
      }

      callback(iconType, url);
    });
  };

  FileManager.prototype.getDescription = function(entry, callback) {
    if (!entry)
      return;

    this.metadataProvider_.fetch(entry.toURL(), function(metadata) {
      callback(metadata.description);
    });
  };

  FileManager.prototype.selectEntry = function(name) {
    for (var i = 0; i < this.dataModel_.length; i++) {
      if (this.dataModel_.item(i).name == name) {
        this.currentList_.selectionModel.selectedIndex = i;
        this.currentList_.scrollIndexIntoView(i);
        this.currentList_.focus();
        return;
      }
    }
  };

  /**
   * Add the file/directory with given name to the current selection.
   *
   * @param {string} name The name of the entry to select.
   * @return {boolean} Whether entry exists.
   */
  FileManager.prototype.addItemToSelection = function(name) {
    var entryExists = false;
    for (var i = 0; i < this.dataModel_.length; i++) {
      if (this.dataModel_.item(i).name == name) {
        this.currentList_.selectionModel.setIndexSelected(i, true);
        this.currentList_.scrollIndexIntoView(i);
        this.currentList_.focus();
        entryExists = true;
        break;
      }
    }
    return entryExists;
  }

  /**
   * Return the name of the entries in the current directory
   *
   * @return {object} Array of entry names.
   */
  FileManager.prototype.listDirectory = function() {
    var list = []
    for (var i = 0; i < this.dataModel_.length; i++) {
      list.push(this.dataModel_.item(i).name);
    }
    return list;
  }

  /**
   * Open the item selected
   */
  FileManager.prototype.doOpen = function() {
    switch (this.dialogType_) {
      case FileManager.DialogType.SELECT_FOLDER:
      case FileManager.DialogType.SELECT_OPEN_FILE:
      case FileManager.DialogType.SELECT_OPEN_MULTI_FILE:
        this.onOk_();
        break;
      default:
        throw new Error('Cannot open an item in this dialog type.');
    }
  }

  /**
   * Save the item using the given name
   *
   * @param {string} name The name given to item to be saved
   */
  FileManager.prototype.doSaveAs = function(name) {
    if (this.dialogType_ == FileManager.DialogType.SELECT_SAVEAS_FILE) {
      this.filenameInput_.value = name;
      this.onOk_();
    }
    else {
      throw new Error('Cannot save an item in this dialog type.');
    }
  }

  /**
   * Return full path of the current directory
   */
  FileManager.prototype.getCurrentDirectory = function() {
    return this.currentDirEntry_.fullPath;
  }

  /**
   * Used by tests to wait before interacting with the file maanager
   */
  FileManager.prototype.isInitialized = function() {
    var initialized =  (this.workerInitialized_ != null) &&
        (this.directoryChanged_ != null);
    return initialized;
  }

  /**
   * Change the current directory to the directory represented by a
   * DirectoryEntry.
   *
   * Dispatches the 'directory-changed' event when the directory is successfully
   * changed.
   *
   * @param {string} path The absolute path to the new directory.
   * @param {bool} opt_saveHistory Save this in the history stack (defaults
   *     to true).
   * @param {string} opt_selectedEntry The name of the file to select after
   *     changing directories.
   */
  FileManager.prototype.changeDirectoryEntry = function(dirEntry,
                                                        opt_saveHistory,
                                                        opt_selectedEntry,
                                                        opt_callback) {
    if (typeof opt_saveHistory == 'undefined') {
      opt_saveHistory = true;
    } else {
      opt_saveHistory = !!opt_saveHistory;
    }

    var location = '#' + encodeURI(dirEntry.fullPath);
    if (opt_saveHistory) {
      history.pushState(undefined, dirEntry.fullPath, location);
    } else if (window.location.hash != location) {
      // If the user typed URL manually that is not canonical it would be fixed
      // here. However it seems history.replaceState doesn't work properly
      // with rewritable URLs (while does with history.pushState). It changes
      // window.location but doesn't change content of the ombibox.
      history.replaceState(undefined, dirEntry.fullPath, location);
    }

    if (this.currentDirEntry_ &&
        this.currentDirEntry_.fullPath == dirEntry.fullPath) {
      // Directory didn't actually change.
      if (opt_selectedEntry)
        this.selectEntry(opt_selectedEntry);
      return;
    }

    var e = new cr.Event('directory-changed');
    e.previousDirEntry = this.currentDirEntry_;
    e.newDirEntry = dirEntry;
    e.saveHistory = opt_saveHistory;
    e.selectedEntry = opt_selectedEntry;
    e.opt_callback = opt_callback;
    this.currentDirEntry_ = dirEntry;
    this.dispatchEvent(e);
  }

  /**
   * Change the current directory to the directory represented by a string
   * path.
   *
   * Dispatches the 'directory-changed' event when the directory is successfully
   * changed.
   *
   * @param {string} path The absolute path to the new directory.
   * @param {bool} opt_saveHistory Save this in the history stack (defaults
   *     to true).
   * @param {string} opt_selectedEntry The name of the file to select after
   *     changing directories.
   */
  FileManager.prototype.changeDirectory = function(path,
                                                   opt_saveHistory,
                                                   opt_selectedEntry,
                                                   opt_callback) {
    if (path == '/')
      return this.changeDirectoryEntry(this.filesystem_.root,
                                       opt_saveHistory,
                                       opt_selectedEntry,
                                       opt_callback);

    var self = this;

    this.filesystem_.root.getDirectory(
        path, {create: false},
        function(dirEntry) {
          self.changeDirectoryEntry(
              dirEntry, opt_saveHistory, opt_selectedEntry, opt_callback);
        },
        function(err) {
          console.error('Error changing directory to: ' + path + ', ' + err);
          if (self.currentDirEntry_) {
            var location = '#' + encodeURI(self.currentDirEntry_.fullPath);
            history.replaceState(undefined,
                                 self.currentDirEntry_.fullPath,
                                 location);
          } else {
            // If we've never successfully changed to a directory, force them
            // to the root.
            self.changeDirectory('/', false);
          }
        });
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

      this.confirm.show(msg,
                        function() { self.deleteEntries(entries, true); });
      return;
    }

    var count = entries.length;

    var self = this;
    function onDelete() {
      if (--count == 0)
        self.rescanDirectory_(function() {
          if (opt_callback)
            opt_callback();
        });
    }

    for (var i = 0; i < entries.length; i++) {
      var entry = entries[i];
      if (entry.isFile) {
        entry.remove(
            onDelete,
            util.flog('Error deleting file: ' + entry.fullPath, onDelete));
      } else {
        entry.removeRecursively(
            onDelete,
            util.flog('Error deleting folder: ' + entry.fullPath, onDelete));
      }
    }
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
      sourceDirEntry: this.currentDirEntry_,
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
      sourceDirEntry: this.currentDirEntry_,
      entries: [].concat(this.selection.entries)
    };

    this.updateCommands_();
    this.showButter(str('SELECTION_CUT'));
  };

  /**
   * Returns true if the local disk is low on available space.
   *
   * TODO(rginda): This is hardcoded to return false now, needs to be hooked
   * up for real after tbarzac's change to report disk space lands.
   */
  FileManager.prototype.isLocalDiskSpaceLow = function() {
    return false;
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
                                this.currentDirEntry_,
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
      text = strf('ONE_FILE_SELECTED', bytes);
    } else if (selection.fileCount == 0 && selection.directoryCount == 1) {
      text = str('ONE_DIRECTORY_SELECTED');
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
    this.changeDirectory(event.srcElement.path);
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
          this.selection.entries[0].isFile)
        this.filenameInput_.value = leadEntry.name;
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
  };

  /**
   * Update the UI then the lead item changes.
   *
   * @param {cr.Event} event The change event.
   */
  FileManager.prototype.onLeadIndexChanged_ = function(event) {
    this.updatePreview_();
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
      if (isSystemDirEntry(this.currentDirEntry_)) {
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
    if (this.renameInput_.currentEntry) {
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
      return this.changeDirectory(entry.fullPath);
    }
  }

  /**
   * Update the UI when the current directory changes.
   *
   * @param {cr.Event} event The directory-changed event.
   */
  FileManager.prototype.onDirectoryChanged_ = function(event) {
    if (this.currentDirEntry_.fullPath.substr(0, DOWNLOADS_DIRECTORY.length) ==
        DOWNLOADS_DIRECTORY &&
        this.isLocalDiskSpaceLow()) {
      if (this.downloadsWarning_.style.height != DOWNLOADS_WARNING_HEIGHT) {
        // Current path starts with DOWNLOADS_DIRECTORY, show the warning.
        this.downloadsWarning_.style.height = DOWNLOADS_WARNING_HEIGHT;
        this.requestResize_(100);
      }
    } else {
      if (this.downloadsWarning_.style.height != '0') {
        this.downloadsWarning_.style.height = '0';
        this.requestResize_(100);
      }
    }

    this.updateCommands_();
    this.updateOkButton_();

    // New folder should never be enabled in the root or media/ directories.
    this.newFolderButton_.disabled = isSystemDirEntry(this.currentDirEntry_);

    this.document_.title = this.currentDirEntry_.fullPath;

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

    this.rescanDirectory_(function() {
        if (event.selectedEntry)
          self.selectEntry(event.selectedEntry);
        if (event.opt_callback) {
          try {
            event.opt_callback();
          } catch (ex) {
            console.error('Caught exception while inovking callback: ', ex);
          }
        }
        // For tests that open the dialog to empty directories, everything
        // is loaded at this point.
        chrome.test.sendMessage('directory-change-complete');
        // PyAuto tests monitor this state by polling this variable
        self.directoryChanged_ = true;
      });
  };

  /**
   * Rescans directory later.
   * This method should be used if we just want rescan but not actually now.
   * This helps us not to flood queue with rescan requests.
   *
   * @param opt_callback
   * @param opt_onError
   */
   FileManager.prototype.rescanDirectoryLater_ = function(opt_callback,
                                                          opt_onError) {
    // It might be massive change, so let's note somehow, that we need
    // rescanning and then wait some time

    if (this.pendingRescanQueue_.length == 0) {
      this.pendingRescanQueue_.push({onSuccess:opt_callback,
                                     onError:opt_onError});

      // If rescan isn't going to run without
      // our interruption, then say that we need to run it
      if (!this.rescanRunning_) {
        setTimeout(this.rescanDirectory_.bind(this),
            SIMULTANEOUS_RESCAN_INTERVAL);
      }
    }
   }


  /**
   * Rescans the current directory, refreshing the list. It decreases the
   * probability that two such calls are pending simultaneously.
   *
   * This method tries to queue request if rescan is already running, and
   * processes this request later. Anyway callback would be called after
   * processing.
   *
   * If no rescan is running, then method starts rescanning immediately.
   *
   * @param {function()} opt_callback Optional function to invoke when the
   *     rescan is complete.
   *
   * @param {function()} opt_onError Optional function to invoke when the
   *     rescan fails.
   */
  FileManager.prototype.rescanDirectory_ = function(opt_callback, opt_onError) {
    // Updated when a user clicks on the label of a file, used to detect
    // when a click is eligible to trigger a rename.  Can be null, or
    // an object with 'path' and 'date' properties.
    this.lastLabelClick_ = null;

    // Clear the table first.
    this.dataModel_.splice(0, this.dataModel_.length);
    this.currentList_.selectionModel.clear();

    this.updateBreadcrumbs_();

    if (this.currentDirEntry_.fullPath != '/') {
      // Add current request to pending result list
      this.pendingRescanQueue_.push({
            onSuccess:opt_callback,
            onError:opt_onError
          });

      if (this.rescanRunning_)
        return;

      this.rescanRunning_ = true;

      // The current list of callbacks is saved and reset.  Subsequent
      // calls to rescanDirectory_ while we're still pending will be
      // saved and will cause an additional rescan to happen after a delay.
      var callbacks = this.pendingRescanQueue_;

      this.pendingRescanQueue_ = [];

      var self = this;
      var reader;

      function onError() {
        if (self.pendingRescanQueue_.length > 0) {
          setTimeout(self.rescanDirectory_.bind(self),
              SIMULTANEOUS_RESCAN_INTERVAL);
        }

        self.rescanRunning_ = false;

        for (var i= 0; i < callbacks.length; i++) {
          if (callbacks[i].onError)
            try {
              callbacks[i].onError();
            } catch (ex) {
              console.error('Caught exception while notifying about error: ' +
                          name, ex);
            }
        }
      }

      function onReadSome(entries) {
        if (entries.length == 0) {
          if (self.pendingRescanQueue_.length > 0) {
            setTimeout(self.rescanDirectory_.bind(self),
                SIMULTANEOUS_RESCAN_INTERVAL);
          }

          self.rescanRunning_ = false;
          for (var i= 0; i < callbacks.length; i++) {
            if (callbacks[i].onSuccess)
              try {
                callbacks[i].onSuccess();
              } catch (ex) {
                console.error('Caught exception while notifying about error: ' +
                            name, ex);
              }
          }

          return;
        }

        // Splice takes the to-be-spliced-in array as individual parameters,
        // rather than as an array, so we need to perform some acrobatics...
        var spliceArgs = [].slice.call(entries);

        // Hide files that start with a dot ('.').
        // TODO(rginda): User should be able to override this. Support for other
        // commonly hidden patterns might be nice too.
        if (self.filterFiles_) {
          spliceArgs = spliceArgs.filter(function(e) {
              return e.name.substr(0, 1) != '.';
            });
        }

        spliceArgs.unshift(0, 0);  // index, deleteCount
        self.dataModel_.splice.apply(self.dataModel_, spliceArgs);

        // Keep reading until entries.length is 0.
        reader.readEntries(onReadSome, onError);
      };

      // If not the root directory, just read the contents.
      reader = this.currentDirEntry_.createReader();
      reader.readEntries(onReadSome, onError);
      return;
    }

    // Otherwise, use the provided list of root subdirectories, since the
    // real local filesystem root directory (the one we use outside the
    // harness) can't be enumerated yet.
    var spliceArgs = [].slice.call(this.rootEntries_);
    spliceArgs.unshift(0, 0);  // index, deleteCount
    this.dataModel_.splice.apply(this.dataModel_, spliceArgs);

    if (opt_callback)
      opt_callback();
  };

  FileManager.prototype.findListItem_ = function(event) {
    var node = event.srcElement;
    while (node) {
      if (node.tagName == 'LI')
        break;
      node = node.parentNode;
    }

    return node;
  };

  FileManager.prototype.onGridMouseDown_ = function(event) {
    this.updateCommands_();

    if (this.allowRenameClick_(event, event.srcElement.parentNode)) {
      event.preventDefault();
      this.initiateRename_(event.srcElement);
    }

    if (event.button != 1)
      return;

    var li = this.findListItem_(event);
    if (!li)
      return;
  };

  FileManager.prototype.onTableMouseDown_ = function(event) {
    this.updateCommands_();

    if (this.allowRenameClick_(event,
                               event.srcElement.parentNode.parentNode)) {
      event.preventDefault();
      this.initiateRename_(event.srcElement);
    }

    if (event.button != 1)
      return;

    var li = this.findListItem_(event);
    if (!li) {
      console.log('li not found', event);
      return;
    }
  };

  FileManager.prototype.onUnload_ = function(event) {
    if (this.subscribedOnDirectoryChanges_) {
      chrome.fileBrowserPrivate.removeFileWatch(event.previousDirEntry.toURL(),
          function(result) {
            if (!result) {
              console.log('Failed to remove file watch');
            }
          });
    }
  };

  FileManager.prototype.onFileChanged_ = function(event) {
    // We receive a lot of events even in folders we are not interested in.
    if (event.fileUrl == this.currentDirEntry_.toURL())
      this.rescanDirectoryLater_();
  };

  /**
   * Determine whether or not a click should initiate a rename.
   *
   * Renames can happen on mouse click if the user clicks on a label twice,
   * at least a half second apart.
   */
  FileManager.prototype.allowRenameClick_ = function(event, row) {
    if (this.dialogType_ != FileManager.DialogType.FULL_PAGE ||
        this.currentDirEntry_.name == '' ||
        isSystemDirEntry(this.currentDirEntry_)) {
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
    var path = event.srcElement.entry.fullPath;
    var lastLabelClick = this.lastLabelClick_;
    this.lastLabelClick_ = {path: path, date: now};

    // Rename already in progress.
    if (this.renameInput_.currentEntry)
      return false;

    if (lastLabelClick && lastLabelClick.path == path) {
      var delay = now - lastLabelClick.date;
      if (delay > 500 && delay < 2000) {
        this.lastLabelClick_ = null;
        return true;
      }
    }

    return false;
  };

  FileManager.prototype.initiateRename_= function(label) {
    var input = this.renameInput_;

    input.value = label.textContent;
    input.style.top = label.offsetTop + 'px';
    input.style.left = label.offsetLeft + 'px';
    input.style.width = label.clientWidth + 'px';
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
    input.currentEntry = label.entry;
  };

  FileManager.prototype.onRenameInputKeyDown_ = function(event) {
    if (!this.renameInput_.currentEntry)
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
    if (this.renameInput_.currentEntry)
      this.cancelRename_();
  };

  FileManager.prototype.renameEntry = function(entry, newName, opt_callback) {
    var self = this;
    function onSuccess() {
      self.rescanDirectory_(function() {
        self.selectEntry(newName);
        if (opt_callback)
          opt_callback();
      });
    }

    function onError(err) {
      self.alert.show(strf('ERROR_RENAMING', entry.name,
                           util.getFileErrorMnemonic(err.code)));
    }

    function resolveCallback(victim) {
      if (victim instanceof FileError) {
        entry.moveTo(self.currentDirEntry_, newName, onSuccess, onError);
      } else {
        var message = victim.isFile ?
            'FILE_ALREADY_EXISTS':
            'DIRECTORY_ALREADY_EXISTS';
        self.alert.show(strf(message, newName));
      }
    }

    this.resolvePath(this.currentDirEntry_.fullPath + '/' + newName,
        resolveCallback, resolveCallback);
  };

  FileManager.prototype.commitRename_ = function() {
    var entry = this.renameInput_.currentEntry;
    var newName = this.renameInput_.value;
    if (!this.validateFileName_(newName))
      return;

    this.renameInput_.currentEntry = null;
    this.lastLabelClick_ = null;

    if (this.renameInput_.parentNode)
      this.renameInput_.parentNode.removeChild(this.renameInput_);

    this.refocus();
    this.renameEntry(entry, newName);
  };

  FileManager.prototype.cancelRename_ = function(event) {
    this.renameInput_.currentEntry = null;

    if (this.renameInput_.parentNode)
      this.renameInput_.parentNode.removeChild(this.renameInput_);

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

  FileManager.prototype.onNewFolderButtonClick_ = function(event) {
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
    var self = this;

    function onSuccess(dirEntry) {
      self.rescanDirectory_(function() {
        self.selectEntry(name);
        if (opt_callback)
          opt_callback();
      });
    }

    function onError(err) {
      self.alert.show(strf('ERROR_CREATING_FOLDER', name,
                           util.getFileErrorMnemonic(err.code)));
    }

    this.currentDirEntry_.getDirectory(name, {create: true, exclusive: true},
                                       onSuccess, onError);
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
    if (event.srcElement.tagName == 'INPUT') {
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
          this.rescanDirectory_();
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

    //console.log(event.keyCode);

    switch (event.keyCode) {
      case 8:  // Backspace => Up one directory.
        event.preventDefault();
        var path = this.currentDirEntry_.fullPath;
        if (path && path != '/') {
          var path = path.replace(/\/[^\/]+$/, '');
          this.changeDirectory(path || '/');
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
        if (this.newFolderButton_.style.display != 'none' && event.ctrlKey) {
          event.preventDefault();
          this.onNewFolderButtonClick_();
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
        if (this.dialogType_ == FileManager.DialogType.FULL_PAGE &&
            this.selection && this.selection.totalCount > 0 &&
            !isSystemDirEntry(this.currentDirEntry_)) {
          event.preventDefault();
          this.deleteEntries(this.selection.entries);
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

    for (var index = 0; index < this.dataModel_.length; ++index) {
      var name = this.dataModel_.item(index).name;
      if (name.substring(0, text.length).toLowerCase() == text) {
        this.currentList_.selectionModel.selectedIndexes = [index];
        return;
      }
    }

    this.textSearchState_.text = '';
  };

  /**
   * Handle a click of the cancel button.  Closes the window.
   *
   * @param {Event} event The click event.
   */
  FileManager.prototype.onCancel_ = function(event) {
    // Closes the window and does not return.
    chrome.fileBrowserPrivate.cancelDialog();
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
    var currentDirUrl = this.currentDirEntry_.toURL();

    if (currentDirUrl.charAt(currentDirUrl.length - 1) != '/')
      currentDirUrl += '/';

    if (this.dialogType_ == FileManager.DialogType.SELECT_SAVEAS_FILE) {
      // Save-as doesn't require a valid selection from the list, since
      // we're going to take the filename from the text input.
      var filename = this.filenameInput_.value;
      if (!filename)
        throw new Error('Missing filename!');
      if (!this.validateFileName_(filename))
        return;

      var self = this;
      function resolveCallback(victim) {
        if (victim instanceof FileError) {
          // File does not exist.  (NB: selectFile Closes the window and does
          // not return.)
          chrome.fileBrowserPrivate.selectFile(
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
                              chrome.fileBrowserPrivate.selectFile(
                                  currentDirUrl + encodeURIComponent(filename),
                                  self.getSelectedFilterIndex_(filename));
                            });
        }
        return;
      }

      this.resolvePath(this.currentDirEntry_.fullPath + '/' + filename,
          resolveCallback, resolveCallback);
      return;
    }

    var ary = [];
    var selectedIndexes = this.currentList_.selectionModel.selectedIndexes;

    // All other dialog types require at least one selected list item.
    // The logic to control whether or not the ok button is enabled should
    // prevent us from ever getting here, but we sanity check to be sure.
    if (!selectedIndexes.length)
      throw new Error('Nothing selected!');

    for (var i = 0; i < selectedIndexes.length; i++) {
      var entry = this.dataModel_.item(selectedIndexes[i]);
      if (!entry) {
        console.log('Error locating selected file at index: ' + i);
        continue;
      }

      ary.push(currentDirUrl + encodeURIComponent(entry.name));
    }

    // Multi-file selection has no other restrictions.
    if (this.dialogType_ == FileManager.DialogType.SELECT_OPEN_MULTI_FILE) {
      // Closes the window and does not return.
      chrome.fileBrowserPrivate.selectFiles(ary);
      return;
    }

    // In full screen mode, open all files for vieweing.
    if (this.dialogType_ == FileManager.DialogType.FULL_PAGE) {
      chrome.fileBrowserPrivate.viewFiles(ary, "default");
      // Window stays open.
      return;
    }

    // Everything else must have exactly one.
    if (ary.length > 1)
      throw new Error('Too many files selected!');

    var selectedEntry = this.dataModel_.item(selectedIndexes[0]);

    if (this.dialogType_ == FileManager.DialogType.SELECT_FOLDER) {
      if (!selectedEntry.isDirectory)
        throw new Error('Selected entry is not a folder!');
    } else if (this.dialogType_ == FileManager.DialogType.SELECT_OPEN_FILE) {
      if (!selectedEntry.isFile)
        throw new Error('Selected entry is not a file!');
    }

    // Closes the window and does not return.
    chrome.fileBrowserPrivate.selectFile(
        ary[0], this.getSelectedFilterIndex_(ary[0]));
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
})();

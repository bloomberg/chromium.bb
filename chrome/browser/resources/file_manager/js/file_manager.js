// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * FileManager constructor.
 *
 * FileManager objects encapsulate the functionality of the file selector
 * dialogs, as well as the full screen file manager application (though the
 * latter is not yet implemented).
 *
 * @constructor
 * @param {HTMLElement} dialogDom The DOM node containing the prototypical
 *     dialog UI.
 */
function FileManager(dialogDom) {
  this.dialogDom_ = dialogDom;
  this.filesystem_ = null;
  this.params_ = location.search ?
                 JSON.parse(decodeURIComponent(location.search.substr(1))) :
                 {};
  if (this.params_.defaultPath && this.params_.defaultPath.indexOf('/') != 0)
    this.params_.defaultPath = '/' + this.params_.defaultPath;

  this.listType_ = null;
  this.showDelayTimeout_ = null;

  this.selection = null;

  this.butterTimer_ = null;
  this.currentButter_ = null;
  this.butterLastShowTime_ = 0;

  this.filesystemObserverId_ = null;
  this.gdataObserverId_ = null;

  this.commands_ = {};

  this.document_ = dialogDom.ownerDocument;
  this.dialogType_ = this.params_.type || FileManager.DialogType.FULL_PAGE;

  metrics.recordEnum('Create', this.dialogType_,
      [FileManager.DialogType.SELECT_FOLDER,
      FileManager.DialogType.SELECT_SAVEAS_FILE,
      FileManager.DialogType.SELECT_OPEN_FILE,
      FileManager.DialogType.SELECT_OPEN_MULTI_FILE,
      FileManager.DialogType.FULL_PAGE]);

  this.initFileSystem_();
  this.volumeManager_ = VolumeManager.getInstance();
  this.initDom_();
  this.initDialogType_();
}

/**
 * Maximum delay in milliseconds for updating thumbnails in the bottom panel
 * to mitigate flickering. If images load faster then the delay they replace
 * old images smoothly. On the other hand we don't want to keep old images
 * too long.
 */
FileManager.THUMBNAIL_SHOW_DELAY = 100;

FileManager.prototype = {
  __proto__: cr.EventTarget.prototype
};

// Anonymous "namespace".
(function() {

  // Private variables and helper functions.

  /**
   * Location of the FAQ about the downloads directory.
   */
  var DOWNLOADS_FAQ_URL = 'http://www.google.com/support/chromeos/bin/' +
      'answer.py?hl=en&answer=1061547';

  /**
   * Location of the FAQ about Google Drive.
   */
  var GOOGLE_DRIVE_FAQ_URL =
      'https://support.google.com/chromeos/?hl=en&p=filemanager_drive';

  /**
   * Location of the page to buy more storage for Google Drive.
   */
  var GOOGLE_DRIVE_BUY_STORAGE =
      'https://www.google.com/settings/storage';

  /**
   * Location of the help page about connecting to Google Drive.
   */
  var GOOGLE_DRIVE_ERROR_HELP_URL =
      'https://support.google.com/chromeos/?p=filemanager_driveerror';

  /**
  * Location of the FAQ about the file actions.
  */
  var NO_ACTION_FOR_FILE_URL = 'http://support.google.com/chromeos/bin/' +
      'answer.py?hl=en&answer=1700055&topic=29026&ctx=topic';

  /**
   * Maximum amount of thumbnails in the preview pane.
   */
  var MAX_PREVIEW_THUMBAIL_COUNT = 4;

  /**
   * Maximum width or height of an image what pops up when the mouse hovers
   * thumbnail in the bottom panel (in pixels).
   */
  var IMAGE_HOVER_PREVIEW_SIZE = 200;

  /**
   * The minimum about of time to display the butter bar for, in ms.
   * Justification is 1000ms for minimum display time plus 300ms for transition
   * duration.
   */
  var MINIMUM_BUTTER_DISPLAY_TIME_MS = 1300;

  /**
   * Number of milliseconds in a day.
   */
  var MILLISECONDS_IN_DAY = 24 * 60 * 60 * 1000;

  /**
   * Item for the Grid View.
   * @param {FileManager} fileManager FileManager instance.
   * @param {boolean} showCheckbox True if select checkbox should be visible
   * @param {Entry} entry File entry.
   * @constructor
   */
  function GridItem(fileManager, showCheckbox, entry) {
    var li = fileManager.document_.createElement('li');
    GridItem.decorate(li, fileManager, showCheckbox, entry);
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

  /**
   * @param {Element} li List item element.
   * @param {FileManager} fileManager FileManager instance.
   * @param {boolean} showCheckbox True if select checkbox should be visible
   * @param {Entry} entry File entry.
   */
  GridItem.decorate = function(li, fileManager, showCheckbox, entry) {
    li.__proto__ = GridItem.prototype;
    fileManager.decorateThumbnail_(li, showCheckbox, entry);
  };

  /**
   * Return a translated string.
   *
   * Wrapper function to make dealing with translated strings more concise.
   * Equivalent to loadTimeData.getString(id).
   *
   * @param {string} id The id of the string to return.
   * @return {string} The translated string.
   */
  function str(id) {
    return loadTimeData.getString(id);
  }

  /**
   * Return a translated string with arguments replaced.
   *
   * Wrapper function to make dealing with translated strings more concise.
   * Equivilant to loadTimeData.getStringF(id, ...).
   *
   * @param {string} id The id of the string to return.
   * @param {...string} The values to replace into the string.
   * @return {string} The translated string with replaced values.
   */
  function strf(id, var_args) {
    return loadTimeData.getStringF.apply(loadTimeData, arguments);
  }

  /**
   * @param {number} code File error code (from FileError object).
   * @return {string} Translated file error string.
   */
  function getFileErrorString(code) {
    for (var key in FileError) {
      var match = /(.*)_ERR$/.exec(key);
      if (match && FileError[key] == code) {
        // This would convert 1 to 'NOT_FOUND'.
        code = match[1];
        break;
      }
    }
    return loadTimeData.getString('FILE_ERROR_' + code) ||
        loadTimeData.getStringF('FILE_ERROR_GENERIC', code);
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

    if (parent_path[parent_path.length - 1] != '/')
      parent_path += '/';

    if (child_path[child_path.length - 1] != '/')
      child_path += '/';

    return child_path.indexOf(parent_path) == 0;
  }

  function removeChildren(element) {
    element.textContent = '';
  }

  function setClassIf(element, className, condition) {
    if (condition)
      element.classList.add(className);
    else
      element.classList.remove(className);
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
   * FileWatcher that also watches for metadata changes.
   * @extends {FileWatcher}
   */
  FileManager.MetadataFileWatcher = function(fileManager) {
    FileWatcher.call(this,
                     fileManager.filesystem_.root,
                     fileManager.directoryModel_,
                     fileManager.volumeManager_);
    this.metadataCache_ = fileManager.metadataCache_;

    this.filesystemChangeHandler_ =
        fileManager.updateMetadataInUI_.bind(fileManager, 'filesystem');
    this.thumbnailChangeHandler_ =
        fileManager.updateMetadataInUI_.bind(fileManager, 'thumbnail');
    this.gdataChangeHandler_ =
        fileManager.updateMetadataInUI_.bind(fileManager, 'gdata');

    this.filesystemObserverId_ = null;
    this.thumbnailObserverId_ = null;
    this.gdataObserverId_ = null;

    // Holds the directories known to contain files with stale metadata
    // as URL to bool map.
    this.directoriesWithStaleMetadata_ = {};
  };

  FileManager.MetadataFileWatcher.prototype.__proto__ = FileWatcher.prototype;

  /**
   * Changed metadata observers for the new directory.
   * @override
   * @param {DirectoryEntryi?} entry New watched directory entry.
   * @override
   */
  FileManager.MetadataFileWatcher.prototype.changeWatchedEntry =
      function(entry) {
    FileWatcher.prototype.changeWatchedEntry.call(this, entry);

    if (this.filesystemObserverId_)
      this.metadataCache_.removeObserver(this.filesystemObserverId_);
    if (this.thumbnailObserverId_)
      this.metadataCache_.removeObserver(this.thumbnailObserverId_);
    if (this.gdataObserverId_)
      this.metadataCache_.removeObserver(this.gdataObserverId_);
    this.filesystemObserverId_ = null;
    this.gdataObserverId_ = null;
    if (!entry)
      return;

    this.filesystemObserverId_ = this.metadataCache_.addObserver(
        entry,
        MetadataCache.CHILDREN,
        'filesystem',
        this.filesystemChangeHandler_);

    this.thumbnailObserverId_ = this.metadataCache_.addObserver(
        entry,
        MetadataCache.CHILDREN,
        'thumbnail',
        this.thumbnailChangeHandler_);

    if (DirectoryModel.getRootType(entry.fullPath) ==
        DirectoryModel.RootType.GDATA) {
      this.gdataObserverId_ = this.metadataCache_.addObserver(
          entry,
          MetadataCache.CHILDREN,
          'gdata',
          this.gdataChangeHandler_);
    }
  };

  /**
   * @override
   */
  FileManager.MetadataFileWatcher.prototype.onFileInWatchedDirectoryChanged =
      function() {
    FileWatcher.prototype.onFileInWatchedDirectoryChanged.apply(this);
    delete this.directoriesWithStaleMetadata_[
        this.getWatchedDirectoryEntry().toURL()];
  };

  /**
   * Ask the GData service to re-fetch the metadata for the current directory.
   * @param {string} imageURL Image URL
   */
  FileManager.MetadataFileWatcher.prototype.requestMetadataRefresh =
      function(imageURL) {
    if (!FileType.isOnGDrive(imageURL))
      return;
    // TODO(kaznacheev) This does not really work with GData search.
    var url = imageURL.substr(0, imageURL.lastIndexOf('/'));
    // Skip if the current directory is now being refreshed.
    if (this.directoriesWithStaleMetadata_[url])
      return;

    this.directoriesWithStaleMetadata_[url] = true;
    chrome.fileBrowserPrivate.requestDirectoryRefresh(url);
  };

  /**
   * Load translated strings.
   */
  FileManager.initStrings = function(callback) {
    chrome.fileBrowserPrivate.getStrings(function(strings) {
      loadTimeData.data = strings;
      if (callback)
        callback();
    });
  };

  /**
   * FileManager initially created hidden to prevent flickering.
   * When DOM is almost constructed it need to be shown. Cancels
   * delayed show.
   */
  FileManager.prototype.show_ = function() {
    if (this.showDelayTimeout_) {
      clearTimeout(this.showDelayTimeout_);
      showDelayTimeout_ = null;
    }
    this.dialogDom_.classList.add('loaded');
  };

  /**
   * If initialization code think that right after initialization
   * something going to be shown instead of just a file list (like Gallery)
   * it may delay show to prevent flickering. However initialization may take
   * significant time and we don't want to keep it hidden for too long.
   * So it will be shown not more than in 0.5 sec. If initialization completed
   * the page must show immediatelly.
   *
   * @param {number} delay In milliseconds.
   */
  FileManager.prototype.delayShow_ = function(delay) {
    if (!this.showDelayTimeout_) {
      this.showDelayTimeout_ = setTimeout(function() {
        this.showDelayTimeout_ = null;
        this.show_();
      }.bind(this), delay);
    }
  };

  // Instance methods.

  /**
   * Request local file system, resolve roots and init_ after that.
   * @private
   */
  FileManager.prototype.initFileSystem_ = function() {
    util.installFileErrorToString();
    // Replace the default unit in util to translated unit.
    util.UNITS = [str('SIZE_KB'),
                  str('SIZE_MB'),
                  str('SIZE_GB'),
                  str('SIZE_TB'),
                  str('SIZE_PB')];

    metrics.startInterval('Load.FileSystem');

    var self = this;
    var downcount = 2;
    function done() {
      if (--downcount == 0)
        self.init_();
    }

    chrome.fileBrowserPrivate.requestLocalFileSystem(function(filesystem) {
      metrics.recordInterval('Load.FileSystem');
      self.filesystem_ = filesystem;
      done();
    });

    // GDATA preferences should be initialized before creating DirectoryModel
    // to tot rebuild the roots list.
    this.updateNetworkStateAndGDataPreferences_(function() {
      done();
    });
  };

  /**
   * Continue initializing the file manager after resolving roots.
   */
  FileManager.prototype.init_ = function() {
    metrics.startInterval('Load.DOM');
    this.initCommands_();

    this.metadataCache_ = MetadataCache.createFull();
    // PyAuto tests monitor this state by polling this variable
    this.__defineGetter__('workerInitialized_', function() {
       return this.metadataCache_.isInitialized();
    }.bind(this));

    this.dateFormatter_ = v8Intl.DateTimeFormat(
        [] /* default locale */,
        {year: 'numeric', month: 'short', day: 'numeric',
         hour: 'numeric', minute: 'numeric'});
    this.timeFormatter_ = v8Intl.DateTimeFormat(
        [] /* default locale */,
        {hour: 'numeric', minute: 'numeric'});

    this.collator_ = v8Intl.Collator([], {numeric: true, sensitivity: 'base'});

    // Optional list of file types.
    this.fileTypes_ = this.params_.typeList;

    this.showCheckboxes_ =
        (this.dialogType_ == FileManager.DialogType.FULL_PAGE ||
         this.dialogType_ == FileManager.DialogType.SELECT_OPEN_MULTI_FILE);

    this.table_.startBatchUpdates();
    this.grid_.startBatchUpdates();

    this.initFileList_();
    this.initDialogs_();

    window.addEventListener('popstate', this.onPopState_.bind(this));
    window.addEventListener('unload', this.onUnload_.bind(this));

    var dm = this.directoryModel_;
    dm.addEventListener('directory-changed',
                        this.onDirectoryChanged_.bind(this));
    var self = this;
    dm.addEventListener('begin-update-files', function() {
      self.currentList_.startBatchUpdates();
    });
    dm.addEventListener('end-update-files', function() {
      self.restoreItemBeingRenamed_();
      self.currentList_.endBatchUpdates();
    });
    dm.addEventListener('scan-started', this.showSpinnerLater_.bind(this));
    dm.addEventListener('scan-completed', this.showSpinner_.bind(this, false));
    dm.addEventListener('scan-completed',
                        this.refreshCurrentDirectoryMetadata_.bind(this));
    dm.addEventListener('rescan-completed',
                        this.refreshCurrentDirectoryMetadata_.bind(this));
    this.addEventListener('selection-summarized',
                          this.onSelectionSummarized_.bind(this));

    this.summarizeSelection_();

    this.setupCurrentDirectory_(true /* page loading */);

    var stateChangeHandler =
        this.onNetworkStateOrGDataPreferencesChanged_.bind(this);
    chrome.fileBrowserPrivate.onGDataPreferencesChanged.addListener(
        stateChangeHandler);
    chrome.fileBrowserPrivate.onNetworkConnectionChanged.addListener(
        stateChangeHandler);
    stateChangeHandler();

    var sortField =
        window.localStorage['sort-field-' + this.dialogType_] ||
        'modificationTime';
    var sortDirection =
        window.localStorage['sort-direction-' + this.dialogType_] || 'desc';
    this.directoryModel_.sortFileList(sortField, sortDirection);

    this.refocus();

    if (this.dialogType_ == FileManager.DialogType.FULL_PAGE)
      this.initDataTransferOperations_();

    this.table_.endBatchUpdates();
    this.grid_.endBatchUpdates();

    // Show the page now unless it's already delayed.
    this.delayShow_(0);

    metrics.recordInterval('Load.DOM');
    metrics.recordInterval('Load.Total');
  };

  FileManager.prototype.initDataTransferOperations_ = function() {
    this.copyManager_ = new FileCopyManager(this.filesystem_.root);
    this.copyManager_.addEventListener('copy-progress',
                                       this.onCopyProgress_.bind(this));
    this.copyManager_.addEventListener('copy-operation-complete',
        this.onCopyManagerOperationComplete_.bind(this));

    var controller = this.fileTransferController_ = new FileTransferController(
        this.directoryModel_.getFileList(),
        this.directoryModel_.getFileListSelection(),
        GridItem.bind(null, this, false /* no checkbox */),
        this.copyManager_,
        this.directoryModel_);
    controller.attachDragSource(this.table_.list);
    controller.attachDropTarget(this.table_.list);
    controller.attachDragSource(this.grid_);
    controller.attachDropTarget(this.grid_);
    controller.attachDropTarget(this.rootsList_, true);
    controller.attachCopyPasteHandlers(this.document_);
    controller.addEventListener('selection-copied',
        this.blinkSelection.bind(this));
    controller.addEventListener('selection-cut',
        this.blinkSelection.bind(this));
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

    this.document_.addEventListener(
      'contextmenu', this.updateCommands_.bind(this), true);

    this.rootsContextMenu_ =
        this.dialogDom_.querySelector('.roots-context-menu');
    cr.ui.Menu.decorate(this.rootsContextMenu_);

    this.docsSettingsMenu_ =
        this.dialogDom_.querySelector('#docs-settings');
    cr.ui.Menu.decorate(this.docsSettingsMenu_);

    this.document_.addEventListener('command', this.onCommand_.bind(this));
  };

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
    this.defaultTaskPicker =
        new cr.filebrowser.DefaultActionDialog(this.dialogDom_);
  };

  /**
   * One-time initialization of various DOM nodes.
   */
  FileManager.prototype.initDom_ = function() {
    // Cache nodes we'll be manipulating.
    this.previewThumbnails_ =
        this.dialogDom_.querySelector('.preview-thumbnails');
    this.previewPanel_ = this.dialogDom_.querySelector('.preview-panel');
    this.previewSummary_ = this.dialogDom_.querySelector('.preview-summary');
    this.filenameInput_ = this.dialogDom_.querySelector(
        '#filename-input-box input');
    this.taskItems_ = this.dialogDom_.querySelector('#tasks');
    this.okButton_ = this.dialogDom_.querySelector('.ok');
    this.cancelButton_ = this.dialogDom_.querySelector('.cancel');
    this.deleteButton_ = this.dialogDom_.querySelector('#delete-button');
    this.table_ = this.dialogDom_.querySelector('.detail-table');
    this.grid_ = this.dialogDom_.querySelector('.thumbnail-grid');
    this.spinner_ = this.dialogDom_.querySelector('.spinner');
    this.showSpinner_(false);
    this.butter_ = this.dialogDom_.querySelector('.butter-bar');
    this.unmountedPanel_ = this.dialogDom_.querySelector('#unmounted-panel');

    cr.ui.decorate('#gdata-settings', cr.ui.MenuButton);
    cr.ui.Table.decorate(this.table_);
    cr.ui.Grid.decorate(this.grid_);

    this.document_.addEventListener('keydown', this.onKeyDown_.bind(this));
    // Disable the default browser context menu.
    this.document_.addEventListener('contextmenu',
                                    function(e) { e.preventDefault() });

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

    this.listContainer_ = this.dialogDom_.querySelector('#list-container');
    this.listContainer_.addEventListener(
        'keydown', this.onListKeyDown_.bind(this));
    this.listContainer_.addEventListener(
        'keypress', this.onListKeyPress_.bind(this));
    this.listContainer_.addEventListener(
        'mousemove', this.onListMouseMove_.bind(this));

    this.okButton_.addEventListener('click', this.onOk_.bind(this));
    this.onCancelBound_ = this.onCancel_.bind(this);
    this.cancelButton_.addEventListener('click', this.onCancelBound_);

    this.deleteButton_.addEventListener('click',
        this.onDeleteButtonClick_.bind(this));
    this.deleteButton_.addEventListener('keypress',
        this.onDeleteButtonKeyPress_.bind(this));

    this.decorateSplitter(
        this.dialogDom_.querySelector('div.sidebar-splitter'));

    this.dialogContainer_ = this.dialogDom_.querySelector('.dialog-container');
    this.dialogDom_.querySelector('#detail-view').addEventListener(
        'click', this.onDetailViewButtonClick_.bind(this));
    this.dialogDom_.querySelector('#thumbnail-view').addEventListener(
        'click', this.onThumbnailViewButtonClick_.bind(this));

    this.syncButton = this.dialogDom_.querySelector('#gdata-sync-settings');
    this.syncButton.addEventListener('click', this.onGDataPrefClick_.bind(
        this, 'cellularDisabled', false /* not inverted */));

    this.hostedButton = this.dialogDom_.querySelector('#gdata-hosted-settings');
    this.hostedButton.addEventListener('click', this.onGDataPrefClick_.bind(
        this, 'hostedFilesDisabled', true /* inverted */));

    cr.ui.ComboButton.decorate(this.taskItems_);
    this.taskItems_.addEventListener('select',
        this.onTaskItemClicked_.bind(this));

    this.dialogDom_.ownerDocument.defaultView.addEventListener(
        'resize', this.onResize_.bind(this));

    if (loadTimeData.getBoolean('ASH'))
      this.dialogDom_.setAttribute('ash', 'true');

    this.filePopup_ = null;

    this.dialogDom_.querySelector('#search-box').addEventListener(
        'input', this.onSearchBoxUpdate_.bind(this));

    // Populate the static localized strings.
    i18nTemplate.process(this.document_, loadTimeData);
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

    var singleSelection =
        this.dialogType_ == FileManager.DialogType.SELECT_OPEN_FILE ||
        this.dialogType_ == FileManager.DialogType.SELECT_FOLDER ||
        this.dialogType_ == FileManager.DialogType.SELECT_SAVEAS_FILE;

    this.directoryModel_ = new DirectoryModel(
        this.filesystem_.root,
        singleSelection,
        this.metadataCache_,
        this.volumeManager_,
        this.isGDataEnabled());

    this.directoryModel_.start();

    this.fileWatcher_ = new FileManager.MetadataFileWatcher(this);
    this.fileWatcher_.start();

    var dataModel = this.directoryModel_.getFileList();
    var collator = this.collator_;
    // TODO(dgozman): refactor comparison functions together with
    // render/update/display.
    dataModel.setCompareFunction('name', function(a, b) {
      return collator.compare(a.name, b.name);
    });
    dataModel.setCompareFunction('modificationTime',
                                 this.compareMtime_.bind(this));
    dataModel.setCompareFunction('size',
                                 this.compareSize_.bind(this));
    dataModel.setCompareFunction('type',
                                 this.compareType_.bind(this));

    dataModel.addEventListener('splice',
                               this.onDataModelSplice_.bind(this));
    dataModel.addEventListener('permuted',
                               this.onDataModelPermuted_.bind(this));

    this.directoryModel_.getFileListSelection().addEventListener(
        'change', this.onSelectionChanged_.bind(this));

    this.initTable_();
    this.initGrid_();
    this.initRootsList_();

    var listType = FileManager.ListType.DETAIL;
    if (FileManager.DialogType.isModal(this.dialogType_))
      listType = window.localStorage['listType-' + this.dialogType_] ||
          FileManager.ListType.DETAIL;
    this.setListType(listType);

    this.textSearchState_ = {text: '', date: new Date()};

    this.volumeManager_.addEventListener('gdata-status-changed',
        this.updateGDataUnmountedPanel_.bind(this));

    this.closeOnUnmount_ = this.params_.mountTriggered;

    if (this.closeOnUnmount_) {
      this.volumeManager_.addEventListener('externally-unmounted',
         this.onExternallyUnmounted_.bind(this));
    }
    // Update metadata to change 'Today' and 'Yesterday' dates.
    var today = new Date();
    today.setHours(0);
    today.setMinutes(0);
    today.setSeconds(0);
    today.setMilliseconds(0);
    setTimeout(this.dailyUpdateModificationTime_.bind(this),
               today.getTime() + MILLISECONDS_IN_DAY - Date.now() + 1000);
  };

  FileManager.prototype.initRootsList_ = function() {
    this.rootsList_ = this.dialogDom_.querySelector('#roots-list');
    cr.ui.List.decorate(this.rootsList_);

    var self = this;
    this.rootsList_.itemConstructor = function(entry) {
      return self.renderRoot_(entry);
    };

    this.rootsList_.selectionModel =
        this.directoryModel_.getRootsListSelectionModel();

    // TODO(dgozman): add "Add a drive" item.
    this.rootsList_.dataModel = this.directoryModel_.getRootsList();
  };

  /**
   * Shows the panel when current directory is GDATA and it's unmounted.
   * Hides it otherwise. The pannel shows spinner if GDATA is mounting or
   * an error message if it failed.
   */
  FileManager.prototype.updateGDataUnmountedPanel_ = function() {
    var node = this.dialogContainer_;
    if (this.isOnGData()) {
      var status = this.volumeManager_.getGDataStatus();
      if (status == VolumeManager.GDataStatus.MOUNTING ||
          status == VolumeManager.GDataStatus.ERROR) {
        this.ensureGDataUnmountedPanelInitialized_();
      }
      if (status == VolumeManager.GDataStatus.ERROR)
        this.unmountedPanel_.classList.add('retry-enabled');
      node.setAttribute('gdata', status);
    } else {
      node.removeAttribute('gdata');
    }
  };

  /**
   * Creates contents for the GDATA unmounted panel.
   */
  FileManager.prototype.ensureGDataUnmountedPanelInitialized_ = function() {
    var panel = this.unmountedPanel_;
    if (panel.firstElementChild)
      return;

    function create(parent, tag, className, opt_textContent) {
      var div = panel.ownerDocument.createElement(tag);
      div.className = className;
      div.textContent = opt_textContent || '';
      parent.appendChild(div);
      return div;
    }

    var loading = create(panel, 'div', 'loading', str('GDATA_LOADING'));
    var spinnerBox = create(loading, 'div', 'spinner-box');
    create(spinnerBox, 'div', 'spinner');
    var progress = create(panel, 'div', 'progress');
    chrome.fileBrowserPrivate.onDocumentFeedFetched.addListener(
        function(fileCount) {
          progress.textContent = strf('GDATA_LOADING_PROGRESS', fileCount);
        });

    create(panel, 'div', 'error', str('GDATA_CANNOT_REACH'));

    var retryButton = create(panel, 'button', 'retry', str('GDATA_RETRY'));
    retryButton.hidden = true;
    var vm = this.volumeManager_;
    retryButton.onclick = function() {
      vm.mountGData(function() {}, function() {});
    };

    var learnMore = create(panel, 'div', 'learn-more plain-link',
                           str('GDATA_LEARN_MORE'));
    learnMore.onclick = this.onExternalLinkClick_.bind(this,
        GOOGLE_DRIVE_ERROR_HELP_URL);
  };

  FileManager.prototype.onDataModelSplice_ = function(event) {
    var checkbox = this.document_.querySelector('#select-all-checkbox');
    if (checkbox)
      this.updateSelectAllCheckboxState_(checkbox);
  };

  FileManager.prototype.onDataModelPermuted_ = function(event) {
    var sortStatus = this.directoryModel_.getFileList().sortStatus;
    window.localStorage['sort-field-' + this.dialogType_] = sortStatus.field;
    window.localStorage['sort-direction-' + this.dialogType_] =
        sortStatus.direction;
  };

  /**
   * Compare by mtime first, then by name.
   */
  FileManager.prototype.compareMtime_ = function(a, b) {
    var aCachedFilesystem = this.metadataCache_.getCached(a, 'filesystem');
    var aTime = aCachedFilesystem ? aCachedFilesystem.modificationTime : 0;

    var bCachedFilesystem = this.metadataCache_.getCached(b, 'filesystem');
    var bTime = bCachedFilesystem ? bCachedFilesystem.modificationTime : 0;

    if (aTime > bTime)
      return 1;

    if (aTime < bTime)
      return -1;

    return this.collator_.compare(a.name, b.name);
  };

  /**
   * Compare by size first, then by name.
   */
  FileManager.prototype.compareSize_ = function(a, b) {
    var aCachedFilesystem = this.metadataCache_.getCached(a, 'filesystem');
    var aSize = aCachedFilesystem ? aCachedFilesystem.size : 0;

    var bCachedFilesystem = this.metadataCache_.getCached(b, 'filesystem');
    var bSize = bCachedFilesystem ? bCachedFilesystem.size : 0;

    if (aSize != bSize) return aSize - bSize;
    return this.collator_.compare(a.name, b.name);
  };

  /**
   * Compare by type first, then by subtype and then by name.
   */
  FileManager.prototype.compareType_ = function(a, b) {
    // Directories precede files.
    if (a.isDirectory != b.isDirectory)
      return Number(b.isDirectory) - Number(a.isDirectory);

    var aType = this.getFileTypeString_(a);
    var bType = this.getFileTypeString_(b);

    var result = this.collator_.compare(aType, bType);
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
    var butter = this.butter_;
    if (opt_options) {
      if ('actions' in opt_options) {
        var actions = butter.querySelector('.actions');
        while (actions.childNodes.length)
          actions.removeChild(actions.firstChild);
        for (var label in opt_options.actions) {
          var link = this.document_.createElement('a');
          link.addEventListener('click', function() {
              opt_options.actions[label]();
              return false;
          });
          actions.appendChild(link);
        }
        actions.classList.remove('hide-in-butter');
      }
      if ('progress' in opt_options) {
        butter.querySelector('.progress-bar')
            .classList.remove('hide-in-butter');
      }
    }

    var self = this;

    setTimeout(function() {
      self.currentButter_ = butter;
      self.updateButter(message, opt_options);
      self.butterLastShowTime_ = new Date();
    });

    return butter;
  };

  FileManager.prototype.showButterError = function(message, opt_options) {
    var butter = this.showButter(message, opt_options);
    butter.classList.add('error');
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
          self.butterTimer_ = null;
      }, timeout);
    }

    var butter = this.currentButter_;
    butter.querySelector('.butter-message').textContent = message;
    if (message) {
      // The butter bar is made visible on the first non-empty message.
      butter.classList.remove('before-show');
    }
    if (opt_options && 'progress' in opt_options) {
      butter.querySelector('.progress-track').style.width =
          (opt_options.progress * 100) + '%';
    }

    butter.style.left = ((this.dialogDom_.clientWidth -
                          butter.clientWidth) / 2) + 'px';
  };

  FileManager.prototype.hideButter = function() {
    if (this.currentButter_) {
      var delay = Math.max(MINIMUM_BUTTER_DISPLAY_TIME_MS -
          (new Date() - this.butterLastShowTime_), 0);

      var butter = this.currentButter_;

      setTimeout(function() {
        butter.classList.add('after-show');
      }, delay);

      setTimeout(function() {
          butter.classList.remove('error');
          butter.classList.remove('after-show');
          butter.classList.add('before-show');
          butter.querySelector('.actions').classList.add('hide-in-butter');
          butter.querySelector('.progress-bar').classList.add('hide-in-butter');
      }, delay + 1000);

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
  FileManager.prototype.getSelectedFilterIndex_ = function(fileName) {
    // TODO(serya): Implement the combo box
    // For now try to guess choice by file extension.
    if (!this.fileTypes_ || this.fileTypes_.length == 0) return 0;

    var extension = /\.[^\.]+$/.exec(fileName);
    extension = extension ? extension[0].substring(1).toLowerCase() : '';
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
    for (var key in this.commands_)
      this.commands_[key].disabled = !this.canExecute_(key);
  };

  /**
   * @param {string} commandId Command identifier.
   * @return {boolean} True if the command can be executed for current
   *                   selection.
   */
  FileManager.prototype.canExecute_ = function(commandId) {
    var readonly = this.isOnReadonlyDirectory();
    switch (commandId) {
      case 'copy':
      case 'cut':
        return this.document_.queryCommandEnabled(commandId);

      // Hack goes below, since we don't receive beforepaste event, but receive
      // beforecut and beforecopy events.
      case 'paste':
        return this.isRenamingInProgress()
                   ? this.document_.queryCommandEnabled(commandId)
                   : (!!this.fileTransferController_ &&
                      this.fileTransferController_.queryPasteCommandEnabled());

      case 'rename':
        return (// Initialized to the point where we have a current directory
                !readonly &&
                // Rename not in progress.
                !this.isRenamingInProgress() &&
                // Only one file selected.
                this.selection &&
                this.selection.totalCount == 1);

      case 'delete':
        return (this.isRenamingInProgress()
                ? this.document_.queryCommandEnabled(commandId)
                : !readonly &&
                  this.selection &&
                  this.selection.totalCount > 0);

      case 'newfolder':
        return !readonly &&
               !this.directoryModel_.isSearching() &&
               (this.dialogType_ == FileManager.DialogType.SELECT_SAVEAS_FILE ||
                this.dialogType_ == FileManager.DialogType.FULL_PAGE);

      case 'unmount':
        return true;

      case 'format':
        var entry = this.directoryModel_.getCurrentRootDirEntry();

        return entry && DirectoryModel.getRootType(entry.fullPath) ==
                        DirectoryModel.RootType.REMOVABLE;
    }
  };

  FileManager.prototype.getRootEntry_ = function(index) {
    if (index == -1)
      return null;

    return this.rootsList_.dataModel.item(index);
  };

  FileManager.prototype.updateCommonActionButtons_ = function() {
    if (this.deleteButton_) {
      if (this.canExecute_('delete'))
        this.deleteButton_.removeAttribute('disabled');
      else
        this.deleteButton_.setAttribute('disabled', true);
    }
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
      this.table_.dataModel = this.directoryModel_.getFileList();
      this.table_.selectionModel = this.directoryModel_.getFileListSelection();
      this.table_.style.display = '';
      this.grid_.style.display = 'none';
      this.grid_.selectionModel = this.emptySelectionModel_;
      this.grid_.dataModel = this.emptyDataModel_;
      this.table_.style.display = '';
      /** @type {cr.ui.List} */
      this.currentList_ = this.table_.list;
      this.dialogDom_.querySelector('#detail-view').disabled = true;
      this.dialogDom_.querySelector('#thumbnail-view').disabled = false;
    } else if (type == FileManager.ListType.THUMBNAIL) {
      this.grid_.dataModel = this.directoryModel_.getFileList();
      this.grid_.selectionModel = this.directoryModel_.getFileListSelection();
      this.grid_.style.display = '';
      this.table_.style.display = 'none';
      this.table_.selectionModel = this.emptySelectionModel_;
      this.table_.dataModel = this.emptyDataModel_;
      this.grid_.style.display = '';
      /** @type {cr.ui.List} */
      this.currentList_ = this.grid_;
      this.dialogDom_.querySelector('#thumbnail-view').disabled = true;
      this.dialogDom_.querySelector('#detail-view').disabled = false;
    } else {
      throw new Error('Unknown list type: ' + type);
    }

    this.listType_ = type;
    this.updateColumnModel_();
    this.onResize_();

    this.table_.list.endBatchUpdates();
    this.grid_.endBatchUpdates();
  };

  /**
   * Initialize the file thumbnail grid.
   */
  FileManager.prototype.initGrid_ = function() {
    var self = this;
    this.grid_.itemConstructor =
        GridItem.bind(null, this, this.showCheckboxes_);
    // TODO(bshe): should override cr.ui.List's activateItemAtIndex function
    // rather than listen explicitly for double click or tap events.
    this.grid_.addEventListener(
        'dblclick', this.onDetailDoubleClickOrTap_.bind(this));
    this.grid_.addEventListener(
        cr.ui.TouchHandler.EventType.TAP,
        this.onDetailDoubleClickOrTap_.bind(this));
    cr.ui.contextMenuHandler.setContextMenu(this.grid_, this.fileContextMenu_);
  };

  /**
   * Initialize the file list table.
   */
  FileManager.prototype.initTable_ = function() {
    var renderFunction = this.table_.getRenderFunction();
    this.table_.setRenderFunction(function(entry, parent) {
      var item = renderFunction(entry, parent);
      this.updateGDataStyle_(
          item, entry, this.metadataCache_.getCached(entry, 'gdata'));
      return item;
    }.bind(this));

    var fullPage = (this.dialogType_ == FileManager.DialogType.FULL_PAGE);

    var columns = [
        new cr.ui.table.TableColumn('name', str('NAME_COLUMN_LABEL'),
                                    fullPage ? 470 : 324),
        new cr.ui.table.TableColumn('size', str('SIZE_COLUMN_LABEL'),
                                    fullPage ? 110 : 92, true),
        new cr.ui.table.TableColumn('type', str('TYPE_COLUMN_LABEL'),
                                    fullPage ? 200 : 160),
        new cr.ui.table.TableColumn('modificationTime',
                                    str('DATE_COLUMN_LABEL'),
                                    fullPage ? 150 : 210)
    ];

    // TODO(dgozman): refactor render/update/display stuff.
    columns[0].renderFunction = this.renderName_.bind(this);
    columns[1].renderFunction = this.renderSize_.bind(this);
    columns[1].defaultOrder = 'desc';
    columns[2].renderFunction = this.renderType_.bind(this);
    columns[3].renderFunction = this.renderDate_.bind(this);
    columns[3].defaultOrder = 'desc';

    if (this.showCheckboxes_) {
      columns[0].headerRenderFunction =
          this.renderNameColumnHeader_.bind(this, columns[0].name);
    }

    this.regularColumnModel_ = new cr.ui.table.TableColumnModel(columns);

    if (fullPage) {
      columns.push(new cr.ui.table.TableColumn(
          'offline', str('OFFLINE_COLUMN_LABEL'), 150));
      columns[4].renderFunction = this.renderOffline_.bind(this);

      this.gdataColumnModel_ = new cr.ui.table.TableColumnModel(columns);
    } else {
      this.gdataColumnModel_ = null;
    }

    // TODO(bshe): should override cr.ui.List's activateItemAtIndex function
    // rather than listen explicitly for double click or tap events.
    // Don't pay attention to double clicks on the table header.
    this.table_.list.addEventListener(
        'dblclick', this.onDetailDoubleClickOrTap_.bind(this));
    this.table_.list.addEventListener(
        cr.ui.TouchHandler.EventType.TAP,
        this.onDetailDoubleClickOrTap_.bind(this));

    cr.ui.contextMenuHandler.setContextMenu(this.table_.querySelector('.list'),
        this.fileContextMenu_);
  };

  FileManager.prototype.initButter_ = function() {
    var self = this;
    var progress = this.copyManager_.getProgress();

    var options = {progress: progress.percentage, actions: {}, timeout: 0};
    options.actions[str('CANCEL_LABEL')] = function cancelPaste() {
      self.copyManager_.requestCancel();
    };
    this.showButter(strf('PASTE_ITEMS_REMAINING', progress.pendingItems),
                    options);
  };

  FileManager.prototype.onCopyProgress_ = function(event) {
    var progress = this.copyManager_.getProgress();

    if (event.reason == 'BEGIN') {
      if (this.currentButter_)
        this.hideButter();

      clearTimeout(this.butterTimeout_);
      // If the copy process lasts more than 500 ms, we show a progress bar.
      this.butterTimeout_ = setTimeout(this.initButter_.bind(this), 500);
      return;
    }
    if (event.reason == 'PROGRESS') {
      // Perform this check inside Progress event handler, avoid to log error
      // message 'Unknown event reason: PROGRESS' in console.
      if (this.currentButter_) {
        var options = {progress: progress.percentage, timeout: 0};
        this.updateButter(strf('PASTE_ITEMS_REMAINING', progress.pendingItems),
                          options);
      }
      return;
    }
    if (event.reason == 'SUCCESS') {
      clearTimeout(this.butterTimeout_);
      if (this.currentButter_)
        this.hideButter();
    } else if (event.reason == 'ERROR') {
      clearTimeout(this.butterTimeout_);
      switch (event.error.reason) {
        case 'TARGET_EXISTS':
          var name = event.error.data.name;
          if (event.error.data.isDirectory)
            name += '/';
          this.showButterError(strf('PASTE_TARGET_EXISTS_ERROR', name));
          break;

        case 'FILESYSTEM_ERROR':
          if (event.error.data.toGDrive &&
              event.error.data.code == FileError.QUOTA_EXCEEDED_ERR) {
            this.hideButter();
            this.alert.showHtml(
                strf('GDATA_SERVER_OUT_OF_SPACE_HEADER'),
                strf('GDATA_SERVER_OUT_OF_SPACE_MESSAGE',
                    decodeURIComponent(
                        event.error.data.sourceFileUrl.split('/').pop()),
                    GOOGLE_DRIVE_BUY_STORAGE));
          } else {
            this.showButterError(
                strf('PASTE_FILESYSTEM_ERROR',
                      getFileErrorString(event.error.data.code)));
          }
          break;

        default:
          this.showButterError(strf('PASTE_UNEXPECTED_ERROR', event.error));
          break;
      }
    } else if (event.reason == 'CANCELLED') {
      this.showButter(str('PASTE_CANCELLED'), {timeout: 1000});
    } else {
      console.log('Unknown event reason: ' + event.reason);
    }

    // TODO(benchan): Currently, there is no FileWatcher emulation for
    // GDataFileSystem, so we need to manually trigger the directory rescan
    // after paste operations complete. Remove this once we emulate file
    // watching functionalities in GDataFileSystem.
    if (this.isOnGData()) {
      if (event.reason == 'SUCCESS' || event.reason == 'ERROR' ||
          event.reason == 'CANCELLED') {
        this.directoryModel_.rescanLater();
      }
    }
  };

  /**
   * Handler of file manager operations. Update directory model
   * to reflect operation result iimediatelly (not waiting directory
   * update event).
   */
  FileManager.prototype.onCopyManagerOperationComplete_ = function(event) {
    var currentPath = this.directoryModel_.getCurrentDirPath();
    if (this.isOnGData() && this.directoryModel_.isSearching())
      return;

    function inCurrentDirectory(entry) {
      var fullPath = entry.fullPath;
      var dirPath = fullPath.substr(0, fullPath.length -
                                       entry.name.length - 1);
      return dirPath == currentPath;
    }
    for (var i = 0; i < event.affectedEntries.length; i++) {
      entry = event.affectedEntries[i];
      if (inCurrentDirectory(entry))
        this.directoryModel_.onEntryChanged(entry.name);
    }
  };

  FileManager.prototype.updateColumnModel_ = function() {
    if (this.listType_ != FileManager.ListType.DETAIL)
      return;
    this.table_.columnModel =
        (this.isOnGData() && this.gdataColumnModel_) ?
            this.gdataColumnModel_ :
            this.regularColumnModel_;
  };

  /**
   * Respond to a command being executed.
   */
  FileManager.prototype.onCommand_ = function(event) {
    switch (event.command.id) {
      case 'cut':
      case 'copy':
      case 'paste':
        document.execCommand(event.command.id);
        return;

      case 'rename':
        this.initiateRename_();
        return;

      case 'delete':
        if (this.isRenamingInProgress())
          document.execCommand('delete');
        else
          this.deleteEntries(this.selection.entries);

        return;

      case 'newfolder':
        this.onNewFolderCommand_(event);
        return;

      case 'unmount':
        this.unmountVolume_(
            this.directoryModel_.getCurrentRootDirEntry());
        return;

      case 'format':
        var entry = this.directoryModel_.getCurrentRootDirEntry();
        this.confirm.show(str('FORMATTING_WARNING'), function() {
          chrome.fileBrowserPrivate.formatDevice(entry.toURL());
        });

        return;
    }
  };

  /**
   * Respond to the back and forward buttons.
   */
  FileManager.prototype.onPopState_ = function(event) {
    this.closeFilePopup_();
    this.setupCurrentDirectory_(false /* page loading */);
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
   *
   * @param {boolean} pageLoading True if the page is loading,
                                  false if popping state.
   */
  FileManager.prototype.setupCurrentDirectory_ = function(pageLoading) {
    var path = location.hash ?  // Location hash has the highest priority.
        decodeURI(location.hash.substr(1)) :
        this.params_.defaultPath;

    if (!pageLoading && path == this.directoryModel_.getCurrentDirPath())
      return;

    if (!path) {
      this.directoryModel_.setupDefaultPath();
      return;
    }

    // In the FULL_PAGE mode if the hash path points to a file we might have
    // to invoke a task after selecting it.
    // If the file path is in params_ we only want to select the file.
    var invokeHandlers = pageLoading && !this.params_.selectOnly &&
        this.dialogType_ == FileManager.DialogType.FULL_PAGE &&
        !!location.hash;

    if (DirectoryModel.getRootType(path) == DirectoryModel.RootType.GDATA) {
      var tracker = this.directoryModel_.createDirectoryChangeTracker();
      // Expected finish of setupPath to GData.
      tracker.exceptInitialChange = true;
      tracker.start();
      if (!this.isGDataEnabled()) {
        if (pageLoading)
          this.show_();
        this.directoryModel_.setupDefaultPath();
        return;
      }
      var gdataPath = '/' + DirectoryModel.GDATA_DIRECTORY;
      if (this.volumeManager_.isMounted(gdataPath)) {
        this.finishSetupCurrentDirectory_(path, invokeHandlers);
        return;
      }
      if (pageLoading)
        this.delayShow_(500);
      // Reflect immediatelly in the UI we are on GData and display
      // mounting UI.
      this.directoryModel_.setupPath(gdataPath);

      if (!this.isOnGData()) {
        // Since GDATA is not mounted it should be resolved synchronously
        // (no need in asynchronous calls to filesystem API). It is important
        // to prevent race condition.
        console.error('Expected path set up synchronously');
      }

      var self = this;
      this.volumeManager_.mountGData(function() {
        tracker.stop();
        if (!tracker.hasChanged) {
          self.finishSetupCurrentDirectory_(path, invokeHandlers);
        }
      }, function(error) {
        tracker.stop();
      });
    } else {
      if (invokeHandlers && pageLoading)
        this.delayShow_(500);
      this.finishSetupCurrentDirectory_(path, invokeHandlers);
    }
  };

  /**
   * @param {string} path Path to setup.
   * @param {boolean} invokeHandlers If thrue and |path| points to a file
   *     then default handler is triggered.
   */
  FileManager.prototype.finishSetupCurrentDirectory_ = function(
      path, invokeHandlers) {
    if (invokeHandlers) {
      // Keep track of whether the path is identified as an existing leaf
      // node.  Note that onResolve is guaranteed to be called (exactly once)
      // before onLoadedActivateLeaf.
      var foundLeaf = true;
      function onResolve(baseName, leafName, exists) {
        if (!exists || leafName == '') {
          // Non-existent file or a directory. Remove the shade immediately.
          foundLeaf = false;
          self.show_();
        }
      }

      // TODO(kaznacheev): refactor dispatchDefaultTask to accept an array
      // of urls instead of a selection. This will remove the need to wait
      // until the selection is done.
      var self = this;
      function onLoadedActivateLeaf() {
        if (foundLeaf) {
          // There are 3 ways we can get here:
          // 1. Invoked from file_manager_util::ViewFile. This can only
          //    happen for 'gallery' and 'mount-archive' actions.
          // 2. Reloading a Gallery page. Must be an image or a video file.
          // 3. A user manually entered a URL pointing to a file.
          if (FileType.isImageOrVideo(path)) {
            self.dispatchInternalTask_('gallery', self.selection.urls);
          } else if (FileType.getMediaType(path) == 'archive') {
            self.show_();
            self.dispatchInternalTask_('mount-archive', self.selection.urls);
          } else {
            self.show_();
            return;
          }
        }
      }
      this.directoryModel_.setupPath(path, onLoadedActivateLeaf, onResolve);
      return;
    }

    if (this.dialogType_ == FileManager.DialogType.SELECT_SAVEAS_FILE) {
      this.directoryModel_.setupPath(path, undefined,
          function(basePath, leafName) {
            this.filenameInput_.value = leafName;
            this.selectDefaultPathInFilenameInput_();
          }.bind(this));
      return;
    }

    this.show_();
    this.directoryModel_.setupPath(path);
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
        // We don't want search enabled in save as dialog.
        this.dialogDom_.querySelector('#search-box').style.display = 'none';
        break;

      case FileManager.DialogType.FULL_PAGE:
        break;

      default:
        throw new Error('Unknown dialog type: ' + this.dialogType_);
    }

    this.okButton_.textContent = okLabel;

    var dialogTitle = this.params_.title || defaultTitle;
    this.dialogDom_.querySelector('.dialog-title').textContent = dialogTitle;

    this.dialogDom_.setAttribute('type', this.dialogType_);
  };

  FileManager.prototype.renderCheckbox_ = function() {
    function stopEventPropagation(event) {
      if (!event.shiftKey)
        event.stopPropagation();
    }
    var input = this.document_.createElement('input');
    input.setAttribute('type', 'checkbox');
    input.setAttribute('tabindex', -1);
    input.classList.add('common');
    input.addEventListener('mousedown', stopEventPropagation);
    input.addEventListener('mouseup', stopEventPropagation);
    input.addEventListener('dblclick', stopEventPropagation);

    input.addEventListener('click', function(event) {
      // Revert default action if shift pressed.
      if (event.shiftKey)
        this.checked = !this.checked;
    });
    return input;
  };

  /**
   * Render (and wire up) a checkbox to be used in either a detail or a
   * thumbnail list item.
   */
  FileManager.prototype.renderSelectionCheckbox_ = function(entry) {
    var input = this.renderCheckbox_();
    input.classList.add('file-checkbox');
    input.addEventListener('click',
                           this.onCheckboxClick_.bind(this));
    // Since we do not want to open the item when tap on checkbox, we need to
    // stop propagation of TAP event dispatched by checkbox ideally. But all
    // touch events from touch_handler are dispatched to the list control. So we
    // have to stop propagation of native touchstart event to prevent list
    // control from generating TAP event here. The synthetic click event will
    // select the touched checkbox/item.
    input.addEventListener('touchstart',
                           function(e) { e.stopPropagation() });

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
    input.className = 'common';
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
    var dm = this.directoryModel_.getFileList();
    checkbox.checked = this.selection && dm.length > 0 &&
                       dm.length == this.selection.totalCount;
    checkbox.disabled = dm.length == 0;
  };

  /**
   * Create a box containing a centered thumbnail image.
   *
   * @param {Entry} entry Entry which thumbnail is generating for.
   * @param {boolean} fill True if fill, false if fit.
   * @param {function(HTMLElement)} opt_imageLoadCallback Callback called when
   *                                the image has been loaded before inserting
   *                                it into the DOM.
   * @param {HTMLDivElement=} opt_box Existing box to render in.
   * @return {HTMLDivElement} Thumbnail box.
   */
  FileManager.prototype.renderThumbnailBox_ = function(
      entry, fill, opt_imageLoadCallback, opt_box) {
    var self = this;

    var box;
    if (opt_box) {
      box = opt_box;
    } else {
      box = this.document_.createElement('div');
      box.className = 'img-container';
    }

    var imageUrl = entry.toURL();

    // Failing to fetch a thumbnail likely means that the thumbnail URL
    // is now stale. Request a refresh of the current directory, to get
    // the new thumbnail URLs. Once the directory is refreshed, we'll get
    // notified via onFileChanged event.
    var onImageLoadError = this.fileWatcher_.requestMetadataRefresh.bind(
        this.fileWatcher_, imageUrl);

    var metadataTypes = 'thumbnail|filesystem';

    if (FileType.isOnGDrive(imageUrl)) {
      metadataTypes += '|gdata';
    } else {
      // TODO(dgozman): If we ask for 'media' for a GDrive file we fall into an
      // infinite loop.
      metadataTypes += '|media';
    }

    this.metadataCache_.get(imageUrl, metadataTypes,
        function(metadata) {
          new ThumbnailLoader(imageUrl, metadata).
              load(box, fill, opt_imageLoadCallback, onImageLoadError);
        });

    return box;
  };

  FileManager.prototype.decorateThumbnail_ =
      function(li, showCheckbox, entry) {
    li.className = 'thumbnail-item';

    var frame = this.document_.createElement('div');
    frame.className = 'thumbnail-frame';
    li.appendChild(frame);

    frame.appendChild(this.renderThumbnailBox_(entry, false));

    var bottom = this.document_.createElement('div');
    bottom.className = 'thumbnail-bottom';
    frame.appendChild(bottom);

    bottom.appendChild(this.renderFileNameLabel_(entry));

    if (showCheckbox) {
      var checkBox = this.renderSelectionCheckbox_(entry);
      checkBox.classList.add('white');
      bottom.appendChild(checkBox);
      bottom.classList.add('show-checkbox');
    }

    this.updateGDataStyle_(
        li, entry, this.metadataCache_.getCached(entry, 'gdata'));
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
    icon.setAttribute('iconType', FileType.getIcon(entry));
    return icon;
  };

  /**
   * Return the localized name for the root.
   * @param {string} path The full path of the root (starting with slash).
   * @return {string} The localized name.
   */
  FileManager.prototype.getRootLabel_ = function(path) {
    if (path == '/' + DirectoryModel.DOWNLOADS_DIRECTORY)
      return str('DOWNLOADS_DIRECTORY_LABEL');

    if (path == '/' + DirectoryModel.ARCHIVE_DIRECTORY)
      return str('ARCHIVE_DIRECTORY_LABEL');
    if (isParentPath('/' + DirectoryModel.ARCHIVE_DIRECTORY, path))
      return path.substring(DirectoryModel.ARCHIVE_DIRECTORY.length + 2);

    if (path == '/' + DirectoryModel.REMOVABLE_DIRECTORY)
      return str('REMOVABLE_DIRECTORY_LABEL');
    if (isParentPath('/' + DirectoryModel.REMOVABLE_DIRECTORY, path))
      return path.substring(DirectoryModel.REMOVABLE_DIRECTORY.length + 2);

    if (path == '/' + DirectoryModel.GDATA_DIRECTORY)
      return str('GDATA_DIRECTORY_LABEL');

    return path;
  };

  FileManager.prototype.renderRoot_ = function(entry) {
    var li = this.document_.createElement('li');
    li.className = 'root-item';
    var dm = this.directoryModel_;
    var handleClick = function() {
      if (li.selected) {
        dm.changeDirectory(entry.fullPath);
      }
    };
    li.addEventListener('mousedown', handleClick);
    li.addEventListener(cr.ui.TouchHandler.EventType.TOUCH_START, handleClick);

    var rootType = DirectoryModel.getRootType(entry.fullPath);

    var div = this.document_.createElement('div');
    div.className = 'root-label';

    div.setAttribute('type', rootType);
    if (rootType == DirectoryModel.RootType.REMOVABLE)
      div.setAttribute('subType',
          this.volumeManager_.getDeviceType(entry.fullPath));

    div.textContent = this.getRootLabel_(entry.fullPath);
    li.appendChild(div);

    if (rootType == DirectoryModel.RootType.ARCHIVE ||
        rootType == DirectoryModel.RootType.REMOVABLE) {
      var eject = this.document_.createElement('div');
      eject.className = 'root-eject';
      eject.addEventListener('click', function(event) {
        event.stopPropagation();
        this.unmountVolume_(entry);
      }.bind(this));
      // Block other mouse handlers.
      eject.addEventListener('mouseup', function(e) { e.stopPropagation() });
      eject.addEventListener('mousedown', function(e) { e.stopPropagation() });
      li.appendChild(eject);

      cr.ui.contextMenuHandler.setContextMenu(li, this.rootsContextMenu_);
    }

    cr.defineProperty(li, 'lead', cr.PropertyKind.BOOL_ATTR);
    cr.defineProperty(li, 'selected', cr.PropertyKind.BOOL_ATTR);

    var icon = rootType;
    if (this.volumeManager_.isUnreadable(entry.fullPath)) {
      icon = 'unreadable';
    }
    div.setAttribute('icon', icon);

    return li;
  };

  /**
   * Unmounts device.
   * @param {Entry} entry The entry to unmount.
   */
  FileManager.prototype.unmountVolume_ = function(entry) {
    listItem = this.rootsList_.getListItem(entry);
    if (listItem)
      listItem.setAttribute('disabled', '');
    var self = this;
    this.volumeManager_.unmount(entry.fullPath, function() {},
                                function(error) {
      if (listItem)
        listItem.removeAttribute('disabled');
      self.alert.show(strf('UNMOUNT_FAILED', error.message));
    });
  };

  FileManager.prototype.updateGDataStyle_ = function(
      listItem, entry, gdata) {
    if (!this.isOnGData() || !gdata)
      return;

    if (!entry.isDirectory) {
      if (!gdata.availableOffline)
        listItem.classList.add('dim-offline');
      if (!gdata.availableWhenMetered)
        listItem.classList.add('dim-metered');
    }

    if (gdata.driveApps.length > 0) {
      var url = gdata.driveApps[0].docIcon;
      var iconDiv = listItem.querySelector('.detail-icon');
      if (url && iconDiv) {
        iconDiv.style.backgroundImage = 'url(' + url + ')';
      }
    }
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
      label.appendChild(this.renderSelectionCheckbox_(entry));
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

    fileName.textContent = entry.name;

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
    div.className = 'size';
    // Unlike other rtl languages, Herbew use MB and writes the unit to the
    // right of the number. We use css trick to workaround this.
    if (navigator.language == 'he')
      div.className = 'align-end-weakrtl';
    this.updateSize_(
        div, entry, this.metadataCache_.getCached(entry, 'filesystem'));

    return div;
  };

  FileManager.prototype.updateSize_ = function(div, entry, filesystemProps) {
    if (!filesystemProps) {
      div.textContent = '...';
    } else if (filesystemProps.size == -1) {
      div.textContent = '--';
    } else if (filesystemProps.size == 0 &&
               FileType.isHosted(entry)) {
      div.textContent = '--';
    } else {
      div.textContent = util.bytesToSi(filesystemProps.size);
    }
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
    div.className = 'type';
    div.textContent = this.getFileTypeString_(entry);
    return div;
  };

  /**
   * @param {Entry} entry File or directory entry.
   * @return {string} Localized string representation of file type.
   */
  FileManager.prototype.getFileTypeString_ = function(entry) {
    var fileType = FileType.getType(entry);
    if (fileType.subtype)
      return strf(fileType.name, fileType.subtype);
    else
      return str(fileType.name);
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
    div.className = 'date';

    this.updateDate_(div,
        this.metadataCache_.getCached(entry, 'filesystem'));
    return div;
  };

  FileManager.prototype.updateDate_ = function(div, filesystemProps) {
    if (!filesystemProps) {
      div.textContent = '...';
      return;
    }

    var modTime = filesystemProps.modificationTime;
    var today = new Date();
    today.setHours(0);
    today.setMinutes(0);
    today.setSeconds(0);
    today.setMilliseconds(0);

    if (modTime >= today &&
        modTime < today.getTime() + MILLISECONDS_IN_DAY) {
      div.textContent = strf('TIME_TODAY', this.timeFormatter_.format(modTime));
    } else if (modTime >= today - MILLISECONDS_IN_DAY && modTime < today) {
      div.textContent = strf('TIME_YESTERDAY',
                             this.timeFormatter_.format(modTime));
    } else {
      div.textContent =
          this.dateFormatter_.format(filesystemProps.modificationTime);
    }
  };

  FileManager.prototype.renderOffline_ = function(entry, columnId, table) {
    var doc = this.document_;
    var div = doc.createElement('div');
    div.className = 'offline';

    if (entry.isDirectory)
      return div;

    var checkbox = this.renderCheckbox_();
    checkbox.classList.add('pin');
    checkbox.addEventListener('click',
                              this.onPinClick_.bind(this, checkbox, entry));
    checkbox.style.display = 'none';
    div.appendChild(checkbox);

    if (this.isOnGData()) {
      this.updateOffline_(
          div, this.metadataCache_.getCached(entry, 'gdata'));
    }
    return div;
  };

  FileManager.prototype.updateOffline_ = function(div, gdata) {
    if (!gdata) return;
    if (gdata.hosted) return;
    var checkbox = div.querySelector('.pin');
    if (!checkbox) return;
    checkbox.style.display = '';
    checkbox.checked = gdata.pinned;
  };

  FileManager.prototype.refreshCurrentDirectoryMetadata_ = function() {
    var entries = this.directoryModel_.getFileList().slice();
    // We don't pass callback here. When new metadata arrives, we have an
    // observer registered to update the UI.

    // TODO(dgozman): refresh content metadata only when modificationTime
    // changed.
    this.metadataCache_.clear(entries, 'filesystem|thumbnail|media');
    this.metadataCache_.get(entries, 'filesystem|thumbnail', null);
    if (this.isOnGData()) {
      this.metadataCache_.clear(entries, 'gdata');
      this.metadataCache_.get(entries, 'gdata', null);
    }
  };

  FileManager.prototype.dailyUpdateModificationTime_ = function() {
    var fileList = this.directoryModel_.getFileList();
    var urls = [];
    for (var i = 0; i < fileList.length; i++) {
      urls.push(fileList.item(i).toURL());
    }
    this.metadataCache_.get(
        fileList.slice(), 'filesystem',
        this.updateMetadataInUI_.bind(this, 'filesystem', urls));

    setTimeout(this.dailyUpdateModificationTime_.bind(this),
               MILLISECONDS_IN_DAY);
  };

  FileManager.prototype.updateMetadataInUI_ = function(
      type, urls, properties) {
    var isDetail = this.listType_ == FileManager.ListType.DETAIL;
    var isThumbnail = this.listType_ == FileManager.ListType.THUMBNAIL;

    var items = {};
    var entries = {};
    var dm = this.directoryModel_.getFileList();
    for (var index = 0; index < dm.length; index++) {
      var listItem = this.currentList_.getListItemByIndex(index);
      if (!listItem) continue;
      var entry = dm.item(index);
      var url = entry.toURL();
      items[url] = listItem;
      entries[url] = entry;
    }

    for (var index = 0; index < urls.length; index++) {
      var url = urls[index];
      if (!(url in items)) continue;
      var listItem = items[url];
      var entry = entries[url];
      var props = properties[index];
      if (type == 'filesystem' && isDetail) {
        this.updateDate_(listItem.querySelector('.date'), props);
        this.updateSize_(listItem.querySelector('.size'), entry, props);
      } else if (type == 'gdata') {
        if (isDetail) {
          var offline = listItem.querySelector('.offline');
          if (offline)  // This column is only present in full page mode.
            this.updateOffline_(offline, props);
        }
        this.updateGDataStyle_(listItem, entry, props);
      } else if (type == 'thumbnail' && isThumbnail) {
        var box = listItem.querySelector('.img-container');
        this.renderThumbnailBox_(entry, false /* fit, not fill */,
                                 null /* callback */, box);
      }
    }
  };

  /**
   * Restore the item which is being renamed while refreshing the file list. Do
   * nothing if no item is being renamed or such an item disappeared.
   *
   * While refreshing file list it gets repopulated with new file entries.
   * There is not a big difference wether DOM items stay the same or not.
   * Except for the item that the user is renaming.
   */
  FileManager.prototype.restoreItemBeingRenamed_ = function() {
    if (!this.isRenamingInProgress())
      return;

    var dm = this.directoryModel_;
    var leadIndex = dm.getFileListSelection().leadIndex;
    if (leadIndex < 0)
      return;

    var leadEntry = dm.getFileList().item(leadIndex);
    if (this.renameInput_.currentEntry.fullPath != leadEntry.fullPath)
      return;

    var leadListItem = this.findListItemForNode_(this.renameInput_);
    if (this.currentList_ == this.table_.list) {
      var props = this.metadataCache_.getCached(leadEntry, 'filesystem');
      this.updateDate_(leadListItem.querySelector('.date'), props);
      this.updateSize_(leadListItem.querySelector('.size'), leadEntry, props);
    }
    this.currentList_.restoreLeadItem(leadListItem);
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
      showBytes: false,
      allGDataFilesPresent: false,
      iconType: null,
      indexes: this.currentList_.selectionModel.selectedIndexes
    };

    if (!selection.indexes.length) {
      this.updateCommonActionButtons_();
      this.updatePreviewPanelVisibility_();
      cr.dispatchSimpleEvent(this, 'selection-summarized');
      return;
    }

    this.previewSummary_.textContent = str('COMPUTING_SELECTION');
    var thumbnails = [];

    var pendingFiles = [];
    var thumbnailCount = 0;
    var thumbnailLoaded = -1;
    var forcedShowTimeout = null;
    var thumbnailsHaveZoom = false;
    var self = this;

    function showThumbnails() {
      // have-zoom class may be updated twice: then timeout exceeds and then
      // then all images loaded.
      if (self.selection == selection)
        setClassIf(self.previewThumbnails_, 'has-zoom', thumbnailsHaveZoom);

      if (forcedShowTimeout === null)
        return;
      clearTimeout(forcedShowTimeout);
      forcedShowTimeout = null;

      // Selection could change while images are loading.
      if (self.selection == selection) {
        removeChildren(self.previewThumbnails_);
        for (var i = 0; i < thumbnails.length; i++)
          self.previewThumbnails_.appendChild(thumbnails[i]);
      }
    }

    function onThumbnailLoaded() {
      thumbnailLoaded++;
      if (thumbnailLoaded == thumbnailCount)
        showThumbnails();
    }

    var thumbnailClickHandler =
        this.dispatchDefaultTask_.bind(this, selection);

    for (var i = 0; i < selection.indexes.length; i++) {
      var entry = this.directoryModel_.getFileList().item(selection.indexes[i]);
      if (!entry)
        continue;

      selection.entries.push(entry);
      selection.urls.push(entry.toURL());

      if (thumbnailCount < MAX_PREVIEW_THUMBAIL_COUNT) {
        var box = this.document_.createElement('div');
        box.className = 'thumbnail';
        if (thumbnailCount == 0) {
          var zoomed = this.document_.createElement('div');
          zoomed.hidden = true;
          thumbnails.push(zoomed);
          function onFirstThumbnailLoaded(img, transform) {
            if (self.decorateThumbnailZoom_(zoomed, img, transform)) {
              zoomed.hidden = false;
              thumbnailsHaveZoom = true;
            }
            onThumbnailLoaded();
          }
          var thumbnail = this.renderThumbnailBox_(entry, true,
                                                   onFirstThumbnailLoaded);
          zoomed.addEventListener('click', thumbnailClickHandler);
        } else {
          var thumbnail = this.renderThumbnailBox_(entry, true,
                                                   onThumbnailLoaded);
        }
        thumbnailCount++;
        box.appendChild(thumbnail);
        box.style.zIndex = MAX_PREVIEW_THUMBAIL_COUNT + 1 - i;
        box.addEventListener('click', thumbnailClickHandler);

        thumbnails.push(box);
      }

      if (selection.iconType == null) {
        selection.iconType = FileType.getIcon(entry);
      } else if (selection.iconType != 'unknown') {
        var iconType = FileType.getIcon(entry);
        if (selection.iconType != iconType)
          selection.iconType = 'unknown';
      }

      if (entry.isFile) {
        selection.fileCount += 1;
        selection.showBytes |= !FileType.isHosted(entry);
      } else {
        selection.directoryCount += 1;
      }
      selection.totalCount++;
    }

    // Now this.selection is complete. Update buttons.
    this.updateCommonActionButtons_();
    this.updatePreviewPanelVisibility_();
    forcedShowTimeout = setTimeout(showThumbnails,
        FileManager.THUMBNAIL_SHOW_DELAY);
    onThumbnailLoaded();

    if (this.dialogType_ == FileManager.DialogType.FULL_PAGE) {
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

    this.metadataCache_.get(selection.entries, 'filesystem', function(props) {
      for (var index = 0; index < selection.entries.length; index++) {
        var filesystem = props[index];

        if (entry.isFile) {
          selection.bytes += filesystem.size;
        }
      }

      this.dispatchEvent(new cr.Event('selection-summarized'));
    }.bind(this));

    if (this.isOnGData()) {
      function predicate(p) {
        return !(p && p.availableOffline);
      }
      this.metadataCache_.get(selection.urls, 'gdata', function(props) {
        selection.allGDataFilesPresent =
            props.filter(predicate).length == 0;
        this.updateOkButton_();
      }.bind(this));
    }
  };

  /**
    * Check if all the files in the current selection are available. The only
    * case when files might be not available is when the selection contains
    * uncached GData files and the browser is offline.
    * @return {boolean} True if all files in the current selection are
    *                   available.
    */
  FileManager.prototype.isSelectionAvailable = function() {
    return !this.isOnGData() ||
        !this.isOffline() ||
        this.selection.allGDataFilesPresent;
  };

  /**
   * Creates enlarged image for a bottom pannel thumbnail.
   * Image's assumed to be just loaded and not inserted into the DOM.
   *
   * @param {HTMLElement} largeImageBox DIV element to decorate.
   * @param {HTMLElement} img Loaded image.
   * @param {Object} transform Image transformation description.
   * @return {boolean} True if zoomed image is present.
   */
  FileManager.prototype.decorateThumbnailZoom_ = function(largeImageBox,
                                                          img, transform) {
    var width = img.width;
    var height = img.height;
    var THUMBNAIL_SIZE = 45;
    if (width < THUMBNAIL_SIZE * 2 && height < THUMBNAIL_SIZE * 2)
      return false;

    var scale = Math.min(1,
        IMAGE_HOVER_PREVIEW_SIZE / Math.max(width, height));

    var imageWidth = Math.round(width * scale);
    var imageHeight = Math.round(height * scale);

    var largeImage = this.document_.createElement('img');
    if (scale < 0.3) {
      // Scaling large images kills animation. Downscale it in advance.

      // Canvas scales images with liner interpolation. Make a larger
      // image (but small enough to not kill animation) and let IMG
      // scale it smoothly.
      var INTERMEDIATE_SCALE = 3;
      var canvas = this.document_.createElement('canvas');
      canvas.width = imageWidth * INTERMEDIATE_SCALE;
      canvas.height = imageHeight * INTERMEDIATE_SCALE;
      var ctx = canvas.getContext('2d');
      ctx.drawImage(img, 0, 0, canvas.width, canvas.height);
      // Using bigger than default compression reduces image size by
      // several times. Quality degradation compensated by greater resolution.
      largeImage.src = canvas.toDataURL('image/jpeg', 0.6);
    } else {
      largeImage.src = img.src;
    }
    largeImageBox.className = 'popup';

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

    util.applyTransform(largeImage, transform);

    largeImageBox.appendChild(largeImage);
    largeImageBox.style.zIndex = 1000;
    return true;
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
      self.previewThumbnails_.textContent = '';
      self.onResize_();
    }

    function setVisibility(visibility) {
      panel.setAttribute('visibility', visibility);
    }
  };

  FileManager.prototype.isOnGData = function() {
    return this.directoryModel_.getCurrentRootType() ==
           DirectoryModel.RootType.GDATA;
  };

  /**
   * Creates combobox item based on task.
   * @param {Object} task Task to convert.
   * @return {Object} Item appendable to combobox drop-down list.
   */
  FileManager.prototype.createComboboxItem_ = function(task) {
    return { label: task.title, iconUrl: task.iconUrl, task: task };
  }

  /**
   * Callback called when tasks for selected files are determined.
   * @param {Object} selection Selection is passed here, since this.selection
   *     can change before tasks were found, and we should be accurate.
   * @param {Array.<Task>} tasksList The tasks list.
   */
  FileManager.prototype.onTasksFound_ = function(selection, tasksList) {
    this.taskItems_.clear();

    var defaultTask = null;
    var dropDownItems = [];

    for (var i = 0; i < tasksList.length; i++) {
      var task = tasksList[i];

      // Tweak images, titles of internal tasks.
      var task_parts = task.taskId.split('|');
      if (task_parts[0] == this.getExtensionId_()) {
        if (task_parts[1] == 'play') {
          // TODO(serya): This hack needed until task.iconUrl is working
          //             (see GetFileTasksFileBrowserFunction::RunImpl).
          task.iconUrl =
              chrome.extension.getURL('images/filetype_audio.png');
          task.title = str('ACTION_LISTEN');
        } else if (task_parts[1] == 'mount-archive') {
          task.iconUrl =
              chrome.extension.getURL('images/filetype_archive.png');
          task.title = str('MOUNT_ARCHIVE');
        } else if (task_parts[1] == 'gallery') {
          task.title = str('ACTION_OPEN');
          task.iconUrl =
              chrome.extension.getURL('images/filetype_image.png');
        } else if (task_parts[1] == 'watch') {
          task.iconUrl =
              chrome.extension.getURL('images/filetype_video.png');
          task.title = str('ACTION_WATCH');
        } else if (task_parts[1] == 'open-hosted') {
          if (selection.urls.length > 1) {
            task.iconUrl =
                chrome.extension.getURL('images/filetype_generic.png');
          } else {
            // Use specific icon.
            var icon = FileType.getIcon(selection.urls[0]);
            task.iconUrl =
                chrome.extension.getURL('images/filetype_' + icon + '.png');
          }
          task.title = str('ACTION_OPEN');
        } else if (task_parts[1] == 'view-pdf') {
          // Do not render this task if disabled.
          if (!loadTimeData.getBoolean('PDF_VIEW_ENABLED'))
            continue;

          task.iconUrl =
              chrome.extension.getURL('images/filetype_pdf.png');
          task.title = str('ACTION_VIEW');
        } else if (task_parts[1] == 'view-in-browser') {
          task.iconUrl =
              chrome.extension.getURL('images/filetype_generic.png');
          task.title = str('ACTION_VIEW');
        } else if (task_parts[1] == 'install-crx') {
          task.iconUrl =
              chrome.extension.getURL('images/filetype_generic.png');
          task.title = str('INSTALL_CRX');
        }
      }

      if (defaultTask == null) {
        defaultTask = task;
        this.taskItems_.defaultItem = this.createComboboxItem_(task);
        task.title = task.title + ' ' + str('DEFAULT_ACTION_LABEL');
        dropDownItems.push(this.createComboboxItem_(task));
      } else {
        dropDownItems.push(this.createComboboxItem_(task));
      }
    }

    var defaultIdx = 0;
    this.taskItems_.hidden = dropDownItems.length == 0;

    if (dropDownItems.length > 1) {
      dropDownItems.sort(function(a, b) {
        return a.label.localeCompare(b.label);
      });

      for (var j = 0; j < dropDownItems.length; j++) {
        this.taskItems_.addDropDownItem(dropDownItems[j]);
        if (dropDownItems[j].task.taskId == defaultTask.taskId) {
          defaultIdx = j;
        }
      }

      this.taskItems_.addSeparator();
      this.taskItems_.addDropDownItem({
            label: str('CHANGE_DEFAULT_MENU_ITEM'),
            items: dropDownItems,
            defaultIdx: defaultIdx
          });
    }

    selection.tasksList = tasksList;
    if (selection.dispatchDefault) {
      // We got a request to dispatch the default task for the selection.
      selection.dispatchDefault = false;
      this.dispatchDefaultTask_(selection);
    }
  };

  FileManager.prototype.getExtensionId_ = function() {
    return chrome.extension.getURL('').split('/')[2];
  };

  FileManager.prototype.onExternalLinkClick_ = function(url) {
    chrome.tabs.create({url: url});
    if (this.dialogType_ != FileManager.DialogType.FULL_PAGE) {
      this.onCancel_();
    }
  };

  /**
   * Task combobox handler.
   *
   * @param {Object} event Event containing task which was clicked.
   */
  FileManager.prototype.onTaskItemClicked_ = function(event) {
    if (event.item.task) {
      // Task field doesn't exist on change-default dropdown item.
      this.dispatchFileTask_(event.item.task.taskId, this.selection.urls);
    } else {
      var extensions = [];

      for (var i = 0; i < this.selection.urls.length; i++) {
        var match = /\.(\w+)$/g.exec(this.selection.urls[i]);
        if (match) {
          var ext = match[1].toUpperCase();
          if (extensions.indexOf(ext) == -1) {
            extensions.push(ext);
          }
        }
      }

      var format = '';

      if (extensions.length != 1) {
        format = extensions[0];
      }

      // Change default was clicked. We should open "change default" dialog.
      this.defaultTaskPicker.show(
          strf('CHANGE_DEFAULT_CAPTION', format),
          event.item.items, event.item.defaultIdx,
          this.onDefaultTaskDone_.bind(this));
    }
  };


  /**
   * Set's given task as default, when this task is applicable.
   * @param {Object} task Task to set as default.
   */
  FileManager.prototype.onDefaultTaskDone_ = function(task) {
    chrome.fileBrowserPrivate.setDefaultTask(task.taskId);

    chrome.fileBrowserPrivate.getFileTasks(
            this.selection.urls,
            this.onTasksFound_.bind(this, this.selection));
  }

  /**
   * Dispatches default task for the current selection. If tasks are not ready
   * yet, dispatches after task are available.
   * @param {Object=} opt_selection Object similar to this.selection.
   */
  FileManager.prototype.dispatchDefaultTask_ = function(opt_selection) {
    var selection = opt_selection || this.selection;

    if (selection.urls.length == 0)
      return;

    if (!selection.tasksList) {
      // Don't have tasks list yet - wait until get it.
      selection.dispatchDefault = true;
      return;
    }

    if (selection.tasksList.length > 0) {
      this.dispatchFileTask_(selection.tasksList[0].taskId, selection.urls);
      return;
    }

    if (selection.urls.length == 1) {
      // We don't have tasks, so try the default action (open a directory or
      // show a file in a browser tab).
      // We only do that for single selection to avoid confusion.
      if (selection.entries[0].isDirectory) {
        this.onDirectoryAction(selection.entries[0]);
      } else {
        var callback = function(success) {
          if (!success)
            this.alert.showHtml(
                unescape(selection.entries[0].name),
                strf('NO_ACTION_FOR_FILE', NO_ACTION_FOR_FILE_URL),
                function() {});
        }.bind(this);

        this.executeIfAvailable_(selection.urls, function(urls) {
          chrome.fileBrowserPrivate.viewFiles(urls, 'default', callback);
        });
      }
    }
  };

  FileManager.prototype.dispatchInternalTask_ = function(task, urls) {
     this.dispatchFileTask_(this.getExtensionId_() + '|' + task, urls);
  };

  FileManager.prototype.dispatchFileTask_ = function(taskId, urls) {
    this.executeIfAvailable_(urls, function(urls) {
      chrome.fileBrowserPrivate.executeTask(taskId, urls);
      var task_parts = taskId.split('|');
      if (task_parts[0] == this.getExtensionId_()) {
        // For internal tasks we do not listen to the event to avoid
        // handling the same task instance from multiple tabs.
        // So, we manually execute the task.
        this.onFileTaskExecute_(task_parts[1], urls);
      }
    }.bind(this));
  };

  FileManager.prototype.executeIfAvailable_ = function(urls, callback) {
    if (this.isOnGData() && this.isOffline()) {
      this.metadataCache_.get(urls, 'gdata', function(props) {
        if (props.filter(function(p) {return !p.availableOffline}).length) {
          this.alert.showHtml(
              str('OFFLINE_HEADER'),
              props[0].hosted ?
                strf(
                    urls.length == 1 ?
                        'HOSTED_OFFLINE_MESSAGE' :
                        'HOSTED_OFFLINE_MESSAGE_PLURAL') :
                strf(
                    urls.length == 1 ?
                        'OFFLINE_MESSAGE' :
                        'OFFLINE_MESSAGE_PLURAL',
                    str('OFFLINE_COLUMN_LABEL')));
          return;
        }
        callback(urls);
      }.bind(this));
    } else if (this.isOnGData() && this.isOnMeteredConnection()) {
      this.metadataCache_.get(urls, 'gdata', function(gdataProps) {
        if (gdataProps.filter(
            function(p) { return !p.availableWhenMetered}).length) {
          this.metadataCache_.get(urls, 'filesystem', function(fileProps) {
            var sizeToDownload = 0;
            for (var i = 0; i != urls.length; i++) {
              if (!gdataProps[i].availableWhenMetered)
                sizeToDownload += fileProps[i].size;
            }
            this.confirm.show(
                strf(
                    urls.length == 1 ?
                        'CONFIRM_MOBILE_DATA_USE' :
                        'CONFIRM_MOBILE_DATA_USE_PLURAL',
                    util.bytesToSi(sizeToDownload)),
                callback.bind(null, urls));
          }.bind(this));
          return;
        }
        callback(urls);
      }.bind(this));
    } else {
      callback(urls);
    }
  };

  FileManager.prototype.updateNetworkStateAndGDataPreferences_ = function(
      callback) {
    var self = this;
    var downcount = 2;
    function done() {
      if (--downcount == 0)
        callback();
    }

    chrome.fileBrowserPrivate.getGDataPreferences(function(prefs) {
      self.gdataPreferences_ = prefs;
      done();
    });

    chrome.fileBrowserPrivate.getNetworkConnectionState(function(netwokState) {
      self.networkState_ = netwokState;
      done();
    });
  };

  FileManager.prototype.onNetworkStateOrGDataPreferencesChanged_ = function() {
    var self = this;
    this.updateNetworkStateAndGDataPreferences_(function() {
      var gdata = self.gdataPreferences_;
      var network = self.networkState_;

      self.directoryModel_.setGDataEnabled(self.isGDataEnabled());
      self.directoryModel_.setOffline(!network.online);

      if (self.isGDataEnabled())
        self.setupGDataWelcome_();
      else
        self.cleanupGDataWelcome_();

      if (gdata.cellularDisabled)
        self.syncButton.setAttribute('checked', '');
      else
        self.syncButton.removeAttribute('checked');

      if (self.hostedButton.hasAttribute('checked') !=
          gdata.hostedFilesDisabled && self.isOnGData()) {
        self.directoryModel_.rescan();
      }

      if (!gdata.hostedFilesDisabled)
        self.hostedButton.setAttribute('checked', '');
      else
        self.hostedButton.removeAttribute('checked');

      if (network.online) {
        if (gdata.cellularDisabled && network.type == 'cellular')
          self.dialogContainer_.setAttribute('connection', 'metered');
        else
          self.dialogContainer_.removeAttribute('connection');
      } else {
        self.dialogContainer_.setAttribute('connection', 'offline');
      }
    });
  };

  FileManager.prototype.isOnMeteredConnection = function() {
    return this.gdataPreferences_.cellularDisabled &&
           this.networkState_.online &&
           this.networkState_.type == 'cellular';
  };

  FileManager.prototype.isOffline = function() {
    return !this.networkState_.online;
  };

  FileManager.prototype.isGDataEnabled = function() {
    return !('driveEnabled' in this.gdataPreferences_) ||
           this.gdataPreferences_.driveEnabled;
  };

  FileManager.prototype.isOnReadonlyDirectory = function() {
    return this.directoryModel_.isReadOnly();
  };

  FileManager.prototype.onExternallyUnmounted_ = function(event) {
    if (event.mountPath == this.directoryModel_.getCurrentRootPath()) {
      if (this.closeOnUnmount_) {
        // If the file manager opened automatically when a usb drive inserted,
        // user have never changed current volume (that implies the current
        // directory is still on the device) then close this tab.
        chrome.tabs.getCurrent(function(tab) {
          chrome.tabs.remove(tab.id);
        });
      }
    }
  };

  /**
   * Event handler called when some internal task should be executed.
   */
  FileManager.prototype.onFileTaskExecute_ = function(id, urls) {
    if (id == 'play') {
      var position = 0;
      if (urls.length == 1) {
        // If just a single audio file is selected pass along every audio file
        // in the directory.
        var selectedUrl = urls[0];
        urls = this.getAllUrlsInCurrentDirectory_().filter(FileType.isAudio);
        position = urls.indexOf(selectedUrl);
      }
      chrome.mediaPlayerPrivate.play(urls, position);
    } else if (id == 'mount-archive') {
      var self = this;
      var tracker = this.directoryModel_.createDirectoryChangeTracker();
      tracker.start();
      this.resolveSelectResults_(urls, function(urls) {
        for (var index = 0; index < urls.length; ++index) {
          // TODO(kaznacheev): Incapsulate URL to path conversion.
          var path =
              /^filesystem:[\w-]*:\/\/[\w]*\/(external|persistent)(\/.*)$/.
                  exec(urls[index])[2];
          if (!path)
            continue;
          path = decodeURIComponent(path);
          self.volumeManager_.mountArchive(path, function(mountPath) {
            console.log('Mounted at: ', mountPath);
            tracker.stop();
            if (!tracker.hasChanged)
              self.directoryModel_.changeDirectory(mountPath);
          }, function(error) {
            tracker.stop();
            var namePos = path.lastIndexOf('/');
            self.alert.show(strf('ARCHIVE_MOUNT_FAILED',
                                 path.substr(namePos + 1), error));
          });
        }
      });
    } else if (id == 'format-device') {
      this.confirm.show(str('FORMATTING_WARNING'), function() {
        chrome.fileBrowserPrivate.formatDevice(urls[0]);
      });
    } else if (id == 'gallery') {
      this.openGallery_(urls);
    } else if (id == 'view-pdf' || id == 'view-in-browser' ||
        id == 'install-crx' || id == 'open-hosted' || id == 'watch') {
      chrome.fileBrowserPrivate.viewFiles(urls, id, function(success) {
        if (!success)
          console.error('chrome.fileBrowserPrivate.viewFiles failed', urls);
      });
    }
  };

  /**
   * Show a modal-like file viewer/editor on top of the File Manager UI.
   *
   * @param {HTMLElement} popup Popup element.
   * @param {function} closeCallback Function to call after the popup is closed.
   */
  FileManager.prototype.openFilePopup_ = function(popup, closeCallback) {
    this.closeFilePopup_();
    this.filePopup_ = popup;
    this.filePopupCloseCallback_ = closeCallback;
    this.dialogDom_.appendChild(this.filePopup_);
    this.filePopup_.focus();
  };

  FileManager.prototype.closeFilePopup_ = function() {
    if (this.filePopup_) {
      this.dialogDom_.removeChild(this.filePopup_);
      this.filePopup_ = null;
      if (this.filePopupCloseCallback_) {
        this.filePopupCloseCallback_();
        this.filePopupCloseCallback_ = null;
      }
      this.refocus();
    }
  };

  FileManager.prototype.getAllUrlsInCurrentDirectory_ = function() {
    var urls = [];
    var fileList = this.directoryModel_.getFileList();
    for (var i = 0; i != fileList.length; i++) {
      urls.push(fileList.item(i).toURL());
    }
    return urls;
  };

  FileManager.prototype.getShareActions_ = function(urls, callback) {
    chrome.fileBrowserPrivate.getFileTasks(urls, function(tasks) {
      var shareActions = [];
      for (var index = 0; index < tasks.length; index++) {
        var task = tasks[index];
        var task_parts = task.taskId.split('|');
        if (task_parts[0] != this.getExtensionId_()) {
          // Add callback, so gallery can execute the task.
          task.execute = this.dispatchFileTask_.bind(this, task.taskId);
          shareActions.push(task);
        }
      }
      callback(shareActions);
    }.bind(this));
  };

  /**
   * Opens provided urls in the gallery.
   *
   * @param {string} selectedUrl Url of the item that should initially be
   *     selected.
   * @param {Array.<string>} urls List of all the urls that will be shown in
   *     the gallery.
   */
  FileManager.prototype.openGallery_ = function(urls) {
    var self = this;

    var singleSelection = urls.length == 1;

    var selectedUrl;
    if (singleSelection && FileType.isImage(urls[0])) {
      // Single image item selected. Pass to the Gallery as a selected.
      selectedUrl = urls[0];
      // Pass along every image and video in the directory so that it shows up
      // in the ribbon.
      // We do not do that if a single video is selected because the UI is
      // cleaner without the ribbon.
      urls = this.getAllUrlsInCurrentDirectory_().filter(
          FileType.isImageOrVideo);
    } else {
      // Pass just the selected items, select the first entry.
      selectedUrl = urls[0];
    }

    var galleryFrame = this.document_.createElement('iframe');
    galleryFrame.className = 'overlay-pane';
    galleryFrame.scrolling = 'no';
    galleryFrame.setAttribute('webkitallowfullscreen', true);

    var dirPath = this.directoryModel_.getCurrentDirPath();
    // Push a temporary state which will be replaced every time an individual
    // item is selected in the Gallery.
    this.updateLocation_(false /*push*/, dirPath);

    galleryFrame.onload = function() {
      self.show_();
      galleryFrame.contentWindow.ImageUtil.metrics = metrics;
      galleryFrame.contentWindow.FileType = FileType;
      galleryFrame.contentWindow.util = util;

      var readonly = self.isOnReadonlyDirectory();
      var currentDir = self.directoryModel_.getCurrentDirEntry();
      var downloadsDir = self.directoryModel_.getRootsList().item(0);

      var gallerySelection;
      var context = {
        // We show the root label in readonly warning (e.g. archive name).
        readonlyDirName:
            readonly ?
                (self.isOnGData() ?
                    self.getRootLabel_(
                        DirectoryModel.getRootPath(currentDir.fullPath)) :
                    self.directoryModel_.getRootName()) :
                null,
        saveDirEntry: readonly ? downloadsDir : currentDir,
        metadataCache: self.metadataCache_,
        getShareActions: self.getShareActions_.bind(self),
        onNameChange: function(name) {
          self.document_.title = gallerySelection = name;
          self.updateLocation_(true /*replace*/, dirPath + '/' + name);
        },
        onClose: function() {
          if (singleSelection)
            self.directoryModel_.selectEntry(gallerySelection);
          history.back(1);
        },
        displayStringFunction: strf
      };
      galleryFrame.contentWindow.Gallery.open(context, urls, selectedUrl);
    };

    galleryFrame.src = 'gallery.html';
    this.openFilePopup_(galleryFrame, this.updateTitle_.bind(this));
  };

  /**
   * Update the breadcrumb display to reflect the current directory.
   */
  FileManager.prototype.updateBreadcrumbs_ = function() {
    var bc = this.dialogDom_.querySelector('.breadcrumbs');
    removeChildren(bc);

    var rootPath = this.directoryModel_.getCurrentRootPath();
    var relativePath = this.directoryModel_.getCurrentDirPath().
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
      div.textContent = i == 0 ? this.getRootLabel_(path) : pathName;

      path = path + '/';
      div.path = path;

      bc.appendChild(div);

      if (i == pathNames.length - 1) {
        div.classList.add('breadcrumb-last');
      } else {
        div.addEventListener('click', this.onBreadcrumbClick_.bind(this));
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
        this.directoryModel_.getCurrentDirPath();
  };

  /**
   * Return URL of the current directory or null.
   */
  FileManager.prototype.getCurrentDirectoryURL = function() {
    return this.directoryModel_ &&
        this.directoryModel_.getCurrentDirEntry().toURL();
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

  FileManager.prototype.onDeleteButtonClick_ = function(event) {
    this.deleteEntries(this.selection.entries);
    event.preventDefault();
    event.stopPropagation();
  },

  FileManager.prototype.onDeleteButtonKeyPress_ = function(event) {
    switch (util.getKeyModifiers(event) + event.keyCode) {
      case '13':  // Enter
      case '32':  // Space
        this.deleteEntries(this.selection.entries);
        event.preventDefault();
        event.stopPropagation();
        break;
    }
  },

  FileManager.prototype.blinkSelection = function() {
    if (!this.selection || this.selection.totalCount == 0)
      return;

    for (var i = 0; i < this.selection.entries.length; i++) {
      var selectedIndex = this.selection.indexes[i];
      var listItem = this.currentList_.getListItemByIndex(selectedIndex);
      if (listItem)
        this.blinkListItem_(listItem);
    }
  };

  FileManager.prototype.blinkListItem_ = function(listItem) {
    listItem.classList.add('blink');
    setTimeout(function() {
      listItem.classList.remove('blink');
    }, 100);
  };

  /**
   * Update the selection summary UI when the selection summarization completes.
   */
  FileManager.prototype.onSelectionSummarized_ = function() {
    var selection = this.selection;
    var bytes = util.bytesToSi(selection.bytes);
    var text = '';
    if (selection.totalCount == 0) {
      // We dont want to change the string during preview panel animating away.
      return;
    } else if (selection.fileCount == 1 && selection.directoryCount == 0) {
      text = selection.entries[0].name;
      if (selection.showBytes) text += ', ' + bytes;
    } else if (selection.fileCount == 0 && selection.directoryCount == 1) {
      text = selection.entries[0].name;
    } else if (selection.directoryCount == 0) {
      text = strf('MANY_FILES_SELECTED', selection.fileCount, bytes);
      // TODO(dgozman): change the string to not contain ", $2".
      if (!selection.showBytes) text = text.substring(0, text.lastIndexOf(','));
    } else if (selection.fileCount == 0) {
      text = strf('MANY_DIRECTORIES_SELECTED', selection.directoryCount);
    } else {
      text = strf('MANY_ENTRIES_SELECTED', selection.totalCount, bytes);
      // TODO(dgozman): change the string to not contain ", $2".
      if (!selection.showBytes) text = text.substring(0, text.lastIndexOf(','));
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

  FileManager.prototype.onCheckboxClick_ = function(event) {
    var sm = this.directoryModel_.getFileListSelection();
    var listIndex = this.findListItemForEvent_(event).listIndex;
    sm.setIndexSelected(listIndex, event.target.checked);
    sm.leadIndex = listIndex;
    if (sm.anchorIndex == -1)
      sm.anchorIndex = listIndex;
  };

  FileManager.prototype.onPinClick_ = function(checkbox, entry, event) {
    // TODO(dgozman): revisit this method when gdata properties updated event
    // will be available.
    var self = this;
    var pin = checkbox.checked;
    function callback(props) {
      var fileProps = props[0];
      if (fileProps.errorCode && pin) {
        self.metadataCache_.get(entry, 'filesystem', function(filesystem) {
          self.alert.showHtml(str('GDATA_OUT_OF_SPACE_HEADER'),
              strf('GDATA_OUT_OF_SPACE_MESSAGE',
                  unescape(entry.name),
                  util.bytesToSi(filesystem.size)));
        });
      }
      // We don't have update events yet, so clear the cached data.
      self.metadataCache_.clear(entry, 'gdata');
      checkbox.checked = fileProps.isPinned;
    }
    chrome.fileBrowserPrivate.pinGDataFile([entry.toURL()], pin, callback);
    event.preventDefault();
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

    setTimeout(this.onSelectionChangeComplete_.bind(this, event), 0);
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
      // In SELECT_FOLDER mode, we allow to select current directory
      // when nothing is selected.
      selectable = this.selection.directoryCount <= 1 &&
          this.selection.fileCount == 0;
    } else if (this.dialogType_ == FileManager.DialogType.SELECT_OPEN_FILE) {
      selectable = (this.isSelectionAvailable() &&
                    this.selection.directoryCount == 0 &&
                    this.selection.fileCount == 1);
    } else if (this.dialogType_ ==
               FileManager.DialogType.SELECT_OPEN_MULTI_FILE) {
      selectable = (this.isSelectionAvailable() &&
                    this.selection.directoryCount == 0 &&
                    this.selection.fileCount >= 1);
    } else if (this.dialogType_ == FileManager.DialogType.SELECT_SAVEAS_FILE) {
      if (this.isOnReadonlyDirectory()) {
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
   * Handle a double-click or tap event on an entry in the detail list.
   *
   * @param {Event} event The click event.
   */
  FileManager.prototype.onDetailDoubleClickOrTap_ = function(event) {
    if (this.isRenamingInProgress()) {
      // Don't pay attention to double clicks during a rename.
      return;
    }

    var listItem = this.findListItemForEvent_(event);
    if (!listItem || !listItem.selected ||
        this.selection.totalCount != 1) {
      return;
    }
    var entry = this.selection.entries[0];

    if (entry.isDirectory) {
      return this.onDirectoryAction(entry);
    }

    this.dispatchSelectionAction_();
  };

  FileManager.prototype.dispatchSelectionAction_ = function() {
    if (this.dialogType_ == FileManager.DialogType.FULL_PAGE) {
      this.dispatchDefaultTask_();
      return true;
    }
    if (!this.okButton_.disabled) {
      this.onOk_();
      return true;
    }
    return false;
  };

  /**
   * Executes directory action (i.e. changes directory).
   *
   * @param {DirectoryEntry} entry Directory entry to which directory should be
   *                               changed.
   */
  FileManager.prototype.onDirectoryAction = function(entry) {
    var mountError = this.volumeManager_.getMountError(
        DirectoryModel.getRootPath(entry.fullPath));
    if (mountError == VolumeManager.Error.UNKNOWN_FILESYSTEM) {
      return this.showButter(str('UNKNOWN_FILESYSTEM_WARNING'));
    } else if (mountError == VolumeManager.Error.UNSUPPORTED_FILESYSTEM) {
      return this.showButter(str('UNSUPPORTED_FILESYSTEM_WARNING'));
    }

    return this.directoryModel_.changeDirectory(entry.fullPath);
  };

  /**
   * Show or hide the "Low disk space" warning.
   * @param {boolean} show True if the box need to be shown.
   */
  FileManager.prototype.showLowDiskSpaceWarning_ = function(show) {
    var box = this.dialogDom_.querySelector('.downloads-warning');

    if (box.hidden == !show) return;

    if (show) {
      var html = util.htmlUnescape(str('DOWNLOADS_DIRECTORY_WARNING'));
      box.lastElementChild.innerHTML = html;
      var link = box.querySelector('a');
      link.addEventListener('click',
          this.onExternalLinkClick_.bind(this, DOWNLOADS_FAQ_URL));
    } else {
      box.lastElementChild.innerHTML = '';
    }

    box.hidden = !show;
    this.requestResize_(100);
  };

  /**
   * Update the location in the address bar.
   *
   * @param {boolean} replace True if the history state should be replaced,
   *                          false if pushed.
   * @param {string} path Path to be put in the address bar after the hash.
   */
  FileManager.prototype.updateLocation_ = function(replace, path) {
    var location = document.location.origin + document.location.pathname + '#' +
                   encodeURI(path);
    //TODO(kaznacheev): Fix replaceState for component extensions. Currently it
    //does not replace the content of the address bar.
    if (replace)
      history.replaceState(undefined, path, location);
    else
      history.pushState(undefined, path, location);
  },

  /**
   * Update the tab title.
   */
  FileManager.prototype.updateTitle_ = function() {
    this.document_.title = this.getCurrentDirectory().substr(1).replace(
        new RegExp('^' + DirectoryModel.GDATA_DIRECTORY),
        str('GDATA_DIRECTORY_LABEL'));
  },

  /**
   * Updates search box value when directory gets changed.
   */
  FileManager.prototype.updateSearchBoxOnDirChange_ = function() {
    var searchBox = this.dialogDom_.querySelector('#search-box');
    if (!searchBox.disabled)
      searchBox.value = '';
  },

  /**
   * Update the UI when the current directory changes.
   *
   * @param {cr.Event} event The directory-changed event.
   */
  FileManager.prototype.onDirectoryChanged_ = function(event) {
    this.updateCommonActionButtons_();
    this.updateOkButton_();
    this.updateBreadcrumbs_();
    this.updateColumnModel_();
    this.updateSearchBoxOnDirChange_();

    this.updateLocation_(event.initial, this.getCurrentDirectory());
    this.checkFreeSpace_(this.getCurrentDirectory());

    if (this.closeOnUnmount_ && !event.initial &&
          DirectoryModel.getRootPath(event.previousDirEntry.fullPath) !=
          DirectoryModel.getRootPath(event.newDirEntry.fullPath)) {
      this.closeOnUnmount_ = false;
    }

    this.updateTitle_();
    this.updateGDataUnmountedPanel_();
    if (this.isOnGData())
      this.unmountedPanel_.classList.remove('retry-enabled');
  };

  FileManager.prototype.findListItemForEvent_ = function(event) {
    return this.findListItemForNode_(event.srcElement);
  };

  FileManager.prototype.findListItemForNode_ = function(node) {
    var item = this.currentList_.getListItemAncestor(node);
    // TODO(serya): list should check that.
    return item && this.currentList_.isItem(item) ? item : null;
  };

  /**
   * Unload handler for the page.  May be called manually for the file picker
   * dialog, because it closes by calling extension API functions that do not
   * return.
   */
  FileManager.prototype.onUnload_ = function() {
    this.fileWatcher_.stop();
  };

  FileManager.prototype.initiateRename_ = function() {
    var item = this.currentList_.ensureLeadItemExists();
    if (!item)
      return;
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

    // Do not move selection or lead item in list during rename.
    if (event.keyIdentifier == 'Up' || event.keyIdentifier == 'Down') {
      event.stopPropagation();
    }

    switch (util.getKeyModifiers(event) + event.keyCode) {
      case '27':  // Escape
        this.cancelRename_();
        event.preventDefault();
        break;

      case '13':  // Enter
        this.commitRename_();
        event.preventDefault();
        break;
    }
  };

  FileManager.prototype.onRenameInputBlur_ = function(event) {
    if (this.isRenamingInProgress() && !this.renameInput_.validation_)
      this.commitRename_();
  };

  FileManager.prototype.commitRename_ = function() {
    var input = this.renameInput_;
    var entry = input.currentEntry;
    var newName = input.value;

    if (newName == entry.name) {
      this.cancelRename_();
      return;
    }

    var nameNode = this.findListItemForNode_(this.renameInput_).
                   querySelector('.filename-label');

    input.validation_ = true;
    function validationDone() {
      input.validation_ = false;
      // Alert dialog restores focus unless the item removed from DOM.
      if (this.document_.activeElement != input)
        this.cancelRename_();
    }

    if (!this.validateFileName_(newName, validationDone.bind(this)))
      return;

    function onError(err) {
      this.alert.show(strf('ERROR_RENAMING', entry.name,
                           getFileErrorString(err.code)));
    }

    this.cancelRename_();
    // Optimistically apply new name immediately to avoid flickering in
    // case of success.
    nameNode.textContent = newName;

    this.directoryModel_.doesExist(entry, newName, function(exists, isFile) {
      if (!exists) {
        this.directoryModel_.renameEntry(entry, newName, onError.bind(this));
      } else {
        nameNode.textContent = entry.name;
        var message = isFile ? 'FILE_ALREADY_EXISTS' :
                               'DIRECTORY_ALREADY_EXISTS';
        this.alert.show(strf(message, newName));
      }
    }.bind(this));
  };

  FileManager.prototype.cancelRename_ = function() {
    this.renameInput_.currentEntry = null;

    var parent = this.renameInput_.parentNode;
    if (parent) {
      parent.removeAttribute('renaming');
      parent.removeChild(this.renameInput_);
    }
    this.refocus();
  };

  FileManager.prototype.onFilenameInputKeyUp_ = function(event) {
    var enabled = this.updateOkButton_();
    if (enabled &&
        (util.getKeyModifiers(event) + event.keyCode) == '13' /* Enter */)
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

  FileManager.prototype.cancelSpinnerTimeout_ = function() {
    if (this.showSpinnerTimeout_) {
      clearTimeout(this.showSpinnerTimeout_);
      this.showSpinnerTimeout_ = null;
    }
  };

  FileManager.prototype.showSpinnerLater_ = function() {
    this.cancelSpinnerTimeout_();
    this.showSpinnerTimeout_ =
        setTimeout(this.showSpinner_.bind(this, true), 500);
  };

  FileManager.prototype.showSpinner_ = function(on) {
    this.cancelSpinnerTimeout_();
    this.spinner_.style.display = on ? '' : 'none';
  };

  FileManager.prototype.onNewFolderCommand_ = function(event) {
    var defaultName = str('DEFAULT_NEW_FOLDER_NAME');

    // Find a name that doesn't exist in the data model.
    var files = this.directoryModel_.getFileList();
    var hash = {};
    for (var i = 0; i < files.length; i++) {
      var name = files.item(i).name;
      // Filtering names prevents from conflicts with prototype's names
      // and '__proto__'.
      if (name.substring(0, defaultName.length) == defaultName)
        hash[name] = 1;
    }

    var baseName = defaultName;
    var separator = '';
    var suffix = '';
    var index = '';

    function advance() {
      separator = ' (';
      suffix = ')';
      index++;
    }

    function current() {
      return baseName + separator + index + suffix;
    }

    // Accessing hasOwnProperty is safe since hash properties filtered.
    while (hash.hasOwnProperty(current())) {
      advance();
    }

    var self = this;
    var list = self.currentList_;
    function tryCreate() {
      self.directoryModel_.createDirectory(current(),
                                           onSuccess, onError);
    }

    function onSuccess(entry) {
      metrics.recordUserAction('CreateNewFolder');
      list.selectedItem = entry;
      self.initiateRename_();
    }

    function onError(error) {
      self.alert.show(strf('ERROR_CREATING_FOLDER', current(),
                           getFileErrorString(error.code)));
    }

    tryCreate();
  };

  FileManager.prototype.onDetailViewButtonClick_ = function(event) {
    this.setListType(FileManager.ListType.DETAIL);
    this.currentList_.focus();
  };

  FileManager.prototype.onThumbnailViewButtonClick_ = function(event) {
    this.setListType(FileManager.ListType.THUMBNAIL);
    this.currentList_.focus();
  };

  /**
   * KeyDown event handler for the document.
   */
  FileManager.prototype.onKeyDown_ = function(event) {
    if (event.srcElement === this.renameInput_) {
      // Ignore keydown handler in the rename input box.
      return;
    }

    switch (util.getKeyModifiers(event) + event.keyCode) {
      case 'Ctrl-190':  // Ctrl-. => Toggle filter files.
        var dm = this.directoryModel_;
        dm.setFilterHidden(!dm.isFilterHiddenOn());
        event.preventDefault();
        return;

      case '27':  // Escape => Cancel dialog.
        if (this.copyManager_ &&
            this.copyManager_.getStatus().totalFiles != 0) {
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
          this.cancelButton_.click();
        }
        break;
    }
  };

  /**
   * KeyDown event handler for the div#list-container element.
   */
  FileManager.prototype.onListKeyDown_ = function(event) {
    if (event.srcElement.tagName == 'INPUT') {
      // Ignore keydown handler in the rename input box.
      return;
    }

    var self = this;
    function handleCommand(name) {
      var command = self.commands_[name];
      command.disabled = !self.canExecute_(name);
      if (command.disabled)
        return;
      event.preventDefault();
      event.stopPropagation();
      command.execute();
    }

    switch (util.getKeyModifiers(event) + event.keyCode) {
      case 'Ctrl-32':  // Ctrl-Space => New Folder.
        handleCommand('newfolder');
        break;

      case 'Ctrl-88':  // Ctrl-X => Cut.
        handleCommand('cut');
        break;

      case 'Ctrl-67':  // Ctrl-C => Copy.
        handleCommand('copy');
        break;

      case 'Ctrl-86':  // Ctrl-V => Paste.
        handleCommand('paste');
        break;

      case 'Ctrl-69':  // Ctrl-E => Rename.
        handleCommand('rename');
        break;

      case 'Ctrl-13':  // Ctrl+Enter
        if (!this.isRenamingInProgress()) {
          event.preventDefault();
          this.initiateRename_();
        }
        break;

      case '8':  // Backspace => Up one directory.
        event.preventDefault();
        var path = this.getCurrentDirectory();
        if (path && !DirectoryModel.isRootPath(path)) {
          var path = path.replace(/\/[^\/]+$/, '');
          this.directoryModel_.changeDirectory(path);
        }
        break;

      case '13':  // Enter => Change directory or perform default action.
        if (this.selection.totalCount == 1 &&
            this.selection.entries[0].isDirectory &&
            this.dialogType_ != FileManager.SELECT_FOLDER) {
          event.preventDefault();
          this.onDirectoryAction(this.selection.entries[0]);
        } else if (this.dispatchSelectionAction_()) {
          event.preventDefault();
        }
        break;

      case '46':  // Delete.
        handleCommand('delete');
        break;
    }

    switch (event.keyIdentifier) {
      case 'Home':
      case 'End':
      case 'Up':
      case 'Down':
      case 'Left':
      case 'Right':
        // When navigating with keyboard we hide the distracting mouse hover
        // highlighting until the user moves the mouse again.
        this.listContainer_.classList.add('nohover');
        break;
    }
  };

  /**
   * KeyPress event handler for the div#list-container element.
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
   * Mousemove event handler for the div#list-container element.
   */
  FileManager.prototype.onListMouseMove_ = function(event) {
    // The user grabbed the mouse, restore the hover highlighting.
    this.listContainer_.classList.remove('nohover');
  };

  /**
   * Performs a 'text search' - selects a first list entry with name
   * starting with entered text (case-insensitive).
   */
  FileManager.prototype.doTextSearch_ = function() {
    var text = this.textSearchState_.text;
    if (!text)
      return;

    var dm = this.directoryModel_.getFileList();
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
   * Resolves selected file urls returned from an Open dialog.
   *
   * For gdata files this involves some special treatment.
   * Starts getting gdata files if needed.
   *
   * @param {Array.<string>} fileUrls GData URLs.
   * @param {function(Array.<string>)} callback To be called with fixed URLs.
   */
  FileManager.prototype.resolveSelectResults_ = function(fileUrls, callback) {
    if (this.isOnGData()) {
      chrome.fileBrowserPrivate.getGDataFiles(
        fileUrls,
        function(localPaths) {
          fileUrls = [].concat(fileUrls);  // Clone the array.
          // localPath can be empty if the file is not present, which
          // can happen if the user specifies a new file name to save a
          // file on gdata.
          for (var i = 0; i != localPaths.length; i++) {
            if (localPaths[i]) {
              // Add "localPath" parameter to the gdata file URL.
              fileUrls[i] += '?localPath=' + encodeURIComponent(localPaths[i]);
            }
          }
          callback(fileUrls);
        });
    } else {
      callback(fileUrls);
    }
  },

  /**
   * Closes this modal dialog with some files selected.
   * TODO(jamescook): Make unload handler work automatically, crbug.com/104811
   * @param {Object} selection Contains urls, filterIndex and multiple fields.
   */
  FileManager.prototype.callSelectFilesApiAndClose_ = function(selection) {
    if (selection.multiple) {
      chrome.fileBrowserPrivate.selectFiles(selection.urls);
    } else {
      chrome.fileBrowserPrivate.selectFile(
          selection.urls[0], selection.filterIndex);
    }
    this.onUnload_();
    window.close();
  };

  /**
   * Tries to close this modal dialog with some files selected.
   * Performs preprocessing if needed (e.g. for GData).
   * @param {Object} selection Contains urls, filterIndex and multiple fields.
   */
  FileManager.prototype.selectFilesAndClose_ = function(selection) {
    if (!this.isOnGData() ||
        this.dialogType_ == FileManager.DialogType.SELECT_SAVEAS_FILE) {
      setTimeout(this.callSelectFilesApiAndClose_.bind(this, selection), 0);
      return;
    }

    var shade = this.document_.createElement('div');
    shade.className = 'shade';
    var footer = this.document_.querySelector('.dialog-footer');
    var progress = footer.querySelector('.progress-track');
    progress.style.width = '0%';
    var cancelled = false;

    var progressMap = {};
    var filesStarted = 0;
    var filesTotal = selection.urls.length;
    for (var index = 0; index < selection.urls.length; index++) {
      progressMap[selection.urls[index]] = -1;
    }
    var lastPercent = 0;
    var bytesTotal = 0;
    var bytesDone = 0;

    var onFileTransfersUpdated = function(statusList) {
      for (var index = 0; index < statusList.length; index++) {
        var status = statusList[index];
        var escaped = encodeURI(status.fileUrl);
        if (!(escaped in progressMap)) continue;
        if (status.total == -1) continue;

        var old = progressMap[escaped];
        if (old == -1) {
          // -1 means we don't know file size yet.
          bytesTotal += status.total;
          filesStarted++;
          old = 0;
        }
        bytesDone += status.processed - old;
        progressMap[escaped] = status.processed;
      }

      var percent = bytesTotal == 0 ? 0 : bytesDone / bytesTotal;
      // For files we don't have information about, assume the progress is zero.
      percent = percent * filesStarted / filesTotal * 100;
      // Do not decrease the progress. This may happen, if first downloaded
      // file is small, and the second one is large.
      lastPercent = Math.max(lastPercent, percent);
      progress.style.width = lastPercent + '%';
    }.bind(this);

    var setup = function() {
      this.document_.querySelector('.dialog-container').appendChild(shade);
      setTimeout(function() { shade.setAttribute('fadein', 'fadein') }, 100);
      footer.setAttribute('progress', 'progress');
      this.cancelButton_.removeEventListener('click', this.onCancelBound_);
      this.cancelButton_.addEventListener('click', onCancel);
      chrome.fileBrowserPrivate.onFileTransfersUpdated.addListener(
          onFileTransfersUpdated);
    }.bind(this);

    var cleanup = function() {
      shade.parentNode.removeChild(shade);
      footer.removeAttribute('progress');
      this.cancelButton_.removeEventListener('click', onCancel);
      this.cancelButton_.addEventListener('click', this.onCancelBound_);
      chrome.fileBrowserPrivate.onFileTransfersUpdated.removeListener(
          onFileTransfersUpdated);
    }.bind(this);

    var onCancel = function() {
      cancelled = true;
      // According to API cancel may fail, but there is no proper UI to reflect
      // this. So, we just silently assume that everything is cancelled.
      chrome.fileBrowserPrivate.cancelFileTransfers(
          selection.urls, function(response) {});
      cleanup();
    }.bind(this);

    var onResolved = function(resolvedUrls) {
      if (cancelled) return;
      cleanup();
      selection.urls = resolvedUrls;
      // Call next method on a timeout, as it's unsafe to
      // close a window from a callback.
      setTimeout(this.callSelectFilesApiAndClose_.bind(this, selection), 0);
    }.bind(this);

    var onProperties = function(properties) {
      for (var i = 0; i < properties.length; i++) {
        if (!properties[i] || properties[i].present) {
          // For files already in GCache, we don't get any transfer updates.
          filesTotal--;
        }
      }
      this.resolveSelectResults_(selection.urls, onResolved);
    }.bind(this);

    setup();
    this.metadataCache_.get(selection.urls, 'gdata', onProperties);
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
    var self = this;
    if (this.dialogType_ == FileManager.DialogType.SELECT_SAVEAS_FILE) {
      var currentDirUrl = this.getCurrentDirectoryURL();

      if (currentDirUrl.charAt(currentDirUrl.length - 1) != '/')
        currentDirUrl += '/';

      // Save-as doesn't require a valid selection from the list, since
      // we're going to take the filename from the text input.
      var filename = this.filenameInput_.value;
      if (!filename)
        throw new Error('Missing filename!');
      if (!this.validateFileName_(filename))
        return;

      var singleSelection = {
        urls: [currentDirUrl + encodeURIComponent(filename)],
        multiple: false,
        filterIndex: self.getSelectedFilterIndex_(filename)
      };

      function resolveCallback(victim) {
        if (victim instanceof FileError) {
          // File does not exist.
          self.selectFilesAndClose_(singleSelection);
          return;
        }

        if (victim.isDirectory) {
          // Do not allow to overwrite directory.
          self.alert.show(strf('DIRECTORY_ALREADY_EXISTS', filename));
        } else {
          self.confirm.show(strf('CONFIRM_OVERWRITE_FILE', filename),
                            function() {
                              // User selected Ok from the confirm dialog.
                              self.selectFilesAndClose_(singleSelection);
                            });
        }
      }

      this.resolvePath(this.getCurrentDirectory() + '/' + filename,
          resolveCallback, resolveCallback);
      return;
    }

    var files = [];
    var selectedIndexes = this.currentList_.selectionModel.selectedIndexes;

    if (this.dialogType_ == FileManager.DialogType.SELECT_FOLDER &&
        selectedIndexes.length == 0) {
      var url = this.getCurrentDirectoryURL();
      var singleSelection = {
        urls: [url],
        multiple: false,
        filterIndex: this.getSelectedFilterIndex_(url)
      };
      this.selectFilesAndClose_(singleSelection);
      return;
    }

    // All other dialog types require at least one selected list item.
    // The logic to control whether or not the ok button is enabled should
    // prevent us from ever getting here, but we sanity check to be sure.
    if (!selectedIndexes.length)
      throw new Error('Nothing selected!');

    var dm = this.directoryModel_.getFileList();
    for (var i = 0; i < selectedIndexes.length; i++) {
      var entry = dm.item(selectedIndexes[i]);
      if (!entry) {
        console.log('Error locating selected file at index: ' + i);
        continue;
      }

      files.push(entry.toURL());
    }

    // Multi-file selection has no other restrictions.
    if (this.dialogType_ == FileManager.DialogType.SELECT_OPEN_MULTI_FILE) {
      var multipleSelection = {
        urls: files,
        multiple: true
      };
      this.selectFilesAndClose_(multipleSelection);
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

    var singleSelection = {
      urls: [files[0]],
      multiple: false,
      filterIndex: this.getSelectedFilterIndex_(files[0])
    };
    this.selectFilesAndClose_(singleSelection);
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
   * @param {function} opt_onDone Function to invoke when user closes the
   *    warning box or immediatelly if file name is correct.
   * @return {boolean} True if name is vaild.
   */
  FileManager.prototype.validateFileName_ = function(name, opt_onDone) {
    var onDone = opt_onDone || function() {};
    var msg;
    var testResult = /[\/\\\<\>\:\?\*\"\|]/.exec(name);
    if (testResult) {
      msg = strf('ERROR_INVALID_CHARACTER', testResult[0]);
    } else if (/^\s*$/i.test(name)) {
      msg = str('ERROR_WHITESPACE_NAME');
    } else if (/^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$/i.test(name)) {
      msg = str('ERROR_RESERVED_NAME');
    } else if (this.directoryModel_.isFilterHiddenOn() && name[0] == '.') {
      msg = str('ERROR_HIDDEN_NAME');
    }

    if (msg) {
      this.alert.show(msg, onDone);
      return false;
    }

    onDone();
    return true;
  };

  /**
   * Start or stop monitoring free space depending on the new value of current
   * directory path. In case the space is low shows a warning box.
   * @param {string} currentPath New path to the current directory.
   */
  FileManager.prototype.checkFreeSpace_ = function(currentPath) {
    var dir = DirectoryModel.DOWNLOADS_DIRECTORY;

    var scheduleCheck = function(timeout) {
      if (this.checkFreeSpaceTimer_) {
        clearTimeout(this.checkFreeSpaceTimer_);
        this.checkFreeSpaceTimer_ = null;
      }

      if (timeout) {
        this.checkFreeSpaceTimer_ = setTimeout(doCheck, timeout);
      }
    }.bind(this);

    var doCheck = function() {
      // Remember our invocation timer, because getSizeStats is long and
      // asynchronous call.
      var selfTimer = this.checkFreeSpaceTimer_;

      chrome.fileBrowserPrivate.getSizeStats(
          util.makeFilesystemUrl('/' + dir),
          function(sizeStats) {
            // If new check started while we were in async getSizeStats call,
            // then we shouldn't do anything.
            if (selfTimer != this.checkFreeSpaceTimer_) return;

            // sizeStats is undefined, if some error occurs.
            var lowDiskSpace =
                sizeStats &&
                sizeStats.totalSizeKB > 0 &&
                sizeStats.remainingSizeKB / sizeStats.totalSizeKB < 0.2;

            this.showLowDiskSpaceWarning_(lowDiskSpace);

            // If disk space is low, check it more often. User can delete files
            // manually and we should not bother her with warning in this case.
            scheduleCheck(lowDiskSpace ? 1000 * 5 : 1000 * 30);
          }.bind(this));
    }.bind(this);

    if (currentPath.substr(1, dir.length) == dir) {
      // Setup short timeout if currentPath just changed.
      scheduleCheck(500);
    } else {
      scheduleCheck(0);

      this.showLowDiskSpaceWarning_(false);
    }
  };

  /**
   * Handler invoked on preference setting in gdata context menu.
   * @param {String} pref  The preference to alter.
   * @param {boolean} inverted Invert the value if true.
   * @param {Event}  event The click event.
   */
  FileManager.prototype.onGDataPrefClick_ = function(pref, inverted, event) {
    var newValue = !event.target.hasAttribute('checked');
    if (newValue)
      event.target.setAttribute('checked', 'checked');
    else
      event.target.removeAttribute('checked');

    var changeInfo = {};
    changeInfo[pref] = inverted ? !newValue : newValue;
    chrome.fileBrowserPrivate.setGDataPreferences(changeInfo);
  };

  FileManager.prototype.onSearchBoxUpdate_ = function(event) {
    var searchString = this.document_.getElementById('search-box').value;
    var noResultsDiv = this.document_.getElementById('no-search-results');

    function reportEmptySearchResults() {
      if (this.directoryModel_.getFileList().length === 0) {
        var text = strf('SEARCH_NO_MATCHING_FILES', searchString);
        noResultsDiv.innerHTML = text;
        noResultsDiv.setAttribute('show', 'true');
      } else {
        noResultsDiv.removeAttribute('show');
      }
    }

    function hideNoResultsDiv() {
      noResultsDiv.removeAttribute('show');
    }

    this.directoryModel_.search(searchString,
                                reportEmptySearchResults.bind(this),
                                hideNoResultsDiv.bind(this));
  };

  FileManager.prototype.decorateSplitter = function(splitterElement) {
    var self = this;

    var Splitter = cr.ui.Splitter;

    var customSplitter = cr.ui.define('div');

    customSplitter.prototype = {
      __proto__: Splitter.prototype,

      handleSplitterDragStart: function(e) {
        Splitter.prototype.handleSplitterDragStart.apply(this, arguments);
        this.ownerDocument.documentElement.classList.add('col-resize');
      },

      handleSplitterDragMove: function(deltaX) {
        Splitter.prototype.handleSplitterDragMove.apply(this, arguments);
        self.onResize_();
      },

      handleSplitterDragEnd: function(e) {
        Splitter.prototype.handleSplitterDragEnd.apply(this, arguments);
        this.ownerDocument.documentElement.classList.remove('col-resize');
      }
    };

    customSplitter.decorate(splitterElement);
  };

  FileManager.prototype.setupGDataWelcome_ = function() {
    if (this.gdataWelcomeHandler_)
      return;
    this.gdataWelcomeHandler_ = this.createGDataWelcomeHandler_();
    if (this.gdataWelcomeHandler_) {
      this.directoryModel_.addEventListener('scan-completed',
          this.gdataWelcomeHandler_);
      this.directoryModel_.addEventListener('rescan-completed',
          this.gdataWelcomeHandler_);
    }
  };

  FileManager.prototype.cleanupGDataWelcome_ = function() {
    this.showGDataWelcome_('none');

    if (this.gdataWelcomeHandler_) {
      this.directoryModel_.removeEventListener('scan-completed',
          this.gdataWelcomeHandler_);
      this.directoryModel_.removeEventListener('rescan-completed',
          this.gdataWelcomeHandler_);
      this.gdataWelcomeHandler_ = null;
    }
  };

  FileManager.prototype.showGDataWelcome_ = function(type) {
    if (this.dialogContainer_.getAttribute('gdrive-welcome') != type) {
      this.dialogContainer_.setAttribute('gdrive-welcome', type);
      this.requestResize_(200);  // Resize only after the animation is done.
    }
  };

  FileManager.prototype.createGDataWelcomeHandler_ = function() {
    var WELCOME_HEADER_COUNTER_KEY = 'gdataWelcomeHeaderCounter';
    var WELCOME_HEADER_COUNTER_LIMIT = 5;

    function getHeaderCounter() {
      return parseInt(localStorage[WELCOME_HEADER_COUNTER_KEY] || '0');
    }

    if (getHeaderCounter() >= WELCOME_HEADER_COUNTER_LIMIT)
      return null;

    function createDiv(className, parent) {
      var div = parent.ownerDocument.createElement('div');
      div.className = className;
      parent.appendChild(div);
      return div;
    }

    var self = this;

    function showBanner(type, messageId) {
      if (!self.dialogContainer_.hasAttribute('gdrive-welcome-style')) {
        self.dialogContainer_.setAttribute('gdrive-welcome-style', true);
        var style = self.document_.createElement('link');
        style.rel = 'stylesheet';
        style.href = 'css/gdrive_welcome.css';
        self.document_.head.appendChild(style);
        style.onload = function() { showBanner(type, messageId) };
        return;
      }

      self.showGDataWelcome_(type);

      var container = self.dialogDom_.querySelector('.gdrive-welcome.' + type);
      if (container.firstElementChild)
        return;  // Do not re-create.

      var wrapper = createDiv('gdrive-welcome-wrapper', container);
      createDiv('gdrive-welcome-icon', wrapper);

      var close = createDiv('cr-dialog-close', wrapper);
      close.addEventListener('click', closeBanner);

      var message = createDiv('gdrive-welcome-message', wrapper);

      var title = createDiv('gdrive-welcome-title', message);
      title.textContent = str('GDATA_WELCOME_TITLE');

      var text = createDiv('gdrive-welcome-text', message);
      text.innerHTML = str(messageId);

      var links = createDiv('gdrive-welcome-links', message);

      var more = createDiv('gdrive-welcome-more plain-link', links);
      more.textContent = str('GDATA_LEARN_MORE');
      more.addEventListener('click',
          self.onExternalLinkClick_.bind(self, GOOGLE_DRIVE_FAQ_URL));

      var dismiss = createDiv('gdrive-welcome-dismiss plain-link', links);
      dismiss.textContent = str('GDATA_WELCOME_DISMISS');
      dismiss.addEventListener('click', closeBanner);
    }

    var previousDirWasOnGData = false;

    function maybeShowBanner() {
      if (!self.isOnGData()) {
        self.showGDataWelcome_('none');
        previousDirWasOnGData = false;
        return;
      }

      var counter = getHeaderCounter();

      if (self.directoryModel_.getFileList().length == 0 && counter == 0) {
        // Only show the full page banner if the header banner was never shown.
        // Do not increment the counter.
        // The timeout below is required because sometimes another
        // 'rescan-completed' event arrives shortly with non-empty file list.
        setTimeout(function() {
          if (self.isOnGData() &&
              self.dialogContainer_.getAttribute('gdrive-welcome') != 'header')
            showBanner('page', 'GDATA_WELCOME_TEXT_LONG');
        }, 2000);
      } else if (counter < WELCOME_HEADER_COUNTER_LIMIT) {
        // We do not want to increment the counter when the user navigates
        // between different directories on GDrive, but we increment the counter
        // once anyway to prevent the full page banner from showing.
        if (!previousDirWasOnGData || counter == 0)
          localStorage[WELCOME_HEADER_COUNTER_KEY] = ++counter;
        showBanner('header', 'GDATA_WELCOME_TEXT_SHORT');
      } else {
        closeBanner();
      }
      previousDirWasOnGData = true;
    }

    function closeBanner() {
      self.cleanupGDataWelcome_();
      // Stop showing the welcome banner.
      localStorage[WELCOME_HEADER_COUNTER_KEY] = WELCOME_HEADER_COUNTER_LIMIT;
    }

    return maybeShowBanner;
  };
})();


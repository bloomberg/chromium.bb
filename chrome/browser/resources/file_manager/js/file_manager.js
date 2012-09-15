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
  this.listType_ = null;
  this.showDelayTimeout_ = null;

  this.selection = null;

  this.filesystemObserverId_ = null;
  this.gdataObserverId_ = null;

  this.document_ = dialogDom.ownerDocument;
  this.dialogType_ = this.params_.type || FileManager.DialogType.FULL_PAGE;

  // Optional list of file types.
  this.fileTypes_ = this.params_.typeList || [];
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
   * Location of the page to buy more storage for Google Drive.
   */
  FileManager.GOOGLE_DRIVE_BUY_STORAGE =
      'https://www.google.com/settings/storage';

  /**
   * Location of Google Drive specific help.
   */
  FileManager.GOOGLE_DRIVE_HELP =
      'https://support.google.com/chromeos/?p=filemanager_drivehelp';

 /**
   * Location of Google Drive specific help.
   */
  FileManager.GOOGLE_DRIVE_ROOT = 'https://drive.google.com';

  /**
   * Maximum amount of thumbnails in the preview pane.
   */
  var MAX_PREVIEW_THUMBNAIL_COUNT = 4;

  /**
   * Maximum width or height of an image what pops up when the mouse hovers
   * thumbnail in the bottom panel (in pixels).
   */
  var IMAGE_HOVER_PREVIEW_SIZE = 200;

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

    var dm = fileManager.directoryModel_;
    this.internalChangeHandler_ = dm.rescan.bind(dm);

    this.filesystemObserverId_ = null;
    this.thumbnailObserverId_ = null;
    this.gdataObserverId_ = null;
    this.internalObserverId_ = null;

    // Holds the directories known to contain files with stale metadata
    // as URL to bool map.
    this.directoriesWithStaleMetadata_ = {};
  };

  FileManager.MetadataFileWatcher.prototype.__proto__ = FileWatcher.prototype;

  /**
   * Changed metadata observers for the new directory.
   * @override
   * @param {?DirectoryEntry} entry New watched directory entry.
   * @override
   */
  FileManager.MetadataFileWatcher.prototype.changeWatchedEntry = function(
      entry) {
    FileWatcher.prototype.changeWatchedEntry.call(this, entry);

    if (this.filesystemObserverId_)
      this.metadataCache_.removeObserver(this.filesystemObserverId_);
    if (this.thumbnailObserverId_)
      this.metadataCache_.removeObserver(this.thumbnailObserverId_);
    if (this.gdataObserverId_)
      this.metadataCache_.removeObserver(this.gdataObserverId_);
    this.filesystemObserverId_ = null;
    this.gdataObserverId_ = null;
    this.internalObserverId_ = null;
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

    if (PathUtil.getRootType(entry.fullPath) === RootType.GDATA) {
      this.gdataObserverId_ = this.metadataCache_.addObserver(
          entry,
          MetadataCache.CHILDREN,
          'gdata',
          this.gdataChangeHandler_);
    }

    this.internalObserverId_ = this.metadataCache_.addObserver(
        entry,
        MetadataCache.CHILDREN,
        'internal',
        this.internalChangeHandler_);
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

    this.showCheckboxes_ =
        (this.dialogType_ == FileManager.DialogType.FULL_PAGE ||
         this.dialogType_ == FileManager.DialogType.SELECT_OPEN_MULTI_FILE);

    this.table_.startBatchUpdates();
    this.grid_.startBatchUpdates();

    this.initFileList_();
    this.initDialogs_();
    this.bannersController_ = new FileListBannerController(
        this.directoryModel_, this.volumeManager_, this.document_);
    this.bannersController_.addEventListener('relayout',
                                             this.onResize_.bind(this));

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
    dm.addEventListener('scan-cancelled', this.hideSpinnerLater_.bind(this));
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

    this.initContextMenus_();
    this.initCommands_();

    this.updateFileTypeFilter_();

    // Show the page now unless it's already delayed.
    this.delayShow_(0);

    metrics.recordInterval('Load.DOM');
    metrics.recordInterval('Load.Total');
  };

  FileManager.prototype.initDataTransferOperations_ = function() {
    this.copyManager_ = new FileCopyManagerWrapper.getInstance(
        this.filesystem_.root);
    this.copyManager_.addEventListener('copy-progress',
                                       this.onCopyProgress_.bind(this));
    this.copyManager_.addEventListener('copy-operation-complete',
        this.onCopyManagerOperationComplete_.bind(this));

    this.butterBar_ = new ButterBar(this.dialogDom_, this.copyManager_,
        this.metadataCache_);

    var controller = this.fileTransferController_ = new FileTransferController(
        GridItem.bind(null, this, false /* no checkbox */),
        this.copyManager_,
        this.directoryModel_);
    controller.attachDragSource(this.table_.list);
    controller.attachDropTarget(this.table_.list);
    controller.attachDragSource(this.grid_);
    controller.attachDropTarget(this.grid_);
    controller.attachDropTarget(this.rootsList_, true);
    controller.attachBreadcrumbsDropTarget(this.breadcrumbs_);
    controller.attachCopyPasteHandlers(this.document_);
    controller.addEventListener('selection-copied',
        this.blinkSelection.bind(this));
    controller.addEventListener('selection-cut',
        this.blinkSelection.bind(this));
  };

  /**
     * One-time initialization of context menus.
     */
  FileManager.prototype.initContextMenus_ = function() {
    this.fileContextMenu_ = this.dialogDom_.querySelector('#file-context-menu');
    cr.ui.Menu.decorate(this.fileContextMenu_);

    cr.ui.contextMenuHandler.setContextMenu(this.grid_, this.fileContextMenu_);
    cr.ui.contextMenuHandler.setContextMenu(this.table_.querySelector('.list'),
        this.fileContextMenu_);

    this.rootsContextMenu_ =
        this.dialogDom_.querySelector('#roots-context-menu');
    cr.ui.Menu.decorate(this.rootsContextMenu_);

    this.textContextMenu_ =
        this.dialogDom_.querySelector('#text-context-menu');
    cr.ui.Menu.decorate(this.textContextMenu_);

    this.gdataSettingsMenu_ = this.dialogDom_.querySelector('#gdata-settings');
    cr.ui.decorate(this.gdataSettingsMenu_, cr.ui.MenuButton);

    this.gdataSettingsMenu_.addEventListener('menushow',
        this.onGDataMenuShow_.bind(this));

    this.gdataSpaceInfo_ = this.dialogDom_.querySelector('#gdata-space-info');
    this.gdataSpaceInfoLabel_ =
        this.dialogDom_.querySelector('#gdata-space-info-label');
    this.gdataSpaceInfoBar_ =
        this.dialogDom_.querySelector('#gdata-space-info-bar');
  };

  /**
   * One-time initialization of commands.
   */
  FileManager.prototype.initCommands_ = function() {
    var commandButtons = this.dialogDom_.querySelectorAll('button[command]');
    for (var j = 0; j < commandButtons.length; j++)
      CommandButton.decorate(commandButtons[j]);

    var commands = this.dialogDom_.querySelectorAll('command');
    for (var i = 0; i < commands.length; i++)
      cr.ui.Command.decorate(commands[i]);

    var doc = this.document_;

    if (this.dialogType_ == FileManager.DialogType.SELECT_SAVEAS_FILE ||
        this.dialogType_ == FileManager.DialogType.FULL_PAGE)
      CommandUtil.registerCommand(doc, 'newfolder',
          Commands.newFolderCommand, this, this.directoryModel_);

    CommandUtil.registerCommand(this.rootsList_, 'unmount',
        Commands.unmountCommand, this.rootsList_, this);

    CommandUtil.registerCommand(this.rootsList_, 'format',
        Commands.formatCommand, this.rootsList_, this);

    CommandUtil.registerCommand(this.rootsList_, 'import-photos',
        Commands.importCommand, this.rootsList_);

    CommandUtil.registerCommand(doc, 'delete',
        Commands.deleteFileCommand, this);

    CommandUtil.registerCommand(doc, 'rename',
        Commands.renameFileCommand, this);

    CommandUtil.registerCommand(doc, 'gdata-buy-more-space',
        Commands.gdataBuySpaceCommand, this);

    CommandUtil.registerCommand(doc, 'gdata-help',
        Commands.gdataHelpCommand, this);

    CommandUtil.registerCommand(doc, 'gdata-clear-local-cache',
        Commands.gdataClearCacheCommand, this);

    CommandUtil.registerCommand(doc, 'gdata-go-to-drive',
        Commands.gdataGoToDriveCommand, this);

    CommandUtil.registerCommand(doc, 'paste',
        Commands.pasteFileCommand, doc, this.fileTransferController_);

    CommandUtil.registerCommand(doc, 'cut', Commands.defaultCommand, doc);
    CommandUtil.registerCommand(doc, 'copy', Commands.defaultCommand, doc);

    var inputs = this.dialogDom_.querySelectorAll(
        'input[type=text], input[type=search], textarea');

    for (i = 0; i < inputs.length; i++) {
      cr.ui.contextMenuHandler.setContextMenu(inputs[i], this.textContextMenu_);
      this.registerInputCommands_(inputs[i]);
    }

    cr.ui.contextMenuHandler.setContextMenu(this.renameInput_,
        this.textContextMenu_);
    this.registerInputCommands_(this.renameInput_);
  };

  /**
   * Registers cut, copy, paste and delete commands on input element.
   * @param {Node} node Text input element to register on.
   */
  FileManager.prototype.registerInputCommands_ = function(node) {
    var defaultCommand = Commands.defaultCommand;
    CommandUtil.registerCommand(node, 'cut', defaultCommand, this.document_);
    CommandUtil.registerCommand(node, 'copy', defaultCommand, this.document_);
    CommandUtil.registerCommand(node, 'paste', defaultCommand, this.document_);
    CommandUtil.registerCommand(node, 'delete', defaultCommand, this.document_);
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
    this.dialogDom_.addEventListener('drop', function(e) {
      // Prevent opening an URL by dropping it onto the page.
      e.preventDefault();
    });

    this.dialogDom_.addEventListener('click',
                                     this.onExternalLinkClick_.bind(this));
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
    this.spinner_ = this.dialogDom_.querySelector('#spinner-with-text');
    this.showSpinner_(false);

    this.breadcrumbs_ = new BreadcrumbsController(
         this.dialogDom_.querySelector('#dir-breadcrumbs'));
    this.breadcrumbs_.addEventListener(
         'pathclick', this.onBreadcrumbClick_.bind(this));
    this.searchBreadcrumbs_ = new BreadcrumbsController(
         this.dialogDom_.querySelector('#search-breadcrumbs'));
    this.searchBreadcrumbs_.addEventListener(
         'pathclick', this.onBreadcrumbClick_.bind(this));
    this.searchBreadcrumbs_.setHideLast(true);

    cr.ui.Table.decorate(this.table_);
    cr.ui.Grid.decorate(this.grid_);

    this.document_.addEventListener('keydown', this.onKeyDown_.bind(this));
    this.document_.addEventListener('keyup', this.onKeyUp_.bind(this));
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
        'keydown', this.onFilenameInputKeyDown_.bind(this));
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

    this.decorateSplitter(
        this.dialogDom_.querySelector('div.sidebar-splitter'));

    this.dialogContainer_ = this.dialogDom_.querySelector('.dialog-container');
    this.dialogDom_.querySelector('#detail-view').addEventListener(
        'click', this.onDetailViewButtonClick_.bind(this));
    this.dialogDom_.querySelector('#thumbnail-view').addEventListener(
        'click', this.onThumbnailViewButtonClick_.bind(this));

    this.syncButton = this.dialogDom_.querySelector('#gdata-sync-settings');
    this.syncButton.addEventListener('activate', this.onGDataPrefClick_.bind(
        this, 'cellularDisabled', false /* not inverted */));

    this.hostedButton = this.dialogDom_.querySelector('#gdata-hosted-settings');
    this.hostedButton.addEventListener('activate', this.onGDataPrefClick_.bind(
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

    this.defaultActionMenuItem_ =
        this.dialogDom_.querySelector('#default-action');

    this.openWithCommand_ =
            this.dialogDom_.querySelector('#open-with');

    this.defaultActionMenuItem_.addEventListener('activate',
        this.dispatchSelectionAction_.bind(this));

    this.fileTypeSelector_ = this.dialogDom_.querySelector('#file-type');
    this.initFileTypeFilter_();
    // Populate the static localized strings.
    i18nTemplate.process(this.document_, loadTimeData);
  };

  FileManager.prototype.onBreadcrumbClick_ = function(event) {
    this.directoryModel_.changeDirectory(event.path);
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
      return self.renderRoot_(entry.fullPath);
    };

    this.rootsList_.selectionModel =
        this.directoryModel_.getRootsListSelectionModel();

    // TODO(dgozman): add "Add a drive" item.
    this.rootsList_.dataModel = this.directoryModel_.getRootsList();
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

  /**
   * Index of selected item in the typeList of the dialog params.
   * @return {number} 1-based index of selected type or 0 if no type selected.
   */
  FileManager.prototype.getSelectedFilterIndex_ = function() {
    var index = Number(this.fileTypeSelector_.selectedIndex);
    if (index < 0)  // Nothing selected.
      return 0;
    if (this.params_.includeAllFiles)  // Already 1-based.
      return index;
    return index + 1;  // Convert to 1-based;
  };

  FileManager.prototype.getRootEntry_ = function(index) {
    if (index == -1)
      return null;

    return this.rootsList_.dataModel.item(index);
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
        'click', this.onDetailClick_.bind(this));
    this.grid_.addEventListener(
        cr.ui.TouchHandler.EventType.TAP,
        this.onDetailDoubleClickOrTap_.bind(this));
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
        'click', this.onDetailClick_.bind(this));
    this.table_.list.addEventListener(
        cr.ui.TouchHandler.EventType.TAP,
        this.onDetailDoubleClickOrTap_.bind(this));
  };

  FileManager.prototype.onCopyProgress_ = function(event) {
    if (event.reason === 'ERROR' &&
        event.error.reason === 'FILESYSTEM_ERROR' &&
        event.error.data.toGDrive &&
        event.error.data.code == FileError.QUOTA_EXCEEDED_ERR) {
      this.alert.showHtml(
          strf('GDATA_SERVER_OUT_OF_SPACE_HEADER'),
          strf('GDATA_SERVER_OUT_OF_SPACE_MESSAGE',
              decodeURIComponent(
                  event.error.data.sourceFileUrl.split('/').pop()),
              FileManager.GOOGLE_DRIVE_BUY_STORAGE));
    }

    // TODO(benchan): Currently, there is no FileWatcher emulation for
    // DriveFileSystem, so we need to manually trigger the directory rescan
    // after paste operations complete. Remove this once we emulate file
    // watching functionalities in DriveFileSystem.
    if (this.isOnGData()) {
      if (event.reason == 'SUCCESS' || event.reason == 'ERROR' ||
          event.reason == 'CANCELLED') {
        this.directoryModel_.rescanLater();
      }
    }
  };

  /**
   * Handler of file manager operations. Update directory model
   * to reflect operation result immediatelly (not waiting directory
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
   * Fills the file type list or hides it.
   */
  FileManager.prototype.initFileTypeFilter_ = function() {
    if (this.params_.includeAllFiles) {
      var option = this.document_.createElement('option');
      option.innerText = str('ALL_FILES_FILTER');
      this.fileTypeSelector_.appendChild(option);
      option.value = 0;
    }

    for (var i = 0; i < this.fileTypes_.length; i++) {
      var fileType = this.fileTypes_[i];
      var option = this.document_.createElement('option');
      var description = fileType.description;
      if (!description) {
        // See if all the extensions in the group have the same description.
        for (var j = 0; j != fileType.extensions.length; j++) {
          var currentDescription =
              this.getFileTypeString_('.' + fileType.extensions[j]);
          if (!description)  // Set the first time.
            description = currentDescription;
          else if (description != currentDescription) {
            // No single description, fall through to the extension list.
            description = null;
            break;
          }
        }

        if (!description)
          // Convert ['jpg', 'png'] to '*.jpg, *.png'.
          description = fileType.extensions.map(function(s) {
           return '*.' + s;
          }).join(', ');
       }
       option.innerText = description;

       option.value = i + 1;

       if (fileType.selected)
         option.selected = true;

       this.fileTypeSelector_.appendChild(option);
    }

    var options = this.fileTypeSelector_.querySelectorAll('option');
    if (options.length < 2) {
      // There is in fact no choice, hide the selector.
      this.fileTypeSelector_.hidden = true;
      return;
    }

    this.fileTypeSelector_.addEventListener('change',
        this.updateFileTypeFilter_.bind(this));
  };

  /**
   * Filters file according to the selected file type.
   */
  FileManager.prototype.updateFileTypeFilter_ = function() {
    this.directoryModel_.removeFilter('fileType');
    var selectedIndex = this.getSelectedFilterIndex_();
    if (selectedIndex > 0) { // Specific filter selected.
      var regexp = new RegExp('.*(' +
          this.fileTypes_[selectedIndex - 1].extensions.join('|') + ')$', 'i');
      function filter(entry) {
        return entry.isDirectory || regexp.test(entry.name);
      }
      this.directoryModel_.addFilter('fileType', filter);
    }
    this.directoryModel_.rescan();
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
    this.breadcrumbs_.truncate();
    this.searchBreadcrumbs_.truncate();
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
      path = this.directoryModel_.getDefaultDirectory();
    } else if (path.indexOf('/') == -1) {
      // Path is a file name.
      path = this.directoryModel_.getDefaultDirectory() + '/' + path;
    }

    // In the FULL_PAGE mode if the hash path points to a file we might have
    // to invoke a task after selecting it.
    // If the file path is in params_ we only want to select the file.
    var invokeHandlers = pageLoading && !this.params_.selectOnly &&
        this.dialogType_ == FileManager.DialogType.FULL_PAGE &&
        !!location.hash;

    if (PathUtil.getRootType(path) === RootType.GDATA) {
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
      var gdataPath = RootDirectory.GDATA;
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
          // Non-existent file or a directory.
          foundLeaf = false;
          if (self.params_.gallery) {
            // Reloading while the Gallery is open with empty or multiple
            // selection. Open the Gallery when the directory is scanned.
            var listener = function() {
              self.directoryModel_.removeEventListener(
                  'scan-completed', listener);
              new FileTasks(self, [], null /* mime types */, self.params_).
                  openGallery([]);
            };
            self.directoryModel_.addEventListener('scan-completed', listener);
          } else {
            self.show_();  // Remove the shade immediately.
          }
        }
      }

      // TODO(dgozman): get rid of onLoadedActivate callback in setupPath.
      var self = this;
      function onLoadedActivateLeaf() {
        if (foundLeaf) {
          // TODO(kaznacheev): use |makeFIlesystemUrl| instead of
          // self.selection.
          var tasks = new FileTasks(self, [self.selection.urls[0]],
              null /* mime types */, self.params_);
          // There are 3 ways we can get here:
          // 1. Invoked from file_manager_util::ViewFile. This can only
          //    happen for 'gallery' and 'mount-archive' actions.
          // 2. Reloading a Gallery page. Must be an image or a video file.
          // 3. A user manually entered a URL pointing to a file.
          if (FileType.isImageOrVideo(path)) {
            tasks.execute(util.getExtensionId() + '|gallery');
          } else if (FileType.getMediaType(path) == 'archive') {
            self.show_();
            tasks.execute(util.getExtensionId() + '|mount-archive');
          } else {
            self.show_();
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

  FileManager.prototype.decorateThumbnail_ = function(li, showCheckbox, entry) {
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
    icon.setAttribute('file-type-icon', FileType.getIcon(entry));
    return icon;
  };

  FileManager.prototype.renderRoot_ = function(path) {
    var li = this.document_.createElement('li');
    li.className = 'root-item';
    var dm = this.directoryModel_;
    var handleClick = function() {
      if (li.selected && path !== dm.getCurrentDirPath()) {
        dm.changeDirectory(path);
      }
    };
    li.addEventListener('mousedown', handleClick);
    li.addEventListener(cr.ui.TouchHandler.EventType.TOUCH_START, handleClick);

    var rootType = PathUtil.getRootType(path);

    var div = this.document_.createElement('div');
    div.className = 'root-label';

    div.setAttribute('volume-type-icon', rootType);
    if (rootType === RootType.REMOVABLE)
      div.setAttribute('volume-subtype',
          this.volumeManager_.getDeviceType(path));

    div.textContent = PathUtil.getRootLabel(path);
    li.appendChild(div);

    if (rootType === RootType.ARCHIVE || rootType === RootType.REMOVABLE) {
      var eject = this.document_.createElement('div');
      eject.className = 'root-eject';
      eject.addEventListener('click', function(event) {
        event.stopPropagation();
        this.unmountVolume(path);
      }.bind(this));
      // Block other mouse handlers.
      eject.addEventListener('mouseup', function(e) { e.stopPropagation() });
      eject.addEventListener('mousedown', function(e) { e.stopPropagation() });
      li.appendChild(eject);
    }

    if (rootType != RootType.GDATA) {
      cr.ui.contextMenuHandler.setContextMenu(li, this.rootsContextMenu_);
    }

    cr.defineProperty(li, 'lead', cr.PropertyKind.BOOL_ATTR);
    cr.defineProperty(li, 'selected', cr.PropertyKind.BOOL_ATTR);

    return li;
  };

  /**
   * Unmounts device.
   * @param {string} path Path to a volume to unmount.
   */
  FileManager.prototype.unmountVolume = function(path) {
    var listItem = this.rootsList_.getListItemByIndex(
        this.directoryModel_.findRootsListIndex(path));
    if (listItem)
      listItem.setAttribute('disabled', '');
    var onError = function(error) {
      if (listItem)
        listItem.removeAttribute('disabled');
      this.alert.show(strf('UNMOUNT_FAILED', error.message));
    };
    this.volumeManager_.unmount(path, function() {}, onError.bind(this));
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
      var iconDiv = listItem.querySelector('.detail-icon');
      if (!iconDiv)
        return;
      // Find the default app for this file.  If there is none, then
      // leave it as the base icon for the file type.
      var url;
      for (var i = 0; i < gdata.driveApps.length; ++i) {
        var app = gdata.driveApps[i];
        if (app && app.docIcon && app.isPrimary) {
          url = app.docIcon;
          break;
        }
      }
      if (url)
        iconDiv.style.backgroundImage = 'url(' + url + ')';
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
    var box = this.document_.createElement('div');
    box.className = 'filename-label';
    var fileName = this.document_.createElement('span');
    fileName.textContent = entry.name;
    box.appendChild(fileName);

    return box;
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
   * There is not a big difference whether DOM items stay the same or not.
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
   *
   * TODO(olege): I believe we need a separate PreviewPanel controller class.
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

    function thumbnailClickHandler() {
      if (selection.tasks)
        selection.tasks.executeDefault();
    }

    for (var i = 0; i < selection.indexes.length; i++) {
      var entry = this.directoryModel_.getFileList().item(selection.indexes[i]);
      if (!entry)
        continue;

      selection.entries.push(entry);
      selection.urls.push(entry.toURL());

      if (thumbnailCount < MAX_PREVIEW_THUMBNAIL_COUNT) {
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
        box.style.zIndex = MAX_PREVIEW_THUMBNAIL_COUNT + 1 - i;
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
    this.updatePreviewPanelVisibility_();
    this.updateSearchBreadcrumbs_();
    forcedShowTimeout = setTimeout(showThumbnails,
        FileManager.THUMBNAIL_SHOW_DELAY);
    onThumbnailLoaded();

    this.updateContextMenuActionItems(null, false);

    function getTasksAndFinish() {
      if (self.dialogType_ == FileManager.DialogType.FULL_PAGE &&
          selection.directoryCount == 0 && selection.fileCount > 0) {
        selection.tasks = new FileTasks(self, selection.urls,
            selection.mimeTypes).display(self.taskItems_).updateMenuItem();
      } else {
        self.taskItems_.hidden = true;
      }

      self.metadataCache_.get(selection.entries, 'filesystem', function(props) {
        for (var index = 0; index < selection.entries.length; index++) {
          var filesystem = props[index];

          if (entry.isFile) {
            selection.bytes += filesystem.size;
          }
        }

        self.dispatchEvent(new cr.Event('selection-summarized'));
      });
    }

    if (this.isOnGData()) {
      function predicate(p) {
        return !(p && p.availableOffline);
      }
      this.metadataCache_.get(selection.urls, 'gdata', function(props) {
        selection.allGDataFilesPresent =
            props.filter(predicate).length == 0;
        this.updateOkButton_();

        // Collect all of the mime types and push that info into the selection.
        selection.mimeTypes = props.map(function(value) {
          return (value && value.contentMimeType) || '';
        });

        getTasksAndFinish();
      }.bind(this));
    } else {
      getTasksAndFinish();
    }
  };

  FileManager.prototype.updateSearchBreadcrumbs_ = function() {
    var selectedIndexes = this.currentList_.selectionModel.selectedIndexes;
    if (selectedIndexes.length !== 1 || !this.directoryModel_.isSearching()) {
      this.searchBreadcrumbs_.hide();
      return;
    }

    var entry = this.directoryModel_.getFileList().item(selectedIndexes[0]);
    this.searchBreadcrumbs_.show(
        PathUtil.getRootPath(entry.fullPath),
        entry.fullPath);
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
    return this.directoryModel_.getCurrentRootType() === RootType.GDATA;
  };

  /**
   * Overrides default handling for clicks on hyperlinks.
   * Opens them in a separate tab and if it's an open/save dialog
   * closes it.
   * @param {Event} event Click event.
   */
  FileManager.prototype.onExternalLinkClick_ = function(event) {
    if (event.target.tagName != 'A' || !event.target.href)
      return;
    chrome.tabs.create({url: event.target.href});
    if (this.dialogType_ != FileManager.DialogType.FULL_PAGE) {
      this.onCancel_();
    }
    event.preventDefault();
  };

  /**
   * Task combobox handler.
   *
   * @param {Object} event Event containing task which was clicked.
   */
  FileManager.prototype.onTaskItemClicked_ = function(event) {
    if (!this.selection.tasks) return;

    if (event.item.task) {
      // Task field doesn't exist on change-default dropdown item.
      this.selection.tasks.execute(event.item.task.taskId);
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

      if (extensions.length == 1) {
        format = extensions[0];
      }

      // Change default was clicked. We should open "change default" dialog.
      this.selection.tasks.showTaskPicker(this.defaultTaskPicker,
          loadTimeData.getString('CHANGE_DEFAULT_MENU_ITEM'),
          strf('CHANGE_DEFAULT_CAPTION', format),
          this.onDefaultTaskDone_.bind(this));
    }
  };


  /**
   * Sets the given task as default, when this task is applicable.
   * @param {Object} task Task to set as default.
   */
  FileManager.prototype.onDefaultTaskDone_ = function(task) {
    chrome.fileBrowserPrivate.setDefaultTask(task.taskId,
      this.selection.urls, this.selection.mimeTypes);
    this.selection.tasks = new FileTasks(
        this, this.selection.urls, this.selection.mimeTypes).
            display(this.taskItems_);
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
    return !this.params_.disableGData &&
        (!('driveEnabled' in this.gdataPreferences_) ||
         this.gdataPreferences_.driveEnabled);
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

  FileManager.prototype.getAllUrlsInCurrentDirectory = function() {
    var urls = [];
    var fileList = this.directoryModel_.getFileList();
    for (var i = 0; i != fileList.length; i++) {
      urls.push(fileList.item(i).toURL());
    }
    return urls;
  };

  FileManager.prototype.isRenamingInProgress = function() {
    return !!this.renameInput_.currentEntry;
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

  FileManager.prototype.deleteSelection = function() {
    this.butterBar_.initiateDelete(this.selection.entries);
  };

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

  /**
   * Handles mouse click or tap. Simulates double click if click happens
   * on the file name or the icon.
   * @param {Event} event The click event.
   */
  FileManager.prototype.onDetailClick_ = function(event) {
    if (this.isRenamingInProgress()) {
      // Don't pay attention to clicks during a rename.
      return;
    }

    if (event.target.parentElement.classList.contains('filename-label') ||
        event.target.classList.contains('detail-icon') ||
        event.target.classList.contains('img-container')) {
      this.onDetailDoubleClickOrTap_(event);
      event.stopPropagation();
      event.preventDefault();
    }
  };

  FileManager.prototype.dispatchSelectionAction_ = function() {
    if (this.dialogType_ == FileManager.DialogType.FULL_PAGE) {
      if (this.selection.tasks)
        this.selection.tasks.executeDefault();
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
        PathUtil.getRootPath(entry.fullPath));
    if (mountError == VolumeManager.Error.UNKNOWN_FILESYSTEM) {
      return this.butterBar_.show(str('UNKNOWN_FILESYSTEM_WARNING'));
    } else if (mountError == VolumeManager.Error.UNSUPPORTED_FILESYSTEM) {
      return this.butterBar_.show(str('UNSUPPORTED_FILESYSTEM_WARNING'));
    }

    return this.directoryModel_.changeDirectory(entry.fullPath);
  };

  /**
   * Update the tab title.
   */
  FileManager.prototype.updateTitle_ = function() {
    if (this.dialogType_ != FileManager.DialogType.FULL_PAGE)
      return;

    this.document_.title = this.getCurrentDirectory().replace(
        new RegExp('^' + RootDirectory.GDATA),
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
    this.updateOkButton_();
    this.breadcrumbs_.update(
        this.directoryModel_.getCurrentRootPath(),
        this.directoryModel_.getCurrentDirPath());
    this.updateColumnModel_();
    this.updateSearchBoxOnDirChange_();

    util.updateLocation(event.initial, this.getCurrentDirectory());

    if (this.closeOnUnmount_ && !event.initial &&
          PathUtil.getRootPath(event.previousDirEntry.fullPath) !=
          PathUtil.getRootPath(event.newDirEntry.fullPath)) {
      this.closeOnUnmount_ = false;
    }

    this.updateTitle_();
  };

  FileManager.prototype.findListItemForEvent_ = function(event) {
    return this.findListItemForNode_(event.touchedElement || event.srcElement);
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

  FileManager.prototype.initiateRename = function() {
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
                      util.getFileErrorString(err.code)));
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

  FileManager.prototype.onFilenameInputKeyDown_ = function(event) {
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

  FileManager.prototype.hideSpinnerLater_ = function() {
    setTimeout(this.showSpinner_.bind(this, false), 100);
  };

  FileManager.prototype.showSpinner_ = function(on) {
    if (on && this.directoryModel_ && this.directoryModel_.isScanning()) {
      if (this.directoryModel_.isSearching()) {
        this.dialogContainer_.classList.add('searching');
        this.spinner_.style.display = 'none';
      } else {
        this.spinner_.style.display = '';
        this.dialogContainer_.classList.remove('searching');
      }
    }

    if (!on && (!this.directoryModel_ || !this.directoryModel_.isScanning())) {
      this.spinner_.style.display = 'none';
      if (this.dialogContainer_)
        this.dialogContainer_.classList.remove('searching');
    }
  };

  FileManager.prototype.createNewFolder = function() {
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
      self.initiateRename();
    }

    function onError(error) {
      self.alert.show(strf('ERROR_CREATING_FOLDER', current(),
                           util.getFileErrorString(error.code)));
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
      case 'Ctrl-17':  // Ctrl => Show hidden setting
        this.dialogDom_.setAttribute('ctrl-pressing', 'true');
        return;

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

        if (this.butterBar_ && this.butterBar_.hideError()) {
          event.preventDefault();
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
   * KeyUp event handler for the document.
   */
  FileManager.prototype.onKeyUp_ = function(event) {
    if (event.srcElement === this.renameInput_) {
      // Ignore keydown handler in the rename input box.
      return;
    }

    switch (util.getKeyModifiers(event) + event.keyCode) {
      case '17':  // Ctrl => Hide hidden setting
        this.dialogDom_.removeAttribute('ctrl-pressing');
        return;
    }
  }

  /**
   * KeyDown event handler for the div#list-container element.
   */
  FileManager.prototype.onListKeyDown_ = function(event) {
    if (event.srcElement.tagName == 'INPUT') {
      // Ignore keydown handler in the rename input box.
      return;
    }

    switch (util.getKeyModifiers(event) + event.keyCode) {
      case '8':  // Backspace => Up one directory.
        event.preventDefault();
        var path = this.getCurrentDirectory();
        if (path && !PathUtil.isRootPath(path)) {
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
        filterIndex: this.getSelectedFilterIndex_()
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
      filterIndex: this.getSelectedFilterIndex_()
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

  /**
   * Listener invoked on gdata menu show event, to update gdata free/total
   * space info in opened menu.
   * @private
   */
  FileManager.prototype.onGDataMenuShow_ = function() {
    this.gdataSpaceInfoBar_.setAttribute('pending', '');
    chrome.fileBrowserPrivate.getSizeStats(
        this.directoryModel_.getCurrentRootUrl(), function(result) {
          if (!chrome.extension.lastError) {
            this.gdataSpaceInfoBar_.removeAttribute('pending');

            var sizeInGb = util.bytesToSi(result.remainingSizeKB * 1024);
            this.gdataSpaceInfoLabel_.textContent =
                strf('GDATA_SPACE_AVAILABLE', sizeInGb);

            var usedSpace = result.totalSizeKB - result.remainingSizeKB;

            this.gdataSpaceInfoBar_.style.display = '';
            this.gdataSpaceInfoBar_.style.width =
                (100 * usedSpace / result.totalSizeKB) + '%';
          } else {
            this.gdataSpaceInfoBar_.style.display = 'none';
            this.gdataSpaceInfoLabel_.textContent =
                str('GDATA_FAILED_SPACE_INFO');
          }
        }.bind(this));
  };

  /**
   * Updates default action menu item to match passed taskItem(icon,
   * label and action).
   *
   * @param {Object} defaultItem - taskItem to match.
   * @param {boolean} isMultiple - if multiple tasks available.
   */
  FileManager.prototype.updateContextMenuActionItems = function(defaultItem,
                                                               isMultiple) {
    if (defaultItem) {
      if (defaultItem.iconType) {
        this.defaultActionMenuItem_.style.backgroundImage = '';
        this.defaultActionMenuItem_.setAttribute('file-type-icon',
                                                 defaultItem.iconType);
      } else if (defaultItem.iconUrl) {
        this.defaultActionMenuItem_.style.backgroundImage =
            'url(' + defaultItem.iconUrl + ')';
      } else {
        this.defaultActionMenuItem_.style.backgroundImage = '';
      }

      this.defaultActionMenuItem_.label = defaultItem.title;
      this.defaultActionMenuItem_.taskId = defaultItem.taskId;
    }

    var defaultActionSeparator =
        this.dialogDom_.querySelector('#default-action-separator');

    // TODO(dzvorygin): Here we use this hack, since 'hidden' is standard
    // attribute and we can't use it's setter as usual.
    this.openWithCommand_.__lookupSetter__('hidden').
        call(this.openWithCommand_, !(defaultItem && isMultiple));
    this.defaultActionMenuItem_.hidden = !defaultItem;
    defaultActionSeparator.hidden = !defaultItem;
  };
})();

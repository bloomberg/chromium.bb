// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This variable is checked in SelectFileDialogExtensionBrowserTest.
var JSErrorCount = 0;

/**
 * Count uncaught exceptions.
 */
window.onerror = function() { JSErrorCount++ };

/**
 * FileManager constructor.
 *
 * FileManager objects encapsulate the functionality of the file selector
 * dialogs, as well as the full screen file manager application (though the
 * latter is not yet implemented).
 *
 * @param {HTMLElement} dialogDom The DOM node containing the prototypical
 *     dialog UI.
 * @constructor
 */
function FileManager(dialogDom) {
  this.dialogDom_ = dialogDom;
  this.filesystem_ = null;

  if (window.appState) {
    this.params_ = window.appState.params || {};
    this.defaultPath = window.appState.defaultPath;
    util.saveAppState();
  } else {
    this.params_ = location.search ?
                   JSON.parse(decodeURIComponent(location.search.substr(1))) :
                   {};
    this.defaultPath = this.params_.defaultPath;
  }
  this.listType_ = null;
  this.showDelayTimeout_ = null;

  this.filesystemObserverId_ = null;
  this.driveObserverId_ = null;

  this.document_ = dialogDom.ownerDocument;
  this.dialogType = this.params_.type || DialogType.FULL_PAGE;
  this.startupPrefName_ = 'file-manager-' + this.dialogType;

  // Used to filter out focusing by mouse.
  this.suppressFocus_ = false;

  // Optional list of file types.
  this.fileTypes_ = this.params_.typeList || [];
  metrics.recordEnum('Create', this.dialogType,
      [DialogType.SELECT_FOLDER,
       DialogType.SELECT_SAVEAS_FILE,
       DialogType.SELECT_OPEN_FILE,
       DialogType.SELECT_OPEN_MULTI_FILE,
       DialogType.FULL_PAGE]);

  this.selectionHandler_ = null;

  this.metadataCache_ = MetadataCache.createFull();
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

/**
 * Unload the file manager.
 * Used by background.js (when running in the packaged mode).
 */
function unload() {
  fileManager.onBeforeUnload_();
  fileManager.onUnload_();
}

/**
 * List of dialog types.
 *
 * Keep this in sync with FileManagerDialog::GetDialogTypeAsString, except
 * FULL_PAGE which is specific to this code.
 *
 * @enum {string}
 */
var DialogType = {
  SELECT_FOLDER: 'folder',
  SELECT_SAVEAS_FILE: 'saveas-file',
  SELECT_OPEN_FILE: 'open-file',
  SELECT_OPEN_MULTI_FILE: 'open-multi-file',
  FULL_PAGE: 'full-page'
};

/**
 * List of connection types of drive.
 *
 * Keep this in sync with the kDriveConnectionType* constants in
 * file_browser_private_api.cc.
 *
 * @enum {string}
 */
var DriveConnectionType = {
  OFFLINE: 'offline',  // Connection is offline or drive is unavailable.
  METERED: 'metered',  // Connection is metered. Should limit traffic.
  ONLINE: 'online'     // Connection is online.
};

/**
 * List of reasons of DriveConnectionType.
 *
 * Keep this in sync with the kDriveConnectionReason constants in
 * file_browser_private_api.cc.
 *
 * @enum {string}
 */
var DriveConnectionReason = {
  NOT_READY: 'not_ready',    // Drive is not ready or authentication is failed.
  NO_NETWORK: 'no_network',  // Network connection is unavailable.
  NO_SERVICE: 'no_service'   // Drive service is unavailable.
};

/**
 * @param {string} type Dialog type.
 * @return {boolean} Whether the type is modal.
 */
DialogType.isModal = function(type) {
  return type == DialogType.SELECT_FOLDER ||
      type == DialogType.SELECT_SAVEAS_FILE ||
      type == DialogType.SELECT_OPEN_FILE ||
      type == DialogType.SELECT_OPEN_MULTI_FILE;
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
   * Location of Files App specific help.
   */
  FileManager.FILES_APP_HELP =
      'https://support.google.com/chromeos/?p=gsg_files_app';

  /**
   * Number of milliseconds in a day.
   */
  var MILLISECONDS_IN_DAY = 24 * 60 * 60 * 1000;

  /**
   * Some UI elements react on a single click and standard double click handling
   * leads to confusing results. We ignore a second click if it comes soon
   * after the first.
   */
  var DOUBLE_CLICK_TIMEOUT = 200;

  var removeChildren = function(element) {
    element.textContent = '';
  };

  /**
   * Update the elemenst to display the information about remainig space for
   * the storage.
   * @param {!Element} spaceInnerBar Block element for a percentage bar
   *                                 representing the remaining space.
   * @param {!Element} spaceInfoLabel Inline element to contain the message.
   * @param {!Element} spaceOuterBar Block element around the percentage bar.
   */
   var updateSpaceInfo = function(
      sizeStatsResult, spaceInnerBar, spaceInfoLabel, spaceOuterBar) {
    spaceInnerBar.removeAttribute('pending');
    if (sizeStatsResult) {
      var sizeStr = util.bytesToString(sizeStatsResult.remainingSizeKB * 1024);
      spaceInfoLabel.textContent = strf('SPACE_AVAILABLE', sizeStr);

      var usedSpace =
          sizeStatsResult.totalSizeKB - sizeStatsResult.remainingSizeKB;
      spaceInnerBar.style.width =
          (100 * usedSpace / sizeStatsResult.totalSizeKB) + '%';

      spaceOuterBar.style.display = '';
    } else {
      spaceOuterBar.style.display = 'none';
      spaceInfoLabel.textContent = str('FAILED_SPACE_INFO');
    }
  };

  // Public statics.

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
    this.driveChangeHandler_ =
        fileManager.updateMetadataInUI_.bind(fileManager, 'drive');

    var dm = fileManager.directoryModel_;
    this.internalChangeHandler_ = dm.rescan.bind(dm);

    this.filesystemObserverId_ = null;
    this.thumbnailObserverId_ = null;
    this.driveObserverId_ = null;
    this.internalObserverId_ = null;
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
    if (this.driveObserverId_)
      this.metadataCache_.removeObserver(this.driveObserverId_);
    this.filesystemObserverId_ = null;
    this.driveObserverId_ = null;
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

    if (PathUtil.getRootType(entry.fullPath) === RootType.DRIVE) {
      this.driveObserverId_ = this.metadataCache_.addObserver(
          entry,
          MetadataCache.CHILDREN,
          'drive',
          this.driveChangeHandler_);
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
    this.metadataCache_.resumeRefresh(this.getWatchedDirectoryEntry().toURL());
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
      this.showDelayTimeout_ = null;
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

    metrics.startInterval('Load.FileSystem');

    var self = this;
    var downcount = 3;
    var viewOptions = {};
    var done = function() {
      if (--downcount == 0)
        self.init_(viewOptions);
    };

    chrome.fileBrowserPrivate.requestLocalFileSystem(function(filesystem) {
      metrics.recordInterval('Load.FileSystem');
      self.filesystem_ = filesystem;
      done();
    });

    // DRIVE preferences should be initialized before creating DirectoryModel
    // to tot rebuild the roots list.
    this.updateNetworkStateAndPreferences_(done);

    util.platform.getPreference(this.startupPrefName_, function(value) {
      // Load the global default options.
      try {
        viewOptions = JSON.parse(value);
      } catch (ignore) {}
      // Override with window-specific options.
      if (window.appState && window.appState.viewOptions) {
        for (var key in window.appState.viewOptions) {
          if (window.appState.viewOptions.hasOwnProperty(key))
            viewOptions[key] = window.appState.viewOptions[key];
        }
      }
      done();
    }.bind(this));
  };

  /**
   * @param {Object} prefs Preferences.
   * Continue initializing the file manager after resolving roots.
   */
  FileManager.prototype.init_ = function(prefs) {
    metrics.startInterval('Load.DOM');

    // PyAuto tests monitor this state by polling this variable
    this.__defineGetter__('workerInitialized_', function() {
       return this.metadataCache_.isInitialized();
    }.bind(this));

    this.initDateTimeFormatters_();

    this.table_.startBatchUpdates();
    this.grid_.startBatchUpdates();

    this.initFileList_(prefs);
    this.initDialogs_();

    var self = this;

    // Get the 'allowRedeemOffers' preference before launching
    // FileListBannerController.
    this.getPreferences_(function(pref) {
      /* @type {boolean} */
      var showOffers = pref['allowRedeemOffers'];
      self.bannersController_ = new FileListBannerController(
          self.directoryModel_, self.volumeManager_, self.document_,
          showOffers);
      self.bannersController_.addEventListener('relayout',
                                               self.onResize_.bind(self));
    });

    if (!util.platform.v2()) {
      window.addEventListener('popstate', this.onPopState_.bind(this));
      window.addEventListener('unload', this.onUnload_.bind(this));
      window.addEventListener('beforeunload', this.onBeforeUnload_.bind(this));
    }

    var dm = this.directoryModel_;
    dm.addEventListener('directory-changed',
                        this.onDirectoryChanged_.bind(this));
    dm.addEventListener('begin-update-files', function() {
      self.currentList_.startBatchUpdates();
    });
    dm.addEventListener('end-update-files', function() {
      self.restoreItemBeingRenamed_();
      self.currentList_.endBatchUpdates();
    });
    dm.addEventListener('scan-started', this.onScanStarted_.bind(this));
    dm.addEventListener('scan-completed', this.showSpinner_.bind(this, false));
    dm.addEventListener('scan-cancelled', this.hideSpinnerLater_.bind(this));
    dm.addEventListener('scan-completed',
                        this.refreshCurrentDirectoryMetadata_.bind(this));
    dm.addEventListener('rescan-completed',
                        this.refreshCurrentDirectoryMetadata_.bind(this));

    this.directoryModel_.sortFileList(
        prefs.sortField || 'modificationTime',
        prefs.sortDirection || 'desc');

    var stateChangeHandler =
        this.onNetworkStateOrPreferencesChanged_.bind(this);
    chrome.fileBrowserPrivate.onPreferencesChanged.addListener(
        stateChangeHandler);
    chrome.fileBrowserPrivate.onDriveConnectionStatusChanged.addListener(
        stateChangeHandler);
    stateChangeHandler();

    this.refocus();

    this.initDataTransferOperations_();

    this.initContextMenus_();
    this.initCommands_();

    this.updateFileTypeFilter_();

    this.selectionHandler_.onFileSelectionChanged();

    this.setupCurrentDirectory_(true /* page loading */);

    this.table_.endBatchUpdates();
    this.grid_.endBatchUpdates();

    // Show the page now unless it's already delayed.
    this.delayShow_(0);

    metrics.recordInterval('Load.DOM');
    metrics.recordInterval('Load.Total');
  };

  FileManager.prototype.initDateTimeFormatters_ = function() {
    var use12hourClock = !this.preferences_['use24hourClock'];
    this.table_.setDateTimeFormat(use12hourClock);
  };

  FileManager.prototype.initDataTransferOperations_ = function() {
    this.copyManager_ = new FileCopyManagerWrapper.getInstance(
        this.filesystem_.root);

    this.butterBar_ = new ButterBar(this.dialogDom_, this.copyManager_,
        this.metadataCache_);

    // CopyManager and ButterBar are required for 'Delete' operation in
    // Open and Save dialogs. But drag-n-drop and copy-paste are not needed.
    if (this.dialogType != DialogType.FULL_PAGE) return;

    this.copyManager_.addEventListener('copy-progress',
                                       this.onCopyProgress_.bind(this));
    this.copyManager_.addEventListener('copy-operation-complete',
        this.onCopyManagerOperationComplete_.bind(this));

    var controller = this.fileTransferController_ =
        new FileTransferController(this.document_,
                                   this.copyManager_,
                                   this.directoryModel_);
    controller.attachDragSource(this.table_.list);
    controller.attachDropTarget(this.table_.list);
    controller.attachDragSource(this.grid_);
    controller.attachDropTarget(this.grid_);
    controller.attachDropTarget(this.rootsList_, true);
    controller.attachBreadcrumbsDropTarget(this.breadcrumbs_);
    controller.attachCopyPasteHandlers();
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
    cr.ui.contextMenuHandler.setContextMenu(
        this.document_.querySelector('.drive-welcome.page'),
        this.fileContextMenu_);

    this.rootsContextMenu_ =
        this.dialogDom_.querySelector('#roots-context-menu');
    cr.ui.Menu.decorate(this.rootsContextMenu_);

    this.textContextMenu_ =
        this.dialogDom_.querySelector('#text-context-menu');
    cr.ui.Menu.decorate(this.textContextMenu_);

    this.gearButton_ = this.dialogDom_.querySelector('#gear-button');
    this.gearButton_.addEventListener('menushow',
        this.refreshRemainingSpace_.bind(this,
                                         false /* Without loading caption. */));
    cr.ui.decorate(this.gearButton_, cr.ui.MenuButton);

    this.syncButton.checkable = true;
    this.hostedButton.checkable = true;
  };

  /**
   * One-time initialization of commands.
   */
  FileManager.prototype.initCommands_ = function() {
    var commandButtons = this.dialogDom_.querySelectorAll('button[command]');
    for (var j = 0; j < commandButtons.length; j++)
      CommandButton.decorate(commandButtons[j]);

    // TODO(dzvorygin): Here we use this hack, since 'hidden' is standard
    // attribute and we can't use it's setter as usual.
    cr.ui.Command.prototype.setHidden = function(value) {
      this.__lookupSetter__('hidden').call(this, value);
    };

    var commands = this.dialogDom_.querySelectorAll('command');
    for (var i = 0; i < commands.length; i++)
      cr.ui.Command.decorate(commands[i]);

    var doc = this.document_;

    CommandUtil.registerCommand(doc, 'newfolder',
        Commands.newFolderCommand, this, this.directoryModel_);

    CommandUtil.registerCommand(this.rootsList_, 'unmount',
        Commands.unmountCommand, this.rootsList_, this);

    CommandUtil.registerCommand(doc, 'format',
        Commands.formatCommand, this.rootsList_, this, this.directoryModel_);

    CommandUtil.registerCommand(this.rootsList_, 'import-photos',
        Commands.importCommand, this.rootsList_);

    CommandUtil.registerCommand(doc, 'delete',
        Commands.deleteFileCommand, this);

    CommandUtil.registerCommand(doc, 'rename',
        Commands.renameFileCommand, this);

    CommandUtil.registerCommand(doc, 'volume-help',
        Commands.volumeHelpCommand, this);

    CommandUtil.registerCommand(doc, 'drive-buy-more-space',
        Commands.driveBuySpaceCommand, this);

    CommandUtil.registerCommand(doc, 'drive-clear-local-cache',
        Commands.driveClearCacheCommand, this);

    CommandUtil.registerCommand(doc, 'drive-reload',
        Commands.driveReloadCommand, this);

    CommandUtil.registerCommand(doc, 'drive-go-to-drive',
        Commands.driveGoToDriveCommand, this);

    CommandUtil.registerCommand(doc, 'paste',
        Commands.pasteFileCommand, doc, this.fileTransferController_);

    CommandUtil.registerCommand(doc, 'open-with',
        Commands.openWithCommand, this);

    CommandUtil.registerCommand(doc, 'toggle-pinned',
        Commands.togglePinnedCommand, this);

    CommandUtil.registerCommand(doc, 'zip-selection',
        Commands.zipSelectionCommand, this, this.directoryModel_);

    CommandUtil.registerCommand(doc, 'search', Commands.searchCommand, this,
            this.dialogDom_.querySelector('#search-box'));

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

    doc.addEventListener('command', this.setNoHover_.bind(this, true));
  };

  /**
   * Registers cut, copy, paste and delete commands on input element.
   * @param {Node} node Text input element to register on.
   */
  FileManager.prototype.registerInputCommands_ = function(node) {
    var defaultCommand = Commands.defaultCommand;
    CommandUtil.forceDefaultHandler(node, 'cut');
    CommandUtil.forceDefaultHandler(node, 'copy');
    CommandUtil.forceDefaultHandler(node, 'paste');
    CommandUtil.forceDefaultHandler(node, 'delete');
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
    var dom = this.dialogDom_;

    this.filenameInput_ = dom.querySelector('#filename-input-box input');
    this.taskItems_ = dom.querySelector('#tasks');
    this.okButton_ = dom.querySelector('.ok');
    this.cancelButton_ = dom.querySelector('.cancel');

    this.table_ = dom.querySelector('.detail-table');
    this.grid_ = dom.querySelector('.thumbnail-grid');
    this.spinner_ = dom.querySelector('#spinner-with-text');
    this.showSpinner_(false);

    this.breadcrumbs_ = new BreadcrumbsController(
         dom.querySelector('#dir-breadcrumbs'));
    this.breadcrumbs_.addEventListener(
         'pathclick', this.onBreadcrumbClick_.bind(this));
    this.searchBreadcrumbs_ = new BreadcrumbsController(
         dom.querySelector('#search-breadcrumbs'));
    this.searchBreadcrumbs_.addEventListener(
         'pathclick', this.onBreadcrumbClick_.bind(this));
    this.searchBreadcrumbs_.setHideLast(true);

    var fullPage = this.dialogType == DialogType.FULL_PAGE;
    FileTable.decorate(this.table_, this.metadataCache_, fullPage);
    FileGrid.decorate(this.grid_, this.metadataCache_);

    this.document_.addEventListener('keydown', this.onKeyDown_.bind(this));
    this.document_.addEventListener('keyup', this.onKeyUp_.bind(this));

    // This capturing event is only used to distinguish focusing using
    // keyboard from focusing using mouse.
    this.document_.addEventListener('mousedown', function() {
      this.suppressFocus_ = true;
    }.bind(this), true);

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

    this.syncButton = this.dialogDom_.querySelector('#drive-sync-settings');
    this.syncButton.addEventListener('activate', this.onDrivePrefClick_.bind(
        this, 'cellularDisabled', false /* not inverted */));

    this.hostedButton = this.dialogDom_.querySelector('#drive-hosted-settings');
    this.hostedButton.addEventListener('activate', this.onDrivePrefClick_.bind(
        this, 'hostedFilesDisabled', true /* inverted */));

    cr.ui.ComboButton.decorate(this.taskItems_);
    this.taskItems_.addEventListener('select',
        this.onTaskItemClicked_.bind(this));

    this.dialogDom_.ownerDocument.defaultView.addEventListener(
        'resize', this.onResize_.bind(this));

    if (loadTimeData.getBoolean('ASH'))
      this.dialogDom_.setAttribute('ash', 'true');

    this.filePopup_ = null;

    var searchBox = this.dialogDom_.querySelector('#search-box');
    searchBox.addEventListener('input', this.onSearchBoxUpdate_.bind(this));

    var autocompleteList = new cr.ui.AutocompleteList();
    autocompleteList.id = 'autocomplete-list';
    autocompleteList.autoExpands = true;
    autocompleteList.requestSuggestions =
        this.requestAutocompleteSuggestions_.bind(this);
    // function(item) {}.bind(this) does not work here, as it's a constructor.
    var self = this;
    autocompleteList.itemConstructor = function(item) {
      return self.createAutocompleteListItem_(item);
    };

    // Do nothing when a suggestion is selected.
    autocompleteList.handleSelectedSuggestion = function(selectedItem) {};
    // Instead, open the suggested item when Enter key is pressed or
    // mouse-clicked.
    autocompleteList.handleEnterKeydown =
        this.openAutocompleteSuggestion_.bind(this);
    autocompleteList.addEventListener('mousedown', function(event) {
      this.openAutocompleteSuggestion_();
    }.bind(this));
    autocompleteList.addEventListener('mouseover', function(event) {
      // Change the selection by a mouse over instead of just changing the
      // color of moused over element with :hover in CSS. Here's why:
      //
      // 1) The user selects an item A with up/down keys (item A is highlighted)
      // 2) Then the user moves the cursor to another item B
      //
      // If we just change the color of moused over element (item B), both
      // the item A and B are highlighted. This is bad. We should change the
      // selection so only the item B is highlighted.
      if (event.target.itemInfo)
        autocompleteList.selectedItem = event.target.itemInfo;
    }.bind(this));

    var container = this.document_.querySelector('.dialog-header');
    container.appendChild(autocompleteList);
    this.autocompleteList_ = autocompleteList;

    searchBox.addEventListener('focus', function(event) {
      this.autocompleteList_.attachToInput(searchBox);
    }.bind(this));
    searchBox.addEventListener('blur', function(event) {
      this.autocompleteList_.detach();
    }.bind(this));

    this.authFailedWarning_ = dom.querySelector('#drive-auth-failed-warning');
    var authFailedText = this.authFailedWarning_.querySelector('.drive-text');
    authFailedText.innerHTML = util.htmlUnescape(str('DRIVE_NOT_REACHED'));
    authFailedText.querySelector('a').addEventListener('click', function(e) {
        chrome.fileBrowserPrivate.logoutUser();
        e.preventDefault();
    });

    this.defaultActionMenuItem_ =
        this.dialogDom_.querySelector('#default-action');

    this.openWithCommand_ =
        this.dialogDom_.querySelector('#open-with');

    this.driveBuyMoreStorageCommand_ =
        this.dialogDom_.querySelector('#drive-buy-more-space');

    this.defaultActionMenuItem_.addEventListener('activate',
        this.dispatchSelectionAction_.bind(this));

    this.fileTypeSelector_ = this.dialogDom_.querySelector('#file-type');
    this.initFileTypeFilter_();

    util.disableBrowserShortcutKeys(this.document_);

    this.updateWindowState_();
    // Populate the static localized strings.
    i18nTemplate.process(this.document_, loadTimeData);
  };

  FileManager.prototype.onBreadcrumbClick_ = function(event) {
    this.directoryModel_.changeDirectory(event.path);
  };

  /**
   * Constructs table and grid (heavy operation).
   * @param {Object} prefs Preferences.
   **/
  FileManager.prototype.initFileList_ = function(prefs) {
    // Always sharing the data model between the detail/thumb views confuses
    // them.  Instead we maintain this bogus data model, and hook it up to the
    // view that is not in use.
    this.emptyDataModel_ = new cr.ui.ArrayDataModel([]);
    this.emptySelectionModel_ = new cr.ui.ListSelectionModel();

    var singleSelection =
        this.dialogType == DialogType.SELECT_OPEN_FILE ||
        this.dialogType == DialogType.SELECT_FOLDER ||
        this.dialogType == DialogType.SELECT_SAVEAS_FILE;

    this.directoryModel_ = new DirectoryModel(
        this.filesystem_.root,
        singleSelection,
        this.metadataCache_,
        this.volumeManager_,
        this.isDriveEnabled());

    this.directoryModel_.start();

    this.selectionHandler_ = new FileSelectionHandler(this);

    this.fileWatcher_ = new FileManager.MetadataFileWatcher(this);
    this.fileWatcher_.start();

    var dataModel = this.directoryModel_.getFileList();

    this.table_.setupCompareFunctions(dataModel);

    dataModel.addEventListener('permuted',
                               this.updateStartupPrefs_.bind(this));

    this.directoryModel_.getFileListSelection().addEventListener('change',
        this.selectionHandler_.onFileSelectionChanged.bind(
            this.selectionHandler_));

    this.initList_(this.grid_);
    this.initList_(this.table_.list);

    var fileListFocusBound = this.onFileListFocus_.bind(this);
    var fileListBlurBound = this.onFileListBlur_.bind(this);

    this.table_.list.addEventListener('focus', fileListFocusBound);
    this.grid_.addEventListener('focus', fileListFocusBound);

    this.table_.list.addEventListener('blur', fileListBlurBound);
    this.grid_.addEventListener('blur', fileListBlurBound);

    this.initRootsList_();

    this.table_.addEventListener('column-resize-end',
                                 this.updateStartupPrefs_.bind(this));

    this.setListType(prefs.listType || FileManager.ListType.DETAIL);

    if (prefs.columns) {
      var cm = this.table_.columnModel;
      for (var i = 0; i < cm.totalSize; i++) {
        if (prefs.columns[i] > 0)
          cm.setWidth(i, prefs.columns[i]);
      }
    }

    this.textSearchState_ = {text: '', date: new Date()};

    this.closeOnUnmount_ = (this.params_.action == 'auto-open');

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

    // Overriding default role 'list' set by cr.ui.List.decorate() to 'listbox'
    // role for better accessibility on ChromeOS.
    this.rootsList_.setAttribute('role', 'listbox');

    var self = this;
    this.rootsList_.itemConstructor = function(entry) {
      return self.renderRoot_(entry.fullPath);
    };

    this.rootsList_.selectionModel =
        this.directoryModel_.getRootsListSelectionModel();

    // TODO(dgozman): add "Add a drive" item.
    this.rootsList_.dataModel = this.directoryModel_.getRootsList();
  };

  FileManager.prototype.updateStartupPrefs_ = function() {
    var sortStatus = this.directoryModel_.getFileList().sortStatus;
    var prefs = {
      sortField: sortStatus.field,
      sortDirection: sortStatus.direction,
      columns: []
    };
    var cm = this.table_.columnModel;
    for (var i = 0; i < cm.totalSize; i++) {
      prefs.columns.push(cm.getWidth(i));
    }
    if (DialogType.isModal(this.dialogType))
      prefs.listType = this.listType;
    // Save the global default.
    util.platform.setPreference(this.startupPrefName_, JSON.stringify(prefs));

    // Save the window-specific preference.
    if (window.appState) {
      window.appState.viewOptions = prefs;
      util.saveAppState();
    }
  };

  FileManager.prototype.refocus = function() {
    if (this.dialogType == DialogType.SELECT_SAVEAS_FILE)
      this.filenameInput_.focus();
    else
      this.currentList_.focus();
  };

  /*
   * File list focus handler. Used to select the top most element on the list
   * if nothing was selected.
   */
  FileManager.prototype.onFileListFocus_ = function() {
    // Do not select default item if focused using mouse.
    if (this.suppressFocus_)
      return;

    var selection = this.getSelection();
    if (!selection || selection.totalCount != 0)
      return;

    this.directoryModel_.selectIndex(0);
  };

  /*
   * File list blur handler.
   */
  FileManager.prototype.onFileListBlur_ = function() {
    this.suppressFocus_ = false;
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
    this.updateStartupPrefs_();
    this.onResize_();

    this.table_.list.endBatchUpdates();
    this.grid_.endBatchUpdates();
  };

  /**
   * Initialize the file list table or grid.
   * @param {cr.ui.List} list The list.
   */
  FileManager.prototype.initList_ = function(list) {
    // Overriding the default role 'list' to 'listbox' for better accessibility
    // on ChromeOS.
    list.setAttribute('role', 'listbox');
    list.addEventListener('click', this.onDetailClick_.bind(this));
  };

  FileManager.prototype.onCopyProgress_ = function(event) {
    if (event.reason === 'ERROR' &&
        event.error.reason === 'FILESYSTEM_ERROR' &&
        event.error.data.toDrive &&
        event.error.data.code == FileError.QUOTA_EXCEEDED_ERR) {
      this.alert.showHtml(
          strf('DRIVE_SERVER_OUT_OF_SPACE_HEADER'),
          strf('DRIVE_SERVER_OUT_OF_SPACE_MESSAGE',
              decodeURIComponent(
                  event.error.data.sourceFileUrl.split('/').pop()),
              FileManager.GOOGLE_DRIVE_BUY_STORAGE));
    }

    // TODO(benchan): Currently, there is no FileWatcher emulation for
    // DriveFileSystem, so we need to manually trigger the directory rescan
    // after paste operations complete. Remove this once we emulate file
    // watching functionalities in DriveFileSystem.
    if (this.isOnDrive()) {
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
    if (this.isOnDrive() && this.directoryModel_.isSearching())
      return;

    var inCurrentDirectory = function(entry) {
      var fullPath = entry.fullPath;
      var dirPath = fullPath.substr(0, fullPath.length -
                                       entry.name.length - 1);
      return dirPath == currentPath;
    };
    for (var i = 0; i < event.affectedEntries.length; i++) {
      var entry = event.affectedEntries[i];
      if (inCurrentDirectory(entry))
        this.directoryModel_.onEntryChanged(entry.name);
    }
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
              FileType.getTypeString('.' + fileType.extensions[j]);
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
      var filter = function(entry) {
        return entry.isDirectory || regexp.test(entry.name);
      };
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

    this.rootsList_.redraw();
    this.breadcrumbs_.truncate();
    this.searchBreadcrumbs_.truncate();

    this.updateWindowState_();
  };

  FileManager.prototype.updateWindowState_ = function() {
    util.platform.getWindowStatus(function(wnd) {
      if (wnd.state == 'maximized') {
        this.dialogDom_.setAttribute('maximized', 'maximized');
      } else {
        this.dialogDom_.removeAttribute('maximized');
      }
    }.bind(this));
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
        this.defaultPath;

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
    var invokeHandlers = pageLoading && (this.params_.action != 'select') &&
        this.dialogType == DialogType.FULL_PAGE;

    if (PathUtil.getRootType(path) === RootType.DRIVE) {
      var tracker = this.directoryModel_.createDirectoryChangeTracker();
      // Expected finish of setupPath to Drive.
      tracker.exceptInitialChange = true;
      tracker.start();
      if (!this.isDriveEnabled()) {
        if (pageLoading)
          this.show_();
        var leafName = path.substr(path.indexOf('/') + 1);
        path = this.directoryModel_.getDefaultDirectory() + '/' + leafName;
        this.finishSetupCurrentDirectory_(path, invokeHandlers);
        return;
      }
      var drivePath = RootDirectory.DRIVE;
      if (this.volumeManager_.isMounted(drivePath)) {
        this.finishSetupCurrentDirectory_(path, invokeHandlers);
        return;
      }
      if (pageLoading)
        this.delayShow_(500);
      // Reflect immediatelly in the UI we are on Drive and display
      // mounting UI.
      this.directoryModel_.setupPath(drivePath);

      if (!this.isOnDrive()) {
        // Since DRIVE is not mounted it should be resolved synchronously
        // (no need in asynchronous calls to filesystem API). It is important
        // to prevent race condition.
        console.error('Expected path set up synchronously');
      }

      var self = this;
      this.volumeManager_.mountDrive(function() {
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
      var onResolve = function(baseName, leafName, exists) {
        var urls = null;
        var action = null;

        if (!exists || leafName == '') {
          // Non-existent file or a directory.
          if (this.params_.gallery) {
            // Reloading while the Gallery is open with empty or multiple
            // selection. Open the Gallery when the directory is scanned.
            urls = [];
            action = 'gallery';
          }
        } else {
          // There are 3 ways we can get here:
          // 1. Invoked from file_manager_util::ViewFile. This can only
          //    happen for 'gallery' and 'mount-archive' actions.
          // 2. Reloading a Gallery page. Must be an image or a video file.
          // 3. A user manually entered a URL pointing to a file.
          // We call the appropriate methods of FileTasks directly as we do
          // not need any of the preparations that |execute| method does.
          if (FileType.isImageOrVideo(path)) {
            urls = [util.makeFilesystemUrl(path)];
            action = 'gallery';
          }
          if (FileType.getMediaType(path) == 'archive') {
            urls = [util.makeFilesystemUrl(path)];
            action = 'archives';
          }
        }

        if (urls) {
          var listener = function() {
            this.directoryModel_.removeEventListener(
                'scan-completed', listener);
            var tasks = new FileTasks(this, this.params_);
            if (action == 'gallery') {
              tasks.openGallery(urls);
            } else if (action == 'archives') {
              tasks.mountArchives_(urls);
            }
          }.bind(this);
          this.directoryModel_.addEventListener('scan-completed', listener);
        }

        if (action != 'gallery') {
          // Opening gallery will invoke |this.show_| at the right time.
          this.show_();
        }
      }.bind(this);

      this.directoryModel_.setupPath(path, onResolve);
      return;
    }

    if (this.dialogType == DialogType.SELECT_SAVEAS_FILE) {
      this.directoryModel_.setupPath(path, function(basePath, leafName) {
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

    switch (this.dialogType) {
      case DialogType.SELECT_FOLDER:
        defaultTitle = str('SELECT_FOLDER_TITLE');
        break;

      case DialogType.SELECT_OPEN_FILE:
        defaultTitle = str('SELECT_OPEN_FILE_TITLE');
        break;

      case DialogType.SELECT_OPEN_MULTI_FILE:
        defaultTitle = str('SELECT_OPEN_MULTI_FILE_TITLE');
        break;

      case DialogType.SELECT_SAVEAS_FILE:
        defaultTitle = str('SELECT_SAVEAS_FILE_TITLE');
        okLabel = str('SAVE_LABEL');
        break;

      case DialogType.FULL_PAGE:
        break;

      default:
        throw new Error('Unknown dialog type: ' + this.dialogType);
    }

    this.okButton_.textContent = okLabel;

    var dialogTitle = this.params_.title || defaultTitle;
    this.dialogDom_.querySelector('.dialog-title').textContent = dialogTitle;

    this.dialogDom_.setAttribute('type', this.dialogType);
  };

  FileManager.prototype.renderRoot_ = function(path) {
    var li = this.document_.createElement('li');
    li.className = 'root-item';
    li.setAttribute('role', 'option');
    var dm = this.directoryModel_;
    var handleClick = function() {
      if (li.selected && path !== dm.getCurrentDirPath()) {
        dm.changeDirectory(path);
      }
    };
    li.addEventListener('mousedown', handleClick);
    li.addEventListener(cr.ui.TouchHandler.EventType.TOUCH_START, handleClick);

    var rootType = PathUtil.getRootType(path);

    var iconDiv = this.document_.createElement('div');
    iconDiv.className = 'volume-icon';
    iconDiv.setAttribute('volume-type-icon', rootType);
    if (rootType === RootType.REMOVABLE) {
      iconDiv.setAttribute('volume-subtype',
          this.volumeManager_.getDeviceType(path));
    }
    li.appendChild(iconDiv);

    var div = this.document_.createElement('div');
    div.className = 'root-label';

    div.textContent = PathUtil.getRootLabel(path);
    li.appendChild(div);

    if (rootType === RootType.ARCHIVE || rootType === RootType.REMOVABLE) {
      var eject = this.document_.createElement('div');
      eject.className = 'root-eject';
      eject.addEventListener('click', function(event) {
        event.stopPropagation();
        var unmountCommand = this.dialogDom_.querySelector('command#unmount');
        // Let's make sure 'canExecute' state of the command is properly set for
        // the root before executing it.
        unmountCommand.canExecuteChange(li);
        unmountCommand.execute(li);
      }.bind(this));
      // Block other mouse handlers.
      eject.addEventListener('mouseup', function(e) { e.stopPropagation() });
      eject.addEventListener('mousedown', function(e) { e.stopPropagation() });
      li.appendChild(eject);
    }

    if (rootType != RootType.DRIVE && rootType != RootType.DOWNLOADS)
      cr.ui.contextMenuHandler.setContextMenu(li, this.rootsContextMenu_);

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
      this.alert.showHtml('', str('UNMOUNT_FAILED'));
    };
    this.volumeManager_.unmount(path, function() {}, onError.bind(this));
  };

  FileManager.prototype.refreshCurrentDirectoryMetadata_ = function() {
    var entries = this.directoryModel_.getFileList().slice();
    var directoryEntry = this.directoryModel_.getCurrentDirEntry();
    // We don't pass callback here. When new metadata arrives, we have an
    // observer registered to update the UI.

    // TODO(dgozman): refresh content metadata only when modificationTime
    // changed.
    var isFakeEntry = typeof directoryEntry.toURL !== 'function';
    var getEntries = (isFakeEntry ? [] : [directoryEntry]).concat(entries);
    this.metadataCache_.clearRecursively(directoryEntry, '*');
    this.metadataCache_.get(getEntries, 'filesystem', null);

    if (this.isOnDrive())
      this.metadataCache_.get(getEntries, 'drive', null);

    var visibleItems = this.currentList_.items;
    var visibleEntries = [];
    for (var i = 0; i < visibleItems.length; i++) {
      var index = this.currentList_.getIndexOfListItem(visibleItems[i]);
      var entry = this.directoryModel_.getFileList().item(index);
      // The following check is a workaround for the bug in list: sometimes item
      // does not have listIndex, and therefore is not found in the list.
      if (entry) visibleEntries.push(entry);
    }
    this.metadataCache_.get(visibleEntries, 'thumbnail', null);
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
    var propertyByUrl = urls.reduce(function(map, url, index) {
      map[url] = properties[index];
      return map;
    }, {});

    if (this.listType_ == FileManager.ListType.DETAIL)
      this.table_.updateListItemsMetadata(type, propertyByUrl);
    else
      this.grid_.updateListItemsMetadata(type, propertyByUrl);
    // TODO: update bottom panel thumbnails.
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
      this.table_.updateFileMetadata(leadListItem, leadEntry);
    }
    this.currentList_.restoreLeadItem(leadListItem);
  };

  /**
   * @return {boolean} True if the current directory content is from Google
   * Drive.
   */
  FileManager.prototype.isOnDrive = function() {
    return this.directoryModel_.getCurrentRootType() === RootType.DRIVE ||
           this.directoryModel_.getCurrentRootType() === RootType.DRIVE_OFFLINE;
  };

  /**
   * @return {boolean} True if the "Available offline" column should be shown in
   * the table layout.
   * @private
   */
  FileManager.prototype.shouldShowOfflineColumn = function() {
    return this.directoryModel_.getCurrentRootType() === RootType.DRIVE;
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

    // In a packaged apps links with targer='_blank' open in a new tab by
    // default, other links do not open at all.
    if (!util.platform.v2()) {
      chrome.tabs.create({url: event.target.href});
      event.preventDefault();
    }

    if (this.dialogType != DialogType.FULL_PAGE) {
      this.onCancel_();
    }
  };

  /**
   * Task combobox handler.
   *
   * @param {Object} event Event containing task which was clicked.
   */
  FileManager.prototype.onTaskItemClicked_ = function(event) {
    var selection = this.getSelection();
    if (!selection.tasks) return;

    if (event.item.task) {
      // Task field doesn't exist on change-default dropdown item.
      selection.tasks.execute(event.item.task.taskId);
    } else {
      var extensions = [];

      for (var i = 0; i < selection.urls.length; i++) {
        var match = /\.(\w+)$/g.exec(selection.urls[i]);
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
      selection.tasks.showTaskPicker(this.defaultTaskPicker,
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
    // TODO(dgozman): move this method closer to tasks.
    var selection = this.getSelection();
    chrome.fileBrowserPrivate.setDefaultTask(task.taskId,
      selection.urls, selection.mimeTypes);
    selection.tasks = new FileTasks(this);
    selection.tasks.init(selection.urls, selection.mimeTypes);
    selection.tasks.display(this.taskItems_);
    this.refreshCurrentDirectoryMetadata_();
    this.selectionHandler_.onFileSelectionChanged();
  };

  FileManager.prototype.updateNetworkStateAndPreferences_ = function(
      callback) {
    var self = this;
    var downcount = 2;
    var done = function() {
      if (--downcount == 0)
        callback();
    };

    this.getPreferences_(function(prefs) {
      done();
    }, true);

    chrome.fileBrowserPrivate.getDriveConnectionState(function(state) {
      self.driveConnectionState_ = state;
      done();
    });
  };

  FileManager.prototype.onNetworkStateOrPreferencesChanged_ = function() {
    var self = this;
    this.updateNetworkStateAndPreferences_(function() {
      var drive = self.preferences_;
      var connection = self.driveConnectionState_;

      self.initDateTimeFormatters_();
      self.refreshCurrentDirectoryMetadata_();

      self.directoryModel_.setDriveEnabled(self.isDriveEnabled());
      self.directoryModel_.setDriveOffline(connection.type == 'offline');

      if (drive.cellularDisabled)
        self.syncButton.setAttribute('checked', '');
      else
        self.syncButton.removeAttribute('checked');

      if (self.hostedButton.hasAttribute('checked') !=
          drive.hostedFilesDisabled && self.isOnDrive()) {
        self.directoryModel_.rescan();
      }

      if (!drive.hostedFilesDisabled)
        self.hostedButton.setAttribute('checked', '');
      else
        self.hostedButton.removeAttribute('checked');

      var hideDriveNotReachedMessage = true;
      switch (connection.type) {
        case DriveConnectionType.ONLINE:
          self.dialogContainer_.removeAttribute('connection');
          break;
        case DriveConnectionType.METERED:
          self.dialogContainer_.setAttribute('connection', 'metered');
          break;
        case DriveConnectionType.OFFLINE:
          if (connection.reasons.indexOf('not_ready') !== -1)
            hideDriveNotReachedMessage = false;

          self.dialogContainer_.setAttribute('connection', 'offline');
          break;
        default:
          console.assert(true, 'unknown connection type.');
      }
      self.authFailedWarning_.hidden = hideDriveNotReachedMessage;
      console.log(hideDriveNotReachedMessage);
    });
  };

  /**
   * Get the metered status of Drive connection.
   *
   * @return {boolean} Returns true if drive should limit the traffic because
   * the connection is metered and the 'disable-sync-on-metered' setting is
   * enabled. Otherwise, returns false.
   */
  FileManager.prototype.isDriveOnMeteredConnection = function() {
    return this.driveConnectionState_.type == DriveConnectionType.METERED;
  };

  /**
   * Get the online/offline status of drive.
   *
   * @return {boolean} Returns true if the connection is offline. Otherwise,
   * returns false.
   */
  FileManager.prototype.isDriveOffline = function() {
    return this.driveConnectionState_.type == DriveConnectionType.OFFLINE;
  };

  FileManager.prototype.isDriveEnabled = function() {
    return !this.params_.disableDrive &&
        (!('driveEnabled' in this.preferences_) ||
         this.preferences_.driveEnabled);
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
        util.platform.closeWindow();
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
    this.document_.body.setAttribute('overlay-visible', '');
  };

  FileManager.prototype.closeFilePopup_ = function() {
    if (this.filePopup_) {
      this.document_.body.removeAttribute('overlay-visible');
      // The window resize would not be processed properly while the relevant
      // divs had 'display:none', force resize after the layout fired.
      setTimeout(this.onResize_.bind(this), 0);
      if (this.filePopup_.contentWindow &&
          this.filePopup_.contentWindow.unload) {
        this.filePopup_.contentWindow.unload();
      }

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
   * @return {string} URL representing the current directory.
   */
  FileManager.prototype.getCurrentDirectoryURL = function() {
    return this.directoryModel_ &&
        this.directoryModel_.getCurrentDirectoryURL();
  };

  FileManager.prototype.deleteSelection = function() {
    // TODO(mtomasz): Remove this temporary dialog. crbug.com/167364
    var entries = this.getSelection().entries;
    var message = entries.length == 1 ?
        strf('GALLERY_CONFIRM_DELETE_ONE', entries[0].name) :
        strf('GALLERY_CONFIRM_DELETE_SOME', entries.length);
    this.confirm.show(message, function() {
      this.copyManager_.deleteEntries(entries);
    }.bind(this));
  };

  FileManager.prototype.blinkSelection = function() {
    var selection = this.getSelection();
    if (!selection || selection.totalCount == 0)
      return;

    for (var i = 0; i < selection.entries.length; i++) {
      var selectedIndex = selection.indexes[i];
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
    this.defaultPath = '';
  };

  /**
   * Handles mouse click or tap.
   * @param {Event} event The click event.
   */
  FileManager.prototype.onDetailClick_ = function(event) {
    if (this.isRenamingInProgress()) {
      // Don't pay attention to clicks during a rename.
      return;
    }

    var listItem = this.findListItemForEvent_(event);
    var selection = this.getSelection();
    if (!listItem || !listItem.selected || selection.totalCount != 1) {
      return;
    }

    var clickNumber;
    if (this.dialogType == DialogType.FULL_PAGE &&
            (event.target.parentElement.classList.contains('filename-label') ||
             event.target.classList.contains('detail-icon'))) {
      // If full page mode the file name and icon should react on single click.
      clickNumber = 1;
    } else if (this.lastClickedItem_ == listItem) {
      // React on double click, but only if both clicks hit the same item.
      clickNumber = 2;
    }
    this.lastClickedItem_ = listItem;

    if (event.detail != clickNumber)
      return;

    var entry = selection.entries[0];
    if (entry.isDirectory) {
      this.onDirectoryAction(entry);
    } else {
      this.dispatchSelectionAction_();
    }
  };

  FileManager.prototype.dispatchSelectionAction_ = function() {
    if (this.dialogType == DialogType.FULL_PAGE) {
      var tasks = this.getSelection().tasks;
      if (tasks) tasks.executeDefault();
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
      return this.butterBar_.show(ButterBar.Mode.ERROR,
                                  str('UNKNOWN_FILESYSTEM_WARNING'));
    } else if (mountError == VolumeManager.Error.UNSUPPORTED_FILESYSTEM) {
      return this.butterBar_.show(ButterBar.Mode.ERROR,
                                  str('UNSUPPORTED_FILESYSTEM_WARNING'));
    }

    return this.directoryModel_.changeDirectory(entry.fullPath);
  };

  /**
   * Update the tab title.
   */
  FileManager.prototype.updateTitle_ = function() {
    if (this.dialogType != DialogType.FULL_PAGE)
      return;

    var path = this.getCurrentDirectory();
    var rootPath = PathUtil.getRootPath(path);
    this.document_.title = PathUtil.getRootLabel(rootPath) +
                           path.substring(rootPath.length);
  };

  /**
   * Updates search box value when directory gets changed.
   */
  FileManager.prototype.updateSearchBoxOnDirChange_ = function() {
    var searchBox = this.dialogDom_.querySelector('#search-box');
    if (!searchBox.disabled)
      searchBox.value = '';
  };

  /**
   * Update the gear menu.
   */
  FileManager.prototype.updateGearMenu_ = function() {
    this.syncButton.hidden = !this.isOnDrive();
    this.hostedButton.hidden = !this.isOnDrive();

    // If volume has changed, then fetch remaining space data.
    if (this.previousRootUrl_ != this.directoryModel_.getCurrentRootUrl())
      this.refreshRemainingSpace_(true);  // Show loading caption.

    this.previousRootUrl_ = this.directoryModel_.getCurrentRootUrl();
  };

  /**
   * Refreshes space info of the current volume.
   * @param {boolean} showLoadingCaption Whether show loading caption or not.
   */
   FileManager.prototype.refreshRemainingSpace_ = function(showLoadingCaption) {
    var volumeSpaceInfoLabel =
        this.dialogDom_.querySelector('#volume-space-info-label');
    var volumeSpaceInnerBar =
        this.dialogDom_.querySelector('#volume-space-info-bar');
    var volumeSpaceOuterBar =
        this.dialogDom_.querySelector('#volume-space-info-bar').parentNode;

    volumeSpaceInnerBar.setAttribute('pending', '');

    if (showLoadingCaption) {
      volumeSpaceInfoLabel.innerText = str('WAITING_FOR_SPACE_INFO');
      volumeSpaceInnerBar.style.width = '100%';
    }

    var currentRootUrl = this.directoryModel_.getCurrentRootUrl();
    chrome.fileBrowserPrivate.getSizeStats(
        this.directoryModel_.getCurrentRootUrl(), function(result) {
          if (this.directoryModel_.getCurrentRootUrl() != currentRootUrl)
            return;
          updateSpaceInfo(result,
                          volumeSpaceInnerBar,
                          volumeSpaceInfoLabel,
                          volumeSpaceOuterBar);
        }.bind(this));
  };

  /**
   * Update the UI when the current directory changes.
   *
   * @param {cr.Event} event The directory-changed event.
   */
  FileManager.prototype.onDirectoryChanged_ = function(event) {
    this.selectionHandler_.onFileSelectionChanged();
    this.updateSearchBoxOnDirChange_();
    if (this.dialogType == DialogType.FULL_PAGE)
      this.table_.showOfflineColumn(this.shouldShowOfflineColumn());

    util.updateAppState(event.initial, this.getCurrentDirectory());

    if (this.closeOnUnmount_ && !event.initial &&
          PathUtil.getRootPath(event.previousDirEntry.fullPath) !=
          PathUtil.getRootPath(event.newDirEntry.fullPath)) {
      this.closeOnUnmount_ = false;
    }

    this.updateUnformattedDriveStatus_();
    this.updateTitle_();
    this.updateGearMenu_();
  };

  FileManager.prototype.updateUnformattedDriveStatus_ = function() {
    var volumeInfo = this.volumeManager_.getVolumeInfo_(
        this.directoryModel_.getCurrentRootPath());

    if (volumeInfo.error) {
      this.dialogDom_.setAttribute('unformatted', '');

      var errorNode = this.dialogDom_.querySelector('#format-panel > .error');
      if (volumeInfo.error == VolumeManager.Error.UNSUPPORTED_FILESYSTEM) {
        errorNode.textContent = str('UNSUPPORTED_FILESYSTEM_WARNING');
      } else {
        errorNode.textContent = str('UNKNOWN_FILESYSTEM_WARNING');
      }

      // Update 'canExecute' for format command so the format button's disabled
      // property is properly set.
      var formatCommand = this.dialogDom_.querySelector('command#format');
      formatCommand.canExecuteChange(errorNode);
    } else {
      this.dialogDom_.removeAttribute('unformatted');
    }
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
    if (this.filePopup_ &&
        this.filePopup_.contentWindow &&
        this.filePopup_.contentWindow.unload) {
      this.filePopup_.contentWindow.unload(true /* exiting */);
    }
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
    var validationDone = function(valid) {
      input.validation_ = false;
      // Alert dialog restores focus unless the item removed from DOM.
      if (this.document_.activeElement != input)
        this.cancelRename_();
      if (!valid)
        return;

      // Validation succeeded. Do renaming.

      this.cancelRename_();
      // Optimistically apply new name immediately to avoid flickering in
      // case of success.
      nameNode.textContent = newName;

      this.directoryModel_.doesExist(entry, newName, function(exists, isFile) {
        if (!exists) {
          var onError = function(err) {
            this.alert.show(strf('ERROR_RENAMING', entry.name,
                                 util.getFileErrorString(err.code)));
          };
          this.directoryModel_.renameEntry(entry, newName, onError.bind(this));
        } else {
          nameNode.textContent = entry.name;
          var message = isFile ? 'FILE_ALREADY_EXISTS' :
                                 'DIRECTORY_ALREADY_EXISTS';
          this.alert.show(strf(message, newName));
        }
      }.bind(this));
    };

    // TODO(haruki): this.getCurrentDirectoryURL() might not return the actual
    // parent if the directory content is a search result. Fix it to do proper
    // validation.
    this.validateFileName_(this.getCurrentDirectoryURL(),
                           newName,
                           validationDone.bind(this));
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
    var enabled = this.selectionHandler_.updateOkButton();
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

  FileManager.prototype.onScanStarted_ = function() {
    this.breadcrumbs_.update(
        this.directoryModel_.getCurrentRootPath(),
        this.directoryModel_.getCurrentDirPath());

    this.cancelSpinnerTimeout_();
    this.showSpinner_(false);
    this.showSpinnerTimeout_ =
        setTimeout(this.showSpinner_.bind(this, true), 500);
  };

  FileManager.prototype.cancelSpinnerTimeout_ = function() {
    if (this.showSpinnerTimeout_) {
      clearTimeout(this.showSpinnerTimeout_);
      this.showSpinnerTimeout_ = null;
    }
  };

  FileManager.prototype.hideSpinnerLater_ = function() {
    this.cancelSpinnerTimeout_();
    this.showSpinnerTimeout_ =
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

    var advance = function() {
      separator = ' (';
      suffix = ')';
      index++;
    };

    var current = function() {
      return baseName + separator + index + suffix;
    };

    // Accessing hasOwnProperty is safe since hash properties filtered.
    while (hash.hasOwnProperty(current())) {
      advance();
    }

    var self = this;
    var list = self.currentList_;
    var tryCreate = function() {
      self.directoryModel_.createDirectory(current(),
                                           onSuccess, onError);
    };

    var onSuccess = function(entry) {
      metrics.recordUserAction('CreateNewFolder');
      list.selectedItem = entry;
      self.initiateRename();
    };

    var onError = function(error) {
      self.alert.show(strf('ERROR_CREATING_FOLDER', current(),
                           util.getFileErrorString(error.code)));
    };

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

        if (this.dialogType != DialogType.FULL_PAGE) {
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
  };

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
        // TODO(dgozman): move directory action to dispatchSelectionAction.
        var selection = this.getSelection();
        if (selection.totalCount == 1 &&
            selection.entries[0].isDirectory &&
            this.dialogType != DialogType.SELECT_FOLDER) {
          event.preventDefault();
          this.onDirectoryAction(selection.entries[0]);
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
        this.setNoHover_(true);
        break;
    }
  };

  /**
   * Suppress/restore hover highlighting in the list container.
   * @param {boolean} on True to temporarity hide hover state.
   */
  FileManager.prototype.setNoHover_ = function(on) {
    if (on) {
      this.listContainer_.classList.add('nohover');
    } else {
      this.listContainer_.classList.remove('nohover');
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
    this.setNoHover_(false);
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
   * For drive files this involves some special treatment.
   * Starts getting drive files if needed.
   *
   * @param {Array.<string>} fileUrls Drive URLs.
   * @param {function(Array.<string>)} callback To be called with fixed URLs.
   */
  FileManager.prototype.resolveSelectResults_ = function(fileUrls, callback) {
    if (this.isOnDrive()) {
      chrome.fileBrowserPrivate.getDriveFiles(
        fileUrls,
        function(localPaths) {
          callback(fileUrls);
        });
    } else {
      callback(fileUrls);
    }
  };

  /**
   * Closes this modal dialog with some files selected.
   * TODO(jamescook): Make unload handler work automatically, crbug.com/104811
   * @param {Object} selection Contains urls, filterIndex and multiple fields.
   */
  FileManager.prototype.callSelectFilesApiAndClose_ = function(selection) {
    var self = this;
    function callback() {
      self.onUnload_();
      window.close();
    }
    if (selection.multiple) {
      chrome.fileBrowserPrivate.selectFiles(selection.urls, callback);
    } else {
      chrome.fileBrowserPrivate.selectFile(
          selection.urls[0], selection.filterIndex, callback);
    }
  };

  /**
   * Tries to close this modal dialog with some files selected.
   * Performs preprocessing if needed (e.g. for Drive).
   * @param {Object} selection Contains urls, filterIndex and multiple fields.
   */
  FileManager.prototype.selectFilesAndClose_ = function(selection) {
    if (!this.isOnDrive() ||
        this.dialogType == DialogType.SELECT_SAVEAS_FILE) {
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
    this.metadataCache_.get(selection.urls, 'drive', onProperties);
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
    if (this.dialogType == DialogType.SELECT_SAVEAS_FILE) {
      var currentDirUrl = this.getCurrentDirectoryURL();

      if (currentDirUrl.charAt(currentDirUrl.length - 1) != '/')
        currentDirUrl += '/';

      // Save-as doesn't require a valid selection from the list, since
      // we're going to take the filename from the text input.
      var filename = this.filenameInput_.value;
      if (!filename)
        throw new Error('Missing filename!');

      var self = this;
      var checkOverwriteAndFinish = function(valid) {
        if (!valid)
          return;

        var singleSelection = {
          urls: [currentDirUrl + encodeURIComponent(filename)],
          multiple: false,
          filterIndex: self.getSelectedFilterIndex_(filename)
        };

        var resolveErrorCallback = function(error) {
          // File does not exist.
          self.selectFilesAndClose_(singleSelection);
        };

        var resolveSuccessCallback = function(victim) {
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
        };

        self.resolvePath(self.getCurrentDirectory() + '/' + filename,
                         resolveSuccessCallback, resolveErrorCallback);
      };

      this.validateFileName_(currentDirUrl, filename, checkOverwriteAndFinish);
      return;
    }

    var files = [];
    var selectedIndexes = this.currentList_.selectionModel.selectedIndexes;

    if (this.dialogType == DialogType.SELECT_FOLDER &&
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
    if (this.dialogType == DialogType.SELECT_OPEN_MULTI_FILE) {
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

    if (this.dialogType == DialogType.SELECT_FOLDER) {
      if (!selectedEntry.isDirectory)
        throw new Error('Selected entry is not a folder!');
    } else if (this.dialogType == DialogType.SELECT_OPEN_FILE) {
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
   * It also verifies if the name length is in the limit of the filesystem.
   *
   * @param {string} parentUrl The URL of the parent directory entry.
   * @param {string} name New file or folder name.
   * @param {function} onDone Function to invoke when user closes the
   *    warning box or immediatelly if file name is correct. If the name was
   *    valid it is passed true, and false otherwise.
   */
  FileManager.prototype.validateFileName_ = function(parentUrl, name, onDone) {
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
      this.alert.show(msg, function() {
        onDone(false);
      });
      return;
    }

    var self = this;
    chrome.fileBrowserPrivate.validatePathNameLength(
        parentUrl, name, function(valid) {
          if (!valid) {
            self.alert.show(str('ERROR_LONG_NAME'),
                            function() { onDone(false); });
          } else {
            onDone(true);
          }
        });
  };

  /**
   * Handler invoked on preference setting in drive context menu.
   * @param {string} pref  The preference to alter.
   * @param {boolean} inverted Invert the value if true.
   * @param {Event}  event The click event.
   */
  FileManager.prototype.onDrivePrefClick_ = function(pref, inverted, event) {
    var newValue = !event.target.hasAttribute('checked');
    if (newValue)
      event.target.setAttribute('checked', 'checked');
    else
      event.target.removeAttribute('checked');

    var changeInfo = {};
    changeInfo[pref] = inverted ? !newValue : newValue;
    chrome.fileBrowserPrivate.setPreferences(changeInfo);
  };

  FileManager.prototype.onSearchBoxUpdate_ = function(event) {
    var searchString = this.document_.getElementById('search-box').value;
    var noResultsDiv = this.document_.getElementById('no-search-results');

    var reportEmptySearchResults = function() {
      if (this.directoryModel_.getFileList().length === 0) {
        // The string 'SEARCH_NO_MATCHING_FILES_HTML' may contain HTML tags,
        // hence we escapes |searchString| here.
        var html = strf('SEARCH_NO_MATCHING_FILES_HTML',
                        util.htmlEscape(searchString));
        noResultsDiv.innerHTML = html;
        noResultsDiv.setAttribute('show', 'true');
      } else {
        noResultsDiv.removeAttribute('show');
      }
    };

    var hideNoResultsDiv = function() {
      noResultsDiv.removeAttribute('show');
    };

    this.directoryModel_.search(searchString,
                                reportEmptySearchResults.bind(this),
                                hideNoResultsDiv.bind(this));
  };

  /**
   * Requests autocomplete suggestions for files on Drive.
   * Once the suggestions are returned, the autocomplete popup will show up.
   * @param {string} query The text to autocomplete from.
   * @private
   */
  FileManager.prototype.requestAutocompleteSuggestions_ = function(query) {
    if (!this.isOnDrive())
      return;
    this.lastQuery_ = query;

    // The autocomplete list should be resized and repositioned here as the
    // search box is resized when it's focused.
    this.autocompleteList_.syncWidthAndPositionToInput();

    chrome.fileBrowserPrivate.searchDriveMetadata(
      query,
      function(suggestions) {
        // searchDriveMetadata() is asynchronous hence the result of an old
        // query could be delivered at a later time.
        if (query != this.lastQuery_)
          return;
        this.autocompleteList_.suggestions = suggestions;
      }.bind(this));
  };

  /**
   * Creates a ListItem element for autocomple.
   * @param {Object} item An object representing a suggestion.
   * @return {HTMLElement} Element containing the autocomplete suggestions.
   * @private
   */
  FileManager.prototype.createAutocompleteListItem_ = function(item) {
    var li = new cr.ui.ListItem();
    li.itemInfo = item;
    var iconType = FileType.getIcon(item.entry);
    var icon = this.document_.createElement('div');
    icon.className = 'detail-icon';
    icon.setAttribute('file-type-icon', iconType);
    var text = this.document_.createElement('div');
    text.className = 'detail-text';
    // highlightedBaseName is a piece of HTML with meta characters properly
    // escaped. See the comment at fileBrowserPrivate.searchDriveMetadata().
    text.innerHTML = item.highlightedBaseName;
    li.appendChild(icon);
    li.appendChild(text);
    return li;
  };

  /**
   * Opens the currently selected suggestion item.
   * @private
   */
  FileManager.prototype.openAutocompleteSuggestion_ = function() {
    var entry = this.autocompleteList_.selectedItem.entry;
    // If the entry is a directory, just change the directory.
    if (entry.isDirectory) {
      this.onDirectoryAction(entry);
      return;
    }

    var urls = [entry.toURL()];
    var self = this;

    // To open a file, first get the mime type.
    this.metadataCache_.get(urls, 'drive', function(props) {
      var mimeType = props[0].contentMimeType || '';
      var mimeTypes = [mimeType];
      var openIt = function() {
        var tasks = new FileTasks(self);
        tasks.init(urls, mimeTypes);
        tasks.executeDefault();
      }

      // Change the current directory to the directory that contains the
      // selected file. Note that this is necessary for an image or a video,
      // which should be opened in the gallery mode, as the gallery mode
      // requires the entry to be in the current directory model. For
      // consistency, the current directory is always changed regardless of
      // the file type.
      entry.getParent(function(parent) {
        var onDirectoryChanged = function(event) {
          self.directoryModel_.removeEventListener('scan-completed',
                                                   onDirectoryChanged);
          self.directoryModel_.selectEntry(entry.name);
          openIt();
        }
        // changeDirectory() returns immediately. We should wait until the
        // directory scan is complete.
        self.directoryModel_.addEventListener('scan-completed',
                                              onDirectoryChanged);
        self.directoryModel_.changeDirectory(
          parent.fullPath,
          function() {
            // Remove the listner if the change directory failed.
            self.directoryModel_.removeEventListener('scan-completed',
                                                     onDirectoryChanged);
          });
      });
    });
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
   * Updates default action menu item to match passed taskItem (icon,
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

    this.openWithCommand_.canExecuteChange();

    this.openWithCommand_.setHidden(!(defaultItem && isMultiple));
    this.defaultActionMenuItem_.hidden = !defaultItem;
    defaultActionSeparator.hidden = !defaultItem;
  };


  /**
   * Window beforeunload handler.
   * @return {string} Message to show. Ignored when running as a packaged app.
   * @private
   */
  FileManager.prototype.onBeforeUnload_ = function() {
    this.butterBar_.forceDeleteAndHide();
    if (this.filePopup_ &&
        this.filePopup_.contentWindow &&
        this.filePopup_.contentWindow.beforeunload) {
      // The gallery might want to prevent the unload if it is busy.
      return this.filePopup_.contentWindow.beforeunload();
    }
    return null;
  };

  /**
   * @return {FileSelection} Selection object.
   */
  FileManager.prototype.getSelection = function() {
    return this.selectionHandler_.selection;
  };

  /**
   * @return {ArrayDataModel} File list.
   */
  FileManager.prototype.getFileList = function() {
    return this.directoryModel_.getFileList();
  };

  /**
   * @return {cr.ui.List} Current list object.
   */
  FileManager.prototype.getCurrentList = function() {
    return this.currentList_;
  };

  /**
   * Retrieve the preferences of the files.app. This method caches the result
   * and returns it unless opt_update is true.
   * @param {function(Object.<string, *>)} callback Callback to get the
   *     preference.
   * @param {boolean=} opt_update If is's true, don't use the cache and
   *     retrieve latest preference. Default is false.
   * @private
   */
  FileManager.prototype.getPreferences_ = function(callback, opt_update) {
    if (!opt_update && !this.preferences_ !== undefined) {
      callback(this.preferences_);
      return;
    }

    chrome.fileBrowserPrivate.getPreferences(function(prefs) {
      this.preferences_ = prefs;
      callback(prefs);
    }.bind(this));
  };
})();

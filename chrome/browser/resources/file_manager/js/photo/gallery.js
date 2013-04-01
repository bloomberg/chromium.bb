// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

util.addPageLoadHandler(function() {
  if (!location.hash)
    return;

  var pageState;
  if (location.search) {
    try {
      pageState = JSON.parse(decodeURIComponent(location.search.substr(1)));
    } catch (ignore) {}
  }
  Gallery.openStandalone(decodeURI(location.hash.substr(1)), pageState);
});

/**
 * Called from the main frame when unloading.
 * @return {string?} User-visible message on null if it is OK to close.
 */
function beforeunload() { return Gallery.instance.onBeforeUnload() }

/**
 * Called from the main frame when unloading.
 * @param {boolean=} opt_exiting True if the app is exiting.
 */
function unload(opt_exiting) { Gallery.instance.onUnload(opt_exiting) }

/**
 * Gallery for viewing and editing image files.
 *
 * @param {Object} context Object containing the following:
 *     {function(string)} onNameChange Called every time a selected
 *         item name changes (on rename and on selection change).
 *     {function} onClose
 *     {MetadataCache} metadataCache
 *     {Array.<Object>} shareActions
 *     {string} readonlyDirName Directory name for readonly warning or null.
 *     {DirEntry} saveDirEntry Directory to save to.
 *     {function(string)} displayStringFunction.
 * @class
 * @constructor
 */
function Gallery(context) {
  this.container_ = document.querySelector('.gallery');
  this.document_ = document;
  this.context_ = context;
  this.metadataCache_ = context.metadataCache;
  this.volumeManager_ = VolumeManager.getInstance();

  this.dataModel_ = new cr.ui.ArrayDataModel([]);
  this.selectionModel_ = new cr.ui.ListSelectionModel();

  var strf = context.displayStringFunction;
  this.displayStringFunction_ = function(id, formatArgs) {
    var args = Array.prototype.slice.call(arguments);
    args[0] = 'GALLERY_' + id.toUpperCase();
    return strf.apply(null, args);
  };

  this.initListeners_();
  this.initDom_();
}

/**
 * Gallery extends cr.EventTarget.
 */
Gallery.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Create and initialize a Gallery object based on a context.
 *
 * @param {Object} context Gallery context.
 * @param {Array.<string>} urls Array of urls.
 * @param {Array.<string>} selectedUrls Array of selected urls.
 */
Gallery.open = function(context, urls, selectedUrls) {
  Gallery.instance = new Gallery(context);
  Gallery.instance.load(urls, selectedUrls);
};

/**
 * Create a Gallery object in a tab.
 * @param {string} path File system path to a selected file.
 * @param {Object} pageState Page state object.
 * @param {function=} opt_callback Called when gallery object is constructed.
 */
Gallery.openStandalone = function(path, pageState, opt_callback) {
  ImageUtil.metrics = metrics;

  var currentDir;
  var urls = [];
  var selectedUrls = [];

  Gallery.getFileBrowserPrivate().requestLocalFileSystem(function(filesystem) {
    // If the path points to the directory scan it.
    filesystem.root.getDirectory(path, {create: false}, scanDirectory,
        function() {
          // Try to scan the parent directory.
          var pathParts = path.split('/');
          pathParts.pop();
          var parentPath = pathParts.join('/');
          filesystem.root.getDirectory(parentPath, {create: false},
              scanDirectory, open /* no data, just display an error */);
        });
  });

  function scanDirectory(dirEntry) {
    currentDir = dirEntry;
    util.forEachDirEntry(currentDir, function(entry) {
      if (entry == null) {
        open();
      } else if (FileType.isImageOrVideo(entry)) {
        var url = entry.toURL();
        urls.push(url);
        if (entry.fullPath == path)
          selectedUrls = [url];
      }
    });
  }

  function onClose() {
    // Exiting to the Files app seems arbitrary. Consider closing the tab.
    document.location = 'main.html?' +
        JSON.stringify({defaultPath: document.location.hash.substr(1)});
  }

  function open() {
    urls.sort();
    Gallery.getFileBrowserPrivate().getStrings(function(strings) {
      loadTimeData.data = strings;
      var context = {
        readonlyDirName: null,
        curDirEntry: currentDir,
        saveDirEntry: null,
        metadataCache: MetadataCache.createFull(),
        pageState: pageState,
        onClose: onClose,
        displayStringFunction: strf
      };
      Gallery.open(context, urls, selectedUrls);
      if (opt_callback) opt_callback();
    });
  }
};

/**
 * Tools fade-out timeout im milliseconds.
 * @const
 * @type {number}
 */
Gallery.FADE_TIMEOUT = 3000;

/**
 * First time tools fade-out timeout im milliseconds.
 * @const
 * @type {number}
 */
Gallery.FIRST_FADE_TIMEOUT = 1000;

/**
 * Time until mosaic is initialized in the background. Used to make gallery
 * in the slide mode load faster. In miiliseconds.
 * @const
 * @type {number}
 */
Gallery.MOSAIC_BACKGROUND_INIT_DELAY = 1000;

/**
 * Types of metadata Gallery uses (to query the metadata cache).
 * @const
 * @type {string}
 */
Gallery.METADATA_TYPE = 'thumbnail|filesystem|media|streaming';

/**
 * Initialize listeners.
 * @private
 */
Gallery.prototype.initListeners_ = function() {
  if (!util.TEST_HARNESS)
    this.document_.oncontextmenu = function(e) { e.preventDefault(); };

  this.keyDownBound_ = this.onKeyDown_.bind(this);
  this.document_.body.addEventListener('keydown', this.keyDownBound_);

  util.disableBrowserShortcutKeys(this.document_);
  if (!util.platform.v2())
    util.enableNewFullScreenHandler(this.document_);

  this.inactivityWatcher_ = new MouseInactivityWatcher(
      this.container_, Gallery.FADE_TIMEOUT, this.hasActiveTool.bind(this));

  // Search results may contain files from different subdirectories so
  // the observer is not going to work.
  if (!this.context_.searchResults) {
    this.thumbnailObserverId_ = this.metadataCache_.addObserver(
        this.context_.curDirEntry,
        MetadataCache.CHILDREN,
        'thumbnail',
        this.updateThumbnails_.bind(this));
  }

  this.volumeManager_.addEventListener('externally-unmounted',
      this.onExternallyUnmounted_.bind(this));
};

/**
 * Closes gallery when a volume containing the selected item is unmounted.
 * @param {Event} event The unmount event.
 * @private
 */
Gallery.prototype.onExternallyUnmounted_ = function(event) {
  if (!this.selectedItemFilesystemPath_)
    return;
  if (this.selectedItemFilesystemPath_.indexOf(event.mountPath) == 0)
    this.onClose_();
};

/**
 * Beforeunload handler.
 * @return {string?} User-visible message on null if it is OK to close.
 */
Gallery.prototype.onBeforeUnload = function() {
  return this.slideMode_.onBeforeUnload();
};

/**
 * Unload the Gallery.
 * @param {boolean} exiting True if the app is exiting.
 */
Gallery.prototype.onUnload = function(exiting) {
  if (!this.context_.searchResults) {
    this.metadataCache_.removeObserver(this.thumbnailObserverId_);
  }
  this.slideMode_.onUnload(exiting);
};

/**
 * Initializes DOM UI
 * @private
 */
Gallery.prototype.initDom_ = function() {
  var content = util.createChild(this.container_, 'content');
  content.addEventListener('click', this.onContentClick_.bind(this));

  var closeButton = util.createChild(this.container_, 'close tool dimmable');
  util.createChild(closeButton);
  closeButton.addEventListener('click', this.onClose_.bind(this));

  this.toolbar_ = util.createChild(this.container_, 'toolbar tool dimmable');

  this.filenameSpacer_ = util.createChild(this.toolbar_, 'filename-spacer');

  this.filenameEdit_ = util.createChild(this.filenameSpacer_,
                                        'namebox', 'input');

  this.filenameEdit_.setAttribute('type', 'text');
  this.filenameEdit_.addEventListener('blur',
      this.onFilenameEditBlur_.bind(this));

  this.filenameEdit_.addEventListener('focus',
      this.onFilenameFocus_.bind(this));

  this.filenameEdit_.addEventListener('keydown',
      this.onFilenameEditKeydown_.bind(this));

  util.createChild(this.toolbar_, 'button-spacer');

  this.prompt_ = new ImageEditor.Prompt(
      this.container_, this.displayStringFunction_);

  var onThumbnailError = this.context_.onThumbnailError || function() {};

  this.modeButton_ = util.createChild(this.toolbar_, 'button mode', 'button');
  this.modeButton_.addEventListener('click',
      this.toggleMode_.bind(this, null));

  this.mosaicMode_ = new MosaicMode(content,
      this.dataModel_, this.selectionModel_, this.metadataCache_,
      this.toggleMode_.bind(this, null), onThumbnailError);

  this.slideMode_ = new SlideMode(this.container_, content,
      this.toolbar_, this.prompt_,
      this.dataModel_, this.selectionModel_, this.context_,
      this.toggleMode_.bind(this), onThumbnailError,
      this.displayStringFunction_);
  this.slideMode_.addEventListener('image-displayed', function() {
    cr.dispatchSimpleEvent(this, 'image-displayed');
  }.bind(this));
  this.slideMode_.addEventListener('image-saved', function() {
    cr.dispatchSimpleEvent(this, 'image-saved');
  }.bind(this));

  var deleteButton = this.createToolbarButton_('delete', 'delete');
  deleteButton.addEventListener('click', this.onDelete_.bind(this));

  this.shareButton_ = this.createToolbarButton_('share', 'share');
  this.shareButton_.setAttribute('disabled', '');
  this.shareButton_.addEventListener('click', this.toggleShare_.bind(this));

  this.shareMenu_ = util.createChild(this.container_, 'share-menu');
  this.shareMenu_.hidden = true;
  util.createChild(this.shareMenu_, 'bubble-point');

  this.dataModel_.addEventListener('splice', this.onSplice_.bind(this));
  this.dataModel_.addEventListener('content', this.onContentChange_.bind(this));

  this.selectionModel_.addEventListener('change', this.onSelection_.bind(this));

  this.slideMode_.addEventListener('useraction', this.onUserAction_.bind(this));
};

/**
 * Creates toolbar button.
 *
 * @param {string} clazz Class to add.
 * @param {string} title Button title.
 * @return {HTMLElement} Newly created button.
 * @private
 */
Gallery.prototype.createToolbarButton_ = function(clazz, title) {
  var button = util.createChild(this.toolbar_, clazz, 'button');
  button.title = this.displayStringFunction_(title);
  return button;
};

/**
 * Load the content.
 *
 * @param {Array.<string>} urls Array of urls.
 * @param {Array.<string>} selectedUrls Array of selected urls.
 */
Gallery.prototype.load = function(urls, selectedUrls) {
  var items = [];
  for (var index = 0; index < urls.length; ++index) {
    items.push(new Gallery.Item(urls[index]));
  }
  this.dataModel_.push.apply(this.dataModel_, items);

  this.selectionModel_.adjustLength(this.dataModel_.length);

  for (var i = 0; i != selectedUrls.length; i++) {
    var selectedIndex = urls.indexOf(selectedUrls[i]);
    if (selectedIndex >= 0)
      this.selectionModel_.setIndexSelected(selectedIndex, true);
    else
      console.error('Cannot select ' + selectedUrls[i]);
  }

  if (this.selectionModel_.selectedIndexes.length == 0)
    this.onSelection_();

  var mosaic = this.mosaicMode_ && this.mosaicMode_.getMosaic();

  // Mosaic view should show up if most of the selected files are images.
  var imagesCount = 0;
  for (var i = 0; i != selectedUrls.length; i++) {
    if (FileType.getMediaType(selectedUrls[i]) == 'image')
      imagesCount++;
  }
  var mostlyImages = imagesCount > (selectedUrls.length / 2.0);

  var forcedMosaic = (this.context_.pageState &&
       this.context_.pageState.gallery == 'mosaic');

  var showMosaic = (mostlyImages && selectedUrls.length > 1) || forcedMosaic;
  if (mosaic && showMosaic) {
    this.setCurrentMode_(this.mosaicMode_);
    mosaic.init();
    mosaic.show();
    this.inactivityWatcher_.check();  // Show the toolbar.
    cr.dispatchSimpleEvent(this, 'loaded');
  } else {
    this.setCurrentMode_(this.slideMode_);
    var maybeLoadMosaic = function() {
      if (mosaic)
        mosaic.init();
      cr.dispatchSimpleEvent(this, 'loaded');
    }.bind(this);
    /* TODO: consider nice blow-up animation for the first image */
    this.slideMode_.enter(null, function() {
        // Flash the toolbar briefly to show it is there.
        this.inactivityWatcher_.kick(Gallery.FIRST_FADE_TIMEOUT);
      }.bind(this),
      maybeLoadMosaic);
  }
};

/**
 * Close the Gallery.
 * @private
 */
Gallery.prototype.close_ = function() {
  if (util.isFullScreen()) {
    util.toggleFullScreen(this.document_,
                          false);  // Leave the full screen mode.
  }
  this.context_.onClose(this.getSelectedUrls());
};

/**
 * Handle user's 'Close' action (Escape or a click on the X icon).
 * @private
 */
Gallery.prototype.onClose_ = function() {
  this.executeWhenReady(this.close_.bind(this));
};

/**
 * Execute a function when the editor is done with the modifications.
 * @param {function} callback Function to execute.
 */
Gallery.prototype.executeWhenReady = function(callback) {
  this.currentMode_.executeWhenReady(callback);
};

/**
 * @return {Object} File browser private API.
 */
Gallery.getFileBrowserPrivate = function() {
  return chrome.fileBrowserPrivate || window.top.chrome.fileBrowserPrivate;
};

/**
 * @return {boolean} True if some tool is currently active.
 */
Gallery.prototype.hasActiveTool = function() {
  return this.currentMode_.hasActiveTool() ||
      this.isSharing_() || this.isRenaming_();
};

/**
* External user action event handler.
* @private
*/
Gallery.prototype.onUserAction_ = function() {
  this.closeShareMenu_();
  // Show the toolbar and hide it after the default timeout.
  this.inactivityWatcher_.kick();
};

/**
 * Set the current mode, update the UI.
 * @param {Object} mode Current mode.
 * @private
 */
Gallery.prototype.setCurrentMode_ = function(mode) {
  if (mode != this.slideMode_ && mode != this.mosaicMode_)
    console.error('Invalid Gallery mode');

  this.currentMode_ = mode;
  if (this.modeButton_) {
    var oppositeMode =
        mode == this.slideMode_ ? this.mosaicMode_ : this.slideMode_;
    this.modeButton_.title =
        this.displayStringFunction_(oppositeMode.getName());
  }
  this.container_.setAttribute('mode', this.currentMode_.getName());
  this.updateSelectionAndState_();
};

/**
 * Mode toggle event handler.
 * @param {function=} opt_callback Callback.
 * @param {Event=} opt_event Event that caused this call.
 * @private
 */
Gallery.prototype.toggleMode_ = function(opt_callback, opt_event) {
  if (!this.modeButton_)
    return;

  if (this.changingMode_) // Do not re-enter while changing the mode.
    return;

  if (opt_event)
    this.onUserAction_();

  this.changingMode_ = true;

  var onModeChanged = function() {
    this.changingMode_ = false;
    if (opt_callback) opt_callback();
  }.bind(this);

  var tileIndex = Math.max(0, this.selectionModel_.selectedIndex);

  var mosaic = this.mosaicMode_.getMosaic();
  var tileRect = mosaic.getTileRect(tileIndex);

  if (this.currentMode_ == this.slideMode_) {
    this.setCurrentMode_(this.mosaicMode_);
    mosaic.transform(
        tileRect, this.slideMode_.getSelectedImageRect(), true /* instant */);
    this.slideMode_.leave(tileRect,
        function() {
          // Animate back to normal position.
          mosaic.transform();
          mosaic.show();
          onModeChanged();
        }.bind(this));
  } else {
    this.setCurrentMode_(this.slideMode_);
    this.slideMode_.enter(tileRect,
        function() {
          // Animate to zoomed position.
          mosaic.transform(tileRect, this.slideMode_.getSelectedImageRect());
          mosaic.hide();
        }.bind(this),
        onModeChanged);
  }
};

/**
 * Delete event handler.
 * @private
 */
Gallery.prototype.onDelete_ = function() {
  this.onUserAction_();

  // Clone the sorted selected indexes array.
  var indexesToRemove = this.selectionModel_.selectedIndexes.slice();
  if (!indexesToRemove.length)
    return;

  /* TODO(dgozman): Implement Undo delete, Remove the confirmation dialog. */

  var itemsToRemove = this.getSelectedItems();
  var plural = itemsToRemove.length > 1;
  var param = plural ? itemsToRemove.length : itemsToRemove[0].getFileName();

  function deleteNext() {
    if (!itemsToRemove.length)
      return;  // All deleted.

    var url = itemsToRemove.pop().getUrl();
    webkitResolveLocalFileSystemURL(url,
        function(entry) {
          entry.remove(deleteNext,
              util.flog('Error deleting ' + url, deleteNext));
        },
        util.flog('Error resolving ' + url, deleteNext));
  }

  // Prevent the Gallery from handling Esc and Enter.
  this.document_.body.removeEventListener('keydown', this.keyDownBound_);
  var restoreListener = function() {
    this.document_.body.addEventListener('keydown', this.keyDownBound_);
  }.bind(this);

  cr.ui.dialogs.BaseDialog.OK_LABEL = this.displayStringFunction_('OK_LABEL');
  cr.ui.dialogs.BaseDialog.CANCEL_LABEL =
      this.displayStringFunction_('CANCEL_LABEL');
  var confirm = new cr.ui.dialogs.ConfirmDialog(this.container_);
  confirm.show(this.displayStringFunction_(
      plural ? 'CONFIRM_DELETE_SOME' : 'CONFIRM_DELETE_ONE', param),
      function() {
        restoreListener();
        this.selectionModel_.unselectAll();
        this.selectionModel_.leadIndex = -1;
        // Remove items from the data model, starting from the highest index.
        while (indexesToRemove.length)
          this.dataModel_.splice(indexesToRemove.pop(), 1);
        // Delete actual files.
        deleteNext();
      }.bind(this),
      function() {
        // Restore the listener after a timeout so that ESC is processed.
        setTimeout(restoreListener, 0);
      });
};

/**
 * @return {Array.<Gallery.Item>} Current selection.
 */
Gallery.prototype.getSelectedItems = function() {
  return this.selectionModel_.selectedIndexes.map(
      this.dataModel_.item.bind(this.dataModel_));
};

/**
 * @return {Array.<string>} Array of currently selected urls.
 */
Gallery.prototype.getSelectedUrls = function() {
  return this.selectionModel_.selectedIndexes.map(function(index) {
    return this.dataModel_.item(index).getUrl();
  }.bind(this));
};

/**
 * @return {Gallery.Item} Current single selection.
 */
Gallery.prototype.getSingleSelectedItem = function() {
  var items = this.getSelectedItems();
  if (items.length > 1)
    throw new Error('Unexpected multiple selection');
  return items[0];
};

/**
  * Selection change event handler.
  * @private
  */
Gallery.prototype.onSelection_ = function() {
  this.updateSelectionAndState_();
  this.updateShareMenu_();
};

/**
  * Data model splice event handler.
  * @private
  */
Gallery.prototype.onSplice_ = function() {
  this.selectionModel_.adjustLength(this.dataModel_.length);
};

/**
 * Content change event handler.
 * @param {Event} event Event.
 * @private
*/
Gallery.prototype.onContentChange_ = function(event) {
  var index = this.dataModel_.indexOf(event.item);
  if (index != this.selectionModel_.selectedIndex)
    console.error('Content changed for unselected item');
  this.updateSelectionAndState_();
};

/**
 * Keydown handler.
 *
 * @param {Event} event Event.
 * @private
 */
Gallery.prototype.onKeyDown_ = function(event) {
  var wasSharing = this.isSharing_();
  this.closeShareMenu_();

  if (this.currentMode_.onKeyDown(event))
    return;

  switch (util.getKeyModifiers(event) + event.keyIdentifier) {
    case 'U+0008': // Backspace.
      // The default handler would call history.back and close the Gallery.
      event.preventDefault();
      break;

    case 'U+001B':  // Escape
      // Swallow Esc if it closed the Share menu, otherwise close the Gallery.
      if (!wasSharing)
        this.onClose_();
      break;

    case 'U+004D':  // 'm' switches between Slide and Mosaic mode.
      this.toggleMode_(null, event);
      break;


    case 'U+0056':  // 'v'
      this.slideMode_.startSlideshow(SlideMode.SLIDESHOW_INTERVAL_FIRST, event);
      return;

    case 'U+007F':  // Delete
    case 'Shift-U+0033':  // Shift+'3' (Delete key might be missing).
      this.onDelete_();
      break;
  }
};

// Name box and rename support.

/**
 * Update the UI related to the selected item and the persistent state.
 *
 * @private
 */
Gallery.prototype.updateSelectionAndState_ = function() {
  var path;
  var displayName = '';

  var selectedItems = this.getSelectedItems();
  if (selectedItems.length == 1) {
    var item = selectedItems[0];
    path = util.extractFilePath(item.getUrl());
    var fullName = item.getFileName();
    window.top.document.title = fullName;
    displayName = ImageUtil.getFileNameFromFullName(fullName);
  } else if (selectedItems.length > 1) {
    // If the Gallery was opened on search results the search query will not be
    // recorded in the app state and the relaunch will just open the gallery
    // in the curDirEntry directory.
    path = this.context_.curDirEntry.fullPath;
    window.top.document.title = this.context_.curDirEntry.name;
    displayName =
        this.displayStringFunction_('ITEMS_SELECTED', selectedItems.length);
  }

  window.top.util.updateAppState(true /*replace*/, path,
      {gallery: (this.currentMode_ == this.mosaicMode_ ? 'mosaic' : 'slide')});


  // We can't rename files in readonly directory.
  // We can only rename a single file.
  this.filenameEdit_.disabled = selectedItems.length != 1 ||
                                this.context_.readonlyDirName;

  this.filenameEdit_.value = displayName;

  // Resolve real filesystem path of the current file.
  if (this.selectionModel_.selectedIndexes.length) {
    var selectedIndex = this.selectionModel_.selectedIndex;
    var selectedItem =
        this.dataModel_.item(this.selectionModel_.selectedIndex);

    this.selectedItemFilesystemPath_ = null;
    webkitResolveLocalFileSystemURL(selectedItem.getUrl(),
      function(entry) {
        if (this.selectionModel_.selectedIndex != selectedIndex)
          return;
        this.selectedItemFilesystemPath_ = entry.fullPath;
      }.bind(this));
  }
};

/**
 * Click event handler on filename edit box
 * @private
 */
Gallery.prototype.onFilenameFocus_ = function() {
  ImageUtil.setAttribute(this.filenameSpacer_, 'renaming', true);
  this.filenameEdit_.originalValue = this.filenameEdit_.value;
  setTimeout(this.filenameEdit_.select.bind(this.filenameEdit_), 0);
  this.onUserAction_();
};

/**
 * Blur event handler on filename edit box.
 *
 * @param {Event} event Blur event.
 * @return {boolean} if default action should be prevented.
 * @private
 */
Gallery.prototype.onFilenameEditBlur_ = function(event) {
  if (this.filenameEdit_.value && this.filenameEdit_.value[0] == '.') {
    this.prompt_.show('file_hidden_name', 5000);
    this.filenameEdit_.focus();
    event.stopPropagation();
    event.preventDefault();
    return false;
  }

  var item = this.getSingleSelectedItem();
  var oldUrl = item.getUrl();

  var onFileExists = function() {
    this.prompt_.show('file_exists', 3000);
    this.filenameEdit_.value = name;
    this.filenameEdit_.focus();
  }.bind(this);

  var onSuccess = function() {
    var e = new cr.Event('content');
    e.item = item;
    e.oldUrl = oldUrl;
    e.metadata = null;  // Metadata unchanged.
    this.dataModel_.dispatchEvent(e);
  }.bind(this);

  if (this.filenameEdit_.value) {
    this.getSingleSelectedItem().rename(
        this.filenameEdit_.value, onSuccess, onFileExists);
  }

  ImageUtil.setAttribute(this.filenameSpacer_, 'renaming', false);
  this.onUserAction_();
};

/**
 * Keydown event handler on filename edit box
 * @private
 */
Gallery.prototype.onFilenameEditKeydown_ = function() {
  switch (event.keyCode) {
    case 27:  // Escape
      this.filenameEdit_.value = this.filenameEdit_.originalValue;
      this.filenameEdit_.blur();
      break;

    case 13:  // Enter
      this.filenameEdit_.blur();
      break;
  }
  event.stopPropagation();
};

/**
 * @return {boolean} True if file renaming is currently in progress.
 * @private
 */
Gallery.prototype.isRenaming_ = function() {
  return this.filenameSpacer_.hasAttribute('renaming');
};

/**
 * Content area click handler.
 * @private
 */
Gallery.prototype.onContentClick_ = function() {
  this.closeShareMenu_();
  this.filenameEdit_.blur();
};

// Share button support.

/**
 * @return {boolean} True if the Share menu is active.
 * @private
 */
Gallery.prototype.isSharing_ = function() {
  return !this.shareMenu_.hidden;
};

/**
 * Close Share menu if it is open.
 * @private
 */
Gallery.prototype.closeShareMenu_ = function() {
  if (this.isSharing_())
    this.toggleShare_();
};

/**
 * Share button handler.
 * @private
 */
Gallery.prototype.toggleShare_ = function() {
  if (!this.shareButton_.hasAttribute('disabled'))
    this.shareMenu_.hidden = !this.shareMenu_.hidden;
  this.inactivityWatcher_.check();
};

/**
 * Update available actions list based on the currently selected urls.
 * @private.
 */
Gallery.prototype.updateShareMenu_ = function() {
  var urls = this.getSelectedUrls();

  var internalId = util.platform.getAppId();
  function isShareAction(task) {
    var taskParts = task.taskId.split('|');
    return taskParts[0] != internalId;
  }

  var api = Gallery.getFileBrowserPrivate();
  var mimeTypes = [];  // TODO(kaznacheev) Collect mime types properly.
  api.getFileTasks(urls, mimeTypes, function(tasks) {
    var wasHidden = this.shareMenu_.hidden;
    this.shareMenu_.hidden = true;
    var items = this.shareMenu_.querySelectorAll('.item');
    for (var i = 0; i != items.length; i++) {
      items[i].parentNode.removeChild(items[i]);
    }

    for (var t = 0; t != tasks.length; t++) {
      var task = tasks[t];
      if (!isShareAction(task)) continue;

      var item = util.createChild(this.shareMenu_, 'item');
      item.textContent = task.title;
      item.style.backgroundImage = 'url(' + task.iconUrl + ')';
      item.addEventListener('click', function(taskId) {
        this.toggleShare_();  // Hide the menu.
        this.executeWhenReady(api.executeTask.bind(api, taskId, urls));
      }.bind(this, task.taskId));
    }

    var empty = this.shareMenu_.querySelector('.item') == null;
    ImageUtil.setAttribute(this.shareButton_, 'disabled', empty);
    this.shareMenu_.hidden = wasHidden || empty;
  }.bind(this));
};

/**
 * Update thumbnails.
 * @private
 */
Gallery.prototype.updateThumbnails_ = function() {
  if (this.currentMode_ == this.slideMode_)
    this.slideMode_.updateThumbnails();

  if (this.mosaicMode_) {
    var mosaic = this.mosaicMode_.getMosaic();
    if (mosaic.isInitialized())
      mosaic.reload();
  }
};

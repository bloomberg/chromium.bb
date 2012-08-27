// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

document.addEventListener('DOMContentLoaded', function() {
  if (document.location.hash)  // File path passed after the #.
    Gallery.openStandalone(decodeURI(document.location.hash.substr(1)));
});

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
 *     {function(string)} displayStringFunction
 * @class
 * @constructor
 */
function Gallery(context) {
  this.container_ = document.querySelector('.gallery');
  this.document_ = document;
  this.context_ = context;
  this.metadataCache_ = context.metadataCache;

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
 * Create and initialize a Gallery object based on a context.
 *
 * @param {Object} context Gallery context.
 * @param {Array.<string>} urls Array of image urls.
 * @param {string} selectedUrl Selected url.
 */
Gallery.open = function(context, urls, selectedUrl) {
  Gallery.instance = new Gallery(context);
  Gallery.instance.load(urls, selectedUrl);
};

/**
 * Create a Gallery object in a tab.
 * @param {string} path File system path to a selected file.
 */
Gallery.openStandalone = function(path) {
  ImageUtil.metrics = metrics;

  var currentDir;
  var urls = [];
  var selectedUrl;

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
          selectedUrl = url;
      }
    });
  }

  function onNameChange(name) {
    window.top.document.title = name;

    var newPath = currentDir.fullPath + '/' + name;
    var location = document.location.origin + document.location.pathname +
                   '#' + encodeURI(newPath);
    history.replaceState(undefined, newPath, location);
  }

  function onClose() {
    document.location = 'main.html?' +
        JSON.stringify({defaultPath: document.location.hash.substr(1)});
  }

  function open() {
    Gallery.getFileBrowserPrivate().getStrings(function(strings) {
      loadTimeData.data = strings;
      var context = {
        readonlyDirName: null,
        saveDirEntry: currentDir,
        metadataCache: MetadataCache.createFull(),
        onNameChange: onNameChange,
        onClose: onClose,
        displayStringFunction: strf
      };
      Gallery.open(context, urls, selectedUrl || urls[0]);
    });
  }
};

/**
 * Tools fade-out timeout im milliseconds.
 * @type {Number}
 */
Gallery.FADE_TIMEOUT = 3000;

/**
 * First time tools fade-out timeout im milliseconds.
 * @type {Number}
 */
Gallery.FIRST_FADE_TIMEOUT = 1000;

/**
 * Types of metadata Gallery uses (to query the metadata cache).
 */
Gallery.METADATA_TYPE = 'thumbnail|filesystem|media|streaming';

/**
 * Initialize listeners.
 * @private
 */

Gallery.prototype.initListeners_ = function() {
  this.document_.oncontextmenu = function(e) { e.preventDefault(); };

  this.document_.body.addEventListener('keydown', this.onKeyDown_.bind(this));

  this.inactivityWatcher_ = new MouseInactivityWatcher(
      this.container_, Gallery.FADE_TIMEOUT, this.hasActiveTool.bind(this));

  // Show tools when the user touches the screen.
  this.document_.body.addEventListener('touchstart',
      this.inactivityWatcher_.startActivity.bind(this.inactivityWatcher_));
  var initiateFading =
      this.inactivityWatcher_.stopActivity.bind(this.inactivityWatcher_,
          Gallery.FADE_TIMEOUT);
  this.document_.body.addEventListener('touchend', initiateFading);
  this.document_.body.addEventListener('touchcancel', initiateFading);
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

  var nameBox = util.createChild(this.filenameSpacer_, 'namebox');

  this.filenameText_ = util.createChild(nameBox);
  this.filenameText_.addEventListener('click',
      this.onFilenameClick_.bind(this));

  this.filenameEdit_ = this.document_.createElement('input');
  this.filenameEdit_.setAttribute('type', 'text');
  this.filenameEdit_.addEventListener('blur',
      this.onFilenameEditBlur_.bind(this));
  this.filenameEdit_.addEventListener('keydown',
      this.onFilenameEditKeydown_.bind(this));
  nameBox.appendChild(this.filenameEdit_);

  util.createChild(this.toolbar_, 'button-spacer');

  this.prompt_ = new ImageEditor.Prompt(
      this.container_, this.displayStringFunction_);

  this.slideMode_ = new SlideMode(this.container_, this.toolbar_, this.prompt_,
      this.context_, this.displayStringFunction_);
  this.slideMode_.addEventListener('edit', this.onEdit_.bind(this));
  this.slideMode_.addEventListener('selection', this.onSelection_.bind(this));

  this.shareMode_ = new ShareMode(
      this.slideMode_.editor_, this.container_, this.toolbar_,
      this.onShare_.bind(this), this.executeWhenReady.bind(this),
      this.displayStringFunction_);

  Gallery.getFileBrowserPrivate().isFullscreen(function(fullscreen) {
    this.originalFullscreen_ = fullscreen;
  }.bind(this));
};

/**
 * Load the content.
 *
 * @param {Array.<string>} urls Array of urls.
 * @param {string} selectedUrl Selected url.
 */
Gallery.prototype.load = function(urls, selectedUrl) {
  this.items_ = [];
  for (var index = 0; index < urls.length; ++index) {
    this.items_.push(new Gallery.Item(urls[index]));
  }

  var selectedIndex = urls.indexOf(selectedUrl);
  this.slideMode_.load(this.items_, selectedIndex, function() {
    // Flash the toolbar briefly to show it is there.
    this.inactivityWatcher_.startActivity();
    this.inactivityWatcher_.stopActivity(Gallery.FIRST_FADE_TIMEOUT);
  }.bind(this));
};

/**
 * Close the Gallery.
 * @private
 */
Gallery.prototype.close_ = function() {
  Gallery.getFileBrowserPrivate().isFullscreen(function(fullscreen) {
    if (this.originalFullscreen_ != fullscreen) {
      Gallery.toggleFullscreen();
    }
    this.context_.onClose();
  }.bind(this));
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
  //TODO(kaznacheev): Execute directly when in grid mode.
  this.slideMode_.editor_.executeWhenReady(callback);
};

/**
 * @return {Object} File browser private API.
 */
Gallery.getFileBrowserPrivate = function() {
  return chrome.fileBrowserPrivate || window.top.chrome.fileBrowserPrivate;
};

/**
 * Switches gallery to fullscreen mode and back.
 */
Gallery.toggleFullscreen = function() {
  Gallery.getFileBrowserPrivate().toggleFullscreen();
};

/**
 * @return {boolean} True if some tool is currently active.
 */
Gallery.prototype.hasActiveTool = function() {
  return this.slideMode_.isEditing() || this.isSharing_() || this.isRenaming_();
};

/**
 * Check if the tools are active and notify the inactivity watcher.
 * @private
 */
Gallery.prototype.checkActivity_ = function() {
  if (this.hasActiveTool())
    this.inactivityWatcher_.startActivity();
  else
    this.inactivityWatcher_.stopActivity();
};

/**
* Edit toggle event handler.
* @private
*/
Gallery.prototype.onEdit_ = function() {
  // The user has just clicked on the Edit button. Dismiss the Share menu.
  if (this.isSharing_())
    this.onShare_();
  this.checkActivity_();
};

/**
 * @return {Array.<Gallery.Item>} Current selection.
 */
Gallery.prototype.getSelectedItems = function() {
  // TODO(kaznacheev) support multiple selection grid/mosaic mode.
  return [this.slideMode_.getSelectedItem()];
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
  this.updateFilename_();
  this.shareMode_.updateMenu(
      this.getSelectedItems().map(function(item) { return item.getUrl() }));
};

/**
 * Keydown handler.
 * @param {Event} event Event.
 * @private
 */
Gallery.prototype.onKeyDown_ = function(event) {
  if (this.slideMode_.onKeyDown(event))
    return;

  switch (util.getKeyModifiers(event) + event.keyIdentifier) {
    case 'U+0008': // Backspace.
      // The default handler would call history.back and close the Gallery.
      event.preventDefault();
      break;

    case 'U+001B':  // Escape
      if (this.isSharing_())
        this.onShare_();
      else
        this.onClose_();
      break;
  }
};

// Name box and rename support.

/**
 * Update the displayed current item file name.
 *
 * @private
 */
Gallery.prototype.updateFilename_ = function() {
  var fullName = this.getSingleSelectedItem().getFileName();

  this.context_.onNameChange(fullName);

  var displayName = ImageUtil.getFileNameFromFullName(fullName);
  this.filenameEdit_.value = displayName;
  this.filenameText_.textContent = displayName;
};

/**
 * Click event handler on filename edit box
 * @private
 */
Gallery.prototype.onFilenameClick_ = function() {
  // We can't rename files in readonly directory.
  if (this.context_.readonlyDirName)
    return;

  ImageUtil.setAttribute(this.filenameSpacer_, 'renaming', true);
  setTimeout(this.filenameEdit_.select.bind(this.filenameEdit_), 0);
  this.inactivityWatcher_.startActivity();
};

/**
 * Blur event handler on filename edit box
 * @private
 */
Gallery.prototype.onFilenameEditBlur_ = function() {
  if (this.filenameEdit_.value && this.filenameEdit_.value[0] == '.') {
    this.prompt_.show('file_hidden_name', 5000);
    this.filenameEdit_.focus();
    return;
  }

  var item = this.getSingleSelectedItem();
  var oldUrl = item.getUrl();

  var onFileExists = function() {
    this.prompt_.show('file_exists', 3000);
    this.filenameEdit_.value = name;
    this.onFilenameClick_();
  }.bind(this);

  var onSuccess = function() {
    this.slideMode_.updateSelectedUrl_(oldUrl, item.getUrl());
  }.bind(this);

  if (this.filenameEdit_.value) {
    this.getSingleSelectedItem().rename(this.context_.saveDirEntry,
        this.filenameEdit_.value, onSuccess, onFileExists);
  }

  ImageUtil.setAttribute(this.filenameSpacer_, 'renaming', false);
  this.checkActivity_();
};

/**
 * Keydown event handler on filename edit box
 * @private
 */
Gallery.prototype.onFilenameEditKeydown_ = function() {
  switch (event.keyCode) {
    case 27:  // Escape
      this.updateFilename_();
      this.filenameEdit_.blur();
      break;

    case 13:  // Enter
      this.filenameEdit_.blur();
      break;
  }
  event.stopPropagation();
};

/**
 * @return {Boolean} True if file renaming is currently in progress
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
  this.filenameEdit_.blur();
};

// Share button support.

/**
 * @return {boolean} True if the Share mode is active.
 * @private
 */
Gallery.prototype.isSharing_ = function() {
  return this.shareMode_.isActive();
};

/**
 * Share button handler.
 * @param {Event} event Event.
 * @private
 */
Gallery.prototype.onShare_ = function(event) {
  this.shareMode_.toggle(event);
  this.checkActivity_();
};

/**
 *
 * @param {ImageEditor} editor Editor.
 * @param {Element} container Container element.
 * @param {Element} toolbar Toolbar element.
 * @param {function} onClick Click handler.
 * @param {function(function())} actionCallback Function to execute the action.
 * @param {function(string):string} displayStringFunction String formatting
 *     function.
 * @constructor
 */
function ShareMode(editor, container, toolbar,
                   onClick, actionCallback, displayStringFunction) {
  ImageEditor.Mode.call(this, 'share');

  this.message_ = null;

  var button = util.createChild(toolbar, 'button share');
  button.textContent = displayStringFunction('share');
  button.addEventListener('click', onClick);
  this.bind(editor, button);

  this.actionCallback_ = actionCallback;

  this.menu_ = util.createChild(container, 'share-menu');
  this.menu_.hidden = true;

  util.createChild(this.menu_, 'bubble-point');
}

ShareMode.prototype = { __proto__: ImageEditor.Mode.prototype };

/**
 * Shows share mode UI.
 */
ShareMode.prototype.setUp = function() {
  ImageEditor.Mode.prototype.setUp.apply(this, arguments);
  this.menu_.hidden = false;
  ImageUtil.setAttribute(this.button_, 'pressed', false);
};

/**
 * Hides share mode UI.
 */
ShareMode.prototype.cleanUpUI = function() {
  ImageEditor.Mode.prototype.cleanUpUI.apply(this, arguments);
  this.menu_.hidden = true;
};

/**
 * @return {boolean} True if the menu is currently open.
 */
ShareMode.prototype.isActive = function() {
  return !this.menu_.hidden;
};

/**
 * Show/hide the menu.
 * @param {Event} event Event.
 */
ShareMode.prototype.toggle = function(event) {
  this.editor_.enterMode(this, event);
};

/**
 * Update available actions list based on the currently selected urls.
 *
 * @param {Array.<string>} urls Array of urls.
 */
ShareMode.prototype.updateMenu = function(urls) {
  var internalId = util.getExtensionId();
  function isShareAction(task) {
    var task_parts = task.taskId.split('|');
    return task_parts[0] != internalId;
  }

  var items = this.menu_.querySelectorAll('.item');
  for (var i = 0; i != items.length; i++) {
    items[i].parentNode.removeChild(items[i]);
  }

  var api = Gallery.getFileBrowserPrivate();
  var mimeTypes = [];  // TODO(kaznacheev) Collect mime types properly.
  api.getFileTasks(urls, mimeTypes, function(tasks) {
    for (var i = 0; i != tasks.length; i++) {
      var task = tasks[i];
      if (!isShareAction(task)) continue;

      var item = document.createElement('div');
      item.className = 'item';
      this.menu_.appendChild(item);

      item.textContent = task.title;
      item.style.backgroundImage = 'url(' + task.iconUrl + ')';
      item.addEventListener('click', this.actionCallback_.bind(null,
          api.executeTask.bind(api, task.taskId, urls)));
    }

    if (this.menu_.firstChild)
      this.button_.removeAttribute('disabled');
    else
      this.button_.setAttribute('disabled', 'true');
  }.bind(this));
};

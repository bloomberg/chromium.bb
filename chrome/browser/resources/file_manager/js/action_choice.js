// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

document.addEventListener('DOMContentLoaded', function() {
  ActionChoice.load();
});

/**
 * The main ActionChoice object.
 *
 * @param {HTMLElement} dom Container.
 * @param {FileSystem} filesystem Local file system.
 * @param {Object} params Parameters.
 * @constructor
 */
function ActionChoice(dom, filesystem, params) {
  this.dom_ = dom;
  this.filesystem_ = filesystem;
  this.params_ = params;
  this.document_ = this.dom_.ownerDocument;
  this.metadataCache_ = this.params_.metadataCache;
  this.volumeManager_ = VolumeManager.getInstance();
  this.volumeManager_.addEventListener('externally-unmounted',
     this.onDeviceUnmounted_.bind(this));
  this.initDom_();

  // Load defined actions and remembered choice, then initialize volumes.
  this.actions_ = [];
  this.actionsById_ = {};
  this.rememberedChoice_ = null;

  ActionChoiceUtil.getDefinedActions(loadTimeData, function(actions) {
    for (var i = 0; i < actions.length; i++) {
      this.registerAction_(actions[i]);
    }

    this.viewFilesAction_ = this.actionsById_['view-files'];
    this.importPhotosToDriveAction_ =
        this.actionsById_['import-photos-to-drive'];
    this.watchSingleVideoAction_ =
        this.actionsById_['watch-single-video'];

    // Special case: if Google+ Photos is installed, then do not show Drive.
    for (var i = 0; i < actions.length; i++) {
      if (actions[i].extensionId == ActionChoice.GPLUS_PHOTOS_EXTENSION_ID) {
        this.importPhotosToDriveAction_.hidden = true;
        break;
      }
    }

    if (this.params_.advancedMode) {
      // In the advanced mode, skip auto-choice.
      this.initializeVolumes_();
    } else {
      // Get the remembered action before initializing volumes.
      ActionChoiceUtil.getRememberedActionId(function(actionId) {
        this.rememberedChoice_ = actionId;
        this.initializeVolumes_();
      }.bind(this));
    }
    this.renderList_();
  }.bind(this));

  // Try to render, what is already available.
  this.renderList_();
}

ActionChoice.prototype = { __proto__: cr.EventTarget.prototype };

/**
 * The number of previews shown.
 * @type {number}
 * @const
 */
ActionChoice.PREVIEW_COUNT = 3;

/**
 * Extension id of Google+ Photos app.
 * @type {string}
 * @const
 */
ActionChoice.GPLUS_PHOTOS_EXTENSION_ID = 'efjnaogkjbogokcnohkmnjdojkikgobo';

/**
 * Loads app in the document body.
 * @param {FileSystem=} opt_filesystem Local file system.
 * @param {Object=} opt_params Parameters.
 */
ActionChoice.load = function(opt_filesystem, opt_params) {
  ImageUtil.metrics = metrics;

  var hash = location.hash ? decodeURIComponent(location.hash.substr(1)) : '';
  var query =
      location.search ? decodeURIComponent(location.search.substr(1)) : '';
  var params = opt_params || {};
  if (!params.source) params.source = hash;
  if (!params.advancedMode) params.advancedMode = (query == 'advanced-mode');
  if (!params.metadataCache) params.metadataCache = MetadataCache.createFull();

  var onFilesystem = function(filesystem) {
    var dom = document.querySelector('.action-choice');
    ActionChoice.instance = new ActionChoice(dom, filesystem, params);
  };

  chrome.fileBrowserPrivate.getStrings(function(strings) {
    loadTimeData.data = strings;
    i18nTemplate.process(document, loadTimeData);
    if (opt_filesystem) {
      onFilesystem(opt_filesystem);
    } else {
      chrome.fileBrowserPrivate.requestLocalFileSystem(onFilesystem);
    }
  });
};

/**
 * Registers an action.
 * @param {Object} action Action item.
 * @private
 */
ActionChoice.prototype.registerAction_ = function(action) {
  this.actions_.push(action);
  this.actionsById_[action.id] = action;
};

/**
 * Initializes the source and Drive. If the remembered choice is available,
 * then performs the action.
 * @private
 */
ActionChoice.prototype.initializeVolumes_ = function() {
  var checkDriveFinished = false;
  var loadSourceFinished = false;

  var maybeRunRememberedAction = function() {
    if (!checkDriveFinished || !loadSourceFinished)
      return;

    // Run the remembered action if it is available.
    if (this.rememberedChoice_) {
      var action = this.actionsById_[this.rememberedChoice_];
      if (action && !action.disabled)
        this.runAction_(action);
    }
  }.bind(this);

  var onCheckDriveFinished = function() {
    checkDriveFinished = true;
    maybeRunRememberedAction();
  };

  var onLoadSourceFinished = function() {
    loadSourceFinished = true;
    maybeRunRememberedAction();
  };

  this.checkDrive_(onCheckDriveFinished);
  this.loadSource_(this.params_.source, onLoadSourceFinished);
};

/**
 * One-time initialization of dom elements.
 * @private
 */
ActionChoice.prototype.initDom_ = function() {
  this.list_ = new cr.ui.List();
  this.list_.id = 'actions-list';
  this.document_.querySelector('.choices').appendChild(this.list_);

  var self = this;  // .bind(this) doesn't work on constructors.
  this.list_.itemConstructor = function(item) {
    return self.renderItem(item);
  };

  this.list_.selectionModel = new cr.ui.ListSingleSelectionModel();
  this.list_.dataModel = new cr.ui.ArrayDataModel([]);
  this.list_.autoExpands = true;

  var acceptActionBound = function() {
    this.acceptAction_();
  }.bind(this);
  this.list_.activateItemAtIndex = acceptActionBound;
  this.list_.addEventListener('click', acceptActionBound);

  this.previews_ = this.document_.querySelector('.previews');
  this.counter_ = this.document_.querySelector('.counter');
  this.document_.addEventListener('keydown', this.onKeyDown_.bind(this));

  metrics.startInterval('PhotoImport.Load');
  this.dom_.setAttribute('loading', '');

  util.disableBrowserShortcutKeys(this.document_);
};

/**
 * Renders the list.
 * @private
 */
ActionChoice.prototype.renderList_ = function() {
  var currentItem = this.list_.dataModel.item(
      this.list_.selectionModel.selectedIndex);

  this.list_.startBatchUpdates();
  this.list_.dataModel.splice(0, this.list_.dataModel.length);

  for (var i = 0; i < this.actions_.length; i++) {
    if (!this.actions_[i].hidden)
      this.list_.dataModel.push(this.actions_[i]);
  }

  for (var i = 0; i < this.list_.dataModel.length; i++) {
    if (this.list_.dataModel.item(i) == currentItem) {
      this.list_.selectionModel.selectedIndex = i;
      break;
    }
  }

  this.list_.endBatchUpdates();
};

/**
 * Renders an item in the list.
 * @param {Object} item Item to render.
 * @return {Element} DOM element with representing the item.
 */
ActionChoice.prototype.renderItem = function(item) {
  var result = this.document_.createElement('li');

  var div = this.document_.createElement('div');
  if (item.disabled && item.disabledTitle)
    div.textContent = item.disabledTitle;
  else
    div.textContent = item.title;

  if (item.class)
    div.classList.add(item.class);
  if (item.icon100 && item.icon200)
    div.style.backgroundImage = '-webkit-image-set(' +
        'url(' + item.icon100 + ') 1x,' +
        'url(' + item.icon200 + ') 2x)';
  if (item.disabled)
    div.classList.add('disabled');

  cr.defineProperty(result, 'lead', cr.PropertyKind.BOOL_ATTR);
  cr.defineProperty(result, 'selected', cr.PropertyKind.BOOL_ATTR);
  result.appendChild(div);

  return result;
};

/**
 * Checks whether Drive is reachable.
 *
 * @param {function()} callback Completion callback.
 * @private
 */
ActionChoice.prototype.checkDrive_ = function(callback) {
  var onMounted = function() {
    this.importPhotosToDriveAction_.disabled = false;
    this.renderList_();
    callback();
  }.bind(this);

  if (this.volumeManager_.isMounted(RootDirectory.DRIVE)) {
    onMounted();
  } else {
    this.volumeManager_.mountDrive(onMounted, callback);
  }
};

/**
 * Load the source contents.
 *
 * @param {string} source Path to source.
 * @param {function()} callback Completion callback.
 * @private
 */
ActionChoice.prototype.loadSource_ = function(source, callback) {
  var onTraversed = function(results) {
    metrics.recordInterval('PhotoImport.Scan');
    var videos = results.filter(FileType.isVideo);
    if (videos.length == 1) {
      this.singleVideo_ = videos[0];
      this.watchSingleVideoAction_.title = loadTimeData.getStringF(
          'ACTION_CHOICE_WATCH_SINGLE_VIDEO', videos[0].name);
      this.watchSingleVideoAction_.hidden = false;
      this.watchSingleVideoAction_.disabled = false;
      this.renderList_();
    }

    var mediaFiles = results.filter(FileType.isImageOrVideo);
    if (mediaFiles.length == 0) {
      // If we have no media files, the only choice is view files. So, don't
      // confuse user with a single choice, and just open file manager.
      this.viewFiles_();
      this.recordAction_('view-files-auto');
      this.close_();
    }

    if (mediaFiles.length < ActionChoice.PREVIEW_COUNT) {
      this.counter_.textContent = loadTimeData.getStringF(
          'ACTION_CHOICE_COUNTER_NO_MEDIA', results.length);
    } else {
      this.counter_.textContent = loadTimeData.getStringF(
          'ACTION_CHOICE_COUNTER', mediaFiles.length);
    }
    var previews = mediaFiles.length ? mediaFiles : results;
    var previewsCount = Math.min(ActionChoice.PREVIEW_COUNT, previews.length);
    this.renderPreview_(previews, previewsCount);
    callback();
  }.bind(this);

  var onEntry = function(entry) {
    this.sourceEntry_ = entry;
    this.document_.querySelector('title').textContent = entry.name;

    var deviceType = this.volumeManager_.getDeviceType(entry.fullPath);
    if (deviceType != 'sd') deviceType = 'usb';
    this.dom_.querySelector('.device-type').setAttribute('device-type',
        deviceType);
    this.dom_.querySelector('.loading-text').textContent =
        loadTimeData.getString('ACTION_CHOICE_LOADING_' +
                               deviceType.toUpperCase());

    util.traverseTree(entry, onTraversed, 0 /* infinite depth */,
        FileType.isVisible);
  }.bind(this);

  var onReady = function() {
    util.resolvePath(this.filesystem_.root, source, onEntry, function() {
      this.recordAction_('error');
      this.close_();
    }.bind(this));
  }.bind(this);

  this.sourceEntry_ = null;
  metrics.startInterval('PhotoImport.Scan');
  if (!this.volumeManager_.isReady())
    this.volumeManager_.addEventListener('ready', onReady);
  else
    onReady();
};

/**
 * Renders a preview for a media entry.
 * @param {Array.<FileEntry>} entries The entries.
 * @param {number} count Remaining count.
 * @private
 */
ActionChoice.prototype.renderPreview_ = function(entries, count) {
  var entry = entries.shift();
  var box = this.document_.createElement('div');
  box.className = 'img-container';

  var done = function() {
    this.dom_.removeAttribute('loading');
    metrics.recordInterval('PhotoImport.Load');
  }.bind(this);

  var onSuccess = function() {
    this.previews_.appendChild(box);
    if (--count == 0) {
      done();
    } else {
      this.renderPreview_(entries, count);
    }
  }.bind(this);

  var onError = function() {
    if (entries.length == 0) {
      // Append one image with generic thumbnail.
      this.previews_.appendChild(box);
      done();
    } else {
      this.renderPreview_(entries, count);
    }
  }.bind(this);

  this.metadataCache_.get(entry, 'thumbnail|filesystem',
      function(metadata) {
        new ThumbnailLoader(entry.toURL(),
                            ThumbnailLoader.LoaderType.IMAGE,
                            metadata).load(
            box,
            ThumbnailLoader.FillMode.FILL,
            ThumbnailLoader.OptimizationMode.NEVER_DISCARD,
            onSuccess,
            onError,
            onError);
      });
};

/**
 * Closes the window.
 * @private
 */
ActionChoice.prototype.close_ = function() {
  window.close();
};

/**
 * Keydown event handler.
 * @param {Event} e The event.
 * @private
 */
ActionChoice.prototype.onKeyDown_ = function(e) {
  switch (util.getKeyModifiers(e) + e.keyCode) {
    case '13':
      this.acceptAction_();
      break;
    case '27':
      this.recordAction_('close');
      this.close_();
      break;
  }
};

/**
 * Runs an action.
 * @param {Object} action Action item to perform.
 * @private
 */
ActionChoice.prototype.runAction_ = function(action) {
  // TODO(mtomasz): Remove these predefined actions in Apps v2.
  if (action == this.importPhotosToDriveAction_) {
    var url = util.platform.getURL('photo_import.html') +
        '#' + this.sourceEntry_.fullPath;
    var width = 728;
    var height = 656;
    var top = Math.round((window.screen.availHeight - height) / 2);
    var left = Math.round((window.screen.availWidth - width) / 2);
    util.platform.createWindow(url,
        {height: height, width: width, left: left, top: top});
    this.recordAction_('import-photos-to-drive');
    this.close_();
    return;
  }

  if (action == this.watchSingleVideoAction_) {
    chrome.fileBrowserPrivate.viewFiles([this.singleVideo_.toURL()], 'watch',
        function(success) {});
    this.recordAction_('watch-single-video');
    this.close_();
    return;
  }

  if (action == this.viewFilesAction_) {
    this.viewFiles_();
    this.recordAction_('view-files');
    this.close_();
    return;
  }

  if (!action.extensionId) {
    console.error('Unknown predefined action.');
    return;
  }

  // Run the media galleries handler.
  chrome.mediaGalleriesPrivate.launchHandler(action.extensionId,
                                             action.actionId,
                                             this.params_.source);
  this.close_();
};

/**
 * Handles accepting an action. Checks if the action is available, remembers
 * and runs it.
 * @private
 */
ActionChoice.prototype.acceptAction_ = function() {
  var action =
      this.list_.dataModel.item(this.list_.selectionModel.selectedIndex);
  if (!action || action.hidden || action.disabled)
    return;

  this.runAction_(action);
  ActionChoiceUtil.setRememberedActionId(action.id);
};

/**
 * Called when some device is unmounted.
 * @param {Event} event Event object.
 * @private
 */
ActionChoice.prototype.onDeviceUnmounted_ = function(event) {
  if (this.sourceEntry_ && event.mountPath == this.sourceEntry_.fullPath) {
    util.platform.closeWindow();
  }
};

/**
 * Perform the 'view files' action.
 * @private
 */
ActionChoice.prototype.viewFiles_ = function() {
  var path = this.sourceEntry_.fullPath;
  if (util.platform.v2()) {
    chrome.runtime.getBackgroundPage(function(bg) {
      bg.launchFileManager({defaultPath: path});
    });
  } else {
    var url = util.platform.getURL('main.html') + '#' + path;
    chrome.fileBrowserPrivate.openNewWindow(url);
  }
};

/**
 * Records an action chosen.
 * @param {string} action Action name.
 * @private
 */
ActionChoice.prototype.recordAction_ = function(action) {
  metrics.recordEnum('PhotoImport.Action', action,
      ['import-photos-to-drive',
       'view-files',
       'view-files-auto',
       'watch-single-video',
       'error',
       'close']);
};

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

  this.rememberedChoice_ = null;
  this.enabledOptions_ = [ActionChoice.Action.VIEW_FILES];
  util.storage.local.get(['action-choice'], function(result) {
    this.rememberedChoice_ = result['action-choice'];
    this.initializeVolumes_();
  }.bind(this));

  this.viewFilesItem_ = {
    title: loadTimeData.getString('ACTION_CHOICE_VIEW_FILES'),
    name: 'Files.app',
    class: 'view-files'
  };

  this.importPhotosToDriveItem_ = {
    title: loadTimeData.getString('ACTION_CHOICE_DRIVE_NOT_REACHED'),
    name: 'Files.app',
    class: 'import-photos-to-drive',
    disabled: true
  };

  this.watchSingleVideoItem_ = {
    title: '',
    name: 'Files.app',
    class: 'play-video',
    hidden: true
  };

  this.staticItems_ = [
    this.viewFilesItem_,
    this.importPhotosToDriveItem_,
    this.watchSingleVideoItem_
  ];

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
 * Action to perform depending on an user's choice.
 * @enum {string}
 * @const
 */
ActionChoice.Action = {
  VIEW_FILES: 'view-files',
  IMPORT_PHOTOS_TO_DRIVE: 'import-photos-to-drive',
  WATCH_SINGLE_VIDEO: 'watch-single-video',
  OPEN_IN_GPLUS_PHOTOS: 'open-in-gplus-photos'
};

/**
 * Loads app in the document body.
 * @param {FileSystem=} opt_filesystem Local file system.
 * @param {Object=} opt_params Parameters.
 */
ActionChoice.load = function(opt_filesystem, opt_params) {
  ImageUtil.metrics = metrics;

  var hash = location.hash ? decodeURI(location.hash.substr(1)) : '';
  var params = opt_params || {};
  if (!params.source) params.source = hash;
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
    if (this.rememberedChoice_ &&
        this.enabledOptions_.indexOf(this.rememberedChoice_) != -1) {
      this.runAction_(this.rememberedChoice_);
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

  // Add predefined items.
  for (var i = 0; i < this.staticItems_.length; i++) {
    if (!this.staticItems_[i].hidden)
      this.list_.dataModel.push(this.staticItems_[i]);
  }

  // Add dynamic Media Galleries handlers.
  // TODO(mtomasz): Implement this.

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
  div.textContent = item.title;
  if (item.class)
    div.classList.add(item.class);
  if (item.icon)
    div.style.backgroundImage = 'url(' + item.icon + ')';
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
    this.importPhotosToDriveItem_.disabled = false;
    this.importPhotosToDriveItem_.title =
        loadTimeData.getString('ACTION_CHOICE_PHOTOS_DRIVE');
    this.renderList_();
    this.enabledOptions_.push(ActionChoice.Action.IMPORT_PHOTOS_TO_DRIVE);
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
      this.enabledOptions_.push(ActionChoice.Action.WATCH_SINGLE_VIDEO);
      this.watchSingleVideoItem_.title = loadTimeData.getStringF(
          'ACTION_CHOICE_WATCH_SINGLE_VIDEO', videos[0].name);
      this.watchSingleVideoItem_.hidden = false;
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
 * @param {ActionChoice.Action} action Action to perform.
 * @private
 */
ActionChoice.prototype.runAction_ = function(action) {
  switch (action) {
    case ActionChoice.Action.IMPORT_PHOTOS_TO_DRIVE:
      var url = util.platform.getURL('photo_import.html') +
          '#' + this.sourceEntry_.fullPath;
      var width = 728;
      var height = 656;
      var top = Math.round((window.screen.availHeight - height) / 2);
      var left = Math.round((window.screen.availWidth - width) / 2);
      util.platform.createWindow(url,
          {height: height, width: width, left: left, top: top});
      this.recordAction_('import-photos-to-drive');
      break;
    case ActionChoice.Action.WATCH_SINGLE_VIDEO:
      chrome.fileBrowserPrivate.viewFiles([this.singleVideo_.toURL()], 'watch',
          function(success) {});
      this.recordAction_('watch-single-video');
      break;
    case ActionChoice.Action.OPEN_IN_GPLUS_PHOTOS:
      // TODO(mtomasz): Not yet implemented.
      break;
    case ActionChoice.Action.VIEW_FILES:
    default:
      this.viewFiles_();
      this.recordAction_('view-files');
      break;
  }
  this.close_();
};

/**
 * Runs importing on Files.app depending on choice and remembers the choice.
 * @private
 */
ActionChoice.prototype.acceptAction_ = function() {
  var item = this.list_.dataModel.item(this.list_.selectionModel.selectedIndex);
  if (!item || item.disabled)
    return;

  if (item == this.importPhotosToDriveItem_) {
    this.runAction_(ActionChoice.Action.IMPORT_PHOTOS_TO_DRIVE);
    util.storage.local.set(
        {'action-choice': ActionChoice.Action.IMPORT_PHOTOS_TO_DRIVE});
    return;
  }

  if (item == this.viewFilesItem_) {
    this.runAction_(ActionChoice.Action.VIEW_FILES);
    util.storage.local.set(
        {'action-choice': ActionChoice.Action.VIEW_FILES});
    return;
  }

  if (item == this.playVideoItem_) {
    this.runAction_(ActionChoice.Action.WATCH_SINGLE_VIDEO);
    util.storage.local.set(
        {'action-choice': ActionChoice.Action.WATCH_SINGLE_VIDEO});
  }
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

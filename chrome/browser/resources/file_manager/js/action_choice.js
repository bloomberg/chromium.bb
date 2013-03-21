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
  IMPORT_TO_DRIVE: 'import-todrive',
  PLAY_VIDEO: 'play-video',
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
  this.previews_ = this.document_.querySelector('.previews');
  this.counter_ = this.document_.querySelector('.counter');

  this.document_.querySelector('button.always').addEventListener('click',
      this.acceptChoice_.bind(this, true));  // Remember the choice.
  this.document_.querySelector('button.once').addEventListener('click',
      this.acceptChoice_.bind(this, false));  // Do not remember the choice.

  var choices = this.document_.querySelectorAll('.choices div');
  for (var index = 0; index < choices.length; index++) {
    choices[index].addEventListener(
        'dblclick',
        this.acceptChoice_.bind(this, false));  // Do not remember the choice.
  }

  this.document_.addEventListener('keydown', this.onKeyDown_.bind(this));

  metrics.startInterval('PhotoImport.Load');
  this.dom_.setAttribute('loading', '');

  this.document_.querySelectorAll('.choices input')[0].focus();

  util.disableBrowserShortcutKeys(this.document_);
};

/**
 * Checks whether Drive is reachable.
 *
 * @param {function()} callback Completion callback.
 * @private
 */
ActionChoice.prototype.checkDrive_ = function(callback) {
  var driveLabel = this.dom_.querySelector('label[for=import-photos-to-drive]');
  var driveChoice = this.dom_.querySelector('#import-photos-to-drive');
  var driveDiv = driveChoice.parentNode;
  driveChoice.disabled = true;
  driveDiv.setAttribute('disabled', '');

  var onMounted = function() {
    driveChoice.disabled = false;
    this.enabledOptions_.push(ActionChoice.Action.IMPORT_TO_DRIVE);
    driveDiv.removeAttribute('disabled');
    driveLabel.textContent =
        loadTimeData.getString('ACTION_CHOICE_PHOTOS_DRIVE');
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
    var videoLabel = this.dom_.querySelector('label[for=watch-single-video]');
    if (videos.length == 1) {
      videoLabel.textContent = loadTimeData.getStringF(
          'ACTION_CHOICE_WATCH_SINGLE_VIDEO', videos[0].name);
      this.singleVideo_ = videos[0];
      this.enabledOptions_.push(ActionChoice.Action.PLAY_VIDEO);
    } else {
      videoLabel.parentNode.style.display = 'none';
    }

    var mediaFiles = results.filter(FileType.isImageOrVideo);
    if (mediaFiles.length == 0) {
      this.dom_.querySelector('#import-photos-to-drive').parentNode.
          style.display = 'none';

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
      this.acceptChoice_(false);  // Do not remember the choice.
      return;
    case '27':
      this.recordAction_('close');
      this.close_();
      return;
  }
};

/**
 * Runs an action.
 * @param {ActionChoice.Action} action Action to perform.
 * @private
 */
ActionChoice.prototype.runAction_ = function(action) {
  switch (action) {
    case ActionChoice.Action.IMPORT_TO_DRIVE:
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
    case ActionChoice.Action.PLAY_VIDEO:
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
 * Runs importing on Files.app depending on choice.
 * @param {boolean} remember Whether remember the choice or not.
 * @private
 */
ActionChoice.prototype.acceptChoice_ = function(remember) {
  if (this.document_.querySelector('#import-photos-to-drive').checked &&
      this.enabledOptions_.indexOf(ActionChoice.Action.IMPORT_TO_DRIVE) != -1) {
    this.runAction_(ActionChoice.Action.IMPORT_TO_DRIVE);
    util.storage.local.set({'action-choice': remember ?
        ActionChoice.Action.IMPORT_TO_DRIVE : undefined});
    return;
  }

  if (this.document_.querySelector('#view-files').checked &&
      this.enabledOptions_.indexOf(ActionChoice.Action.VIEW_FILES) != -1) {
    this.runAction_(ActionChoice.Action.VIEW_FILES);
    util.storage.local.set({'action-choice': remember ?
        ActionChoice.Action.VIEW_FILES : undefined});
    return;
  }

  if (this.document_.querySelector('#watch-single-video').checked &&
      this.enabledOptions_.indexOf(ActionChoice.Action.PLAY_VIDEO) != -1) {
    this.runAction_(ActionChoice.Action.PLAY_VIDEO);
    util.storage.local.set({'action-choice': remember ?
        ActionChoice.Action.PLAY_VIDEO : undefined});
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

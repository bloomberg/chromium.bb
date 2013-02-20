// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

document.addEventListener('DOMContentLoaded', function() {
  ActionChoice.load();
});

/**
 * The main ActionChoice object.
 * @param {HTMLElement} dom Container.
 * @param {FileSystem} filesystem Local file system.
 * @param {Object} params Parameters.
 * @constructor
 */
function ActionChoice(dom, filesystem, params) {
  this.filesystem_ = filesystem;
  this.dom_ = dom;
  this.document_ = this.dom_.ownerDocument;
  this.metadataCache_ = params.metadataCache;
  this.volumeManager_ = new VolumeManager();
  this.volumeManager_.addEventListener('externally-unmounted',
     this.onDeviceUnmounted_.bind(this));

  this.initDom_();
  this.checkDrive_();
  this.loadSource_(params.source);
}

ActionChoice.prototype = { __proto__: cr.EventTarget.prototype };

/**
 * The number of previews shown.
 */
ActionChoice.PREVIEW_COUNT = 3;

/**
 * Loads app in the document body.
 * @param {FileSystem=} opt_filesystem Local file system.
 * @param {Object} opt_params Parameters.
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
 * One-time initialization of dom elements.
 * @private
 */
ActionChoice.prototype.initDom_ = function() {
  this.previews_ = this.document_.querySelector('.previews');
  this.counter_ = this.document_.querySelector('.counter');

  this.document_.querySelector('button.ok').addEventListener('click',
      this.onOk_.bind(this));

  var choices = this.document_.querySelectorAll('.choices div');
  for (var index = 0; index < choices.length; index++) {
    choices[index].addEventListener('dblclick', this.onOk_.bind(this));
  }

  this.document_.addEventListener('keydown', this.onKeyDown_.bind(this));

  metrics.startInterval('PhotoImport.Load');
  this.dom_.setAttribute('loading', '');

  this.document_.querySelectorAll('.choices input')[0].focus();
};

/**
 * Checks whether Drive is reachable.
 * @private
 */
ActionChoice.prototype.checkDrive_ = function() {
  var driveLabel = this.dom_.querySelector('label[for=import-photos-to-drive]');
  var driveChoice = this.dom_.querySelector('#import-photos-to-drive');
  var driveDiv = driveChoice.parentNode;
  driveChoice.disabled = true;
  driveDiv.setAttribute('disabled', '');

  var onMounted = function() {
    driveChoice.disabled = false;
    driveDiv.removeAttribute('disabled');
    driveLabel.textContent =
        loadTimeData.getString('ACTION_CHOICE_PHOTOS_DRIVE');
  };

  if (this.volumeManager_.isMounted(RootDirectory.DRIVE)) {
    onMounted();
  } else {
    this.volumeManager_.mountDrive(onMounted, function() {});
  }
};

/**
 * Load the source contents.
 * @param {string} source Path to source.
 * @private
 */
ActionChoice.prototype.loadSource_ = function(source) {
  var onTraversed = function(results) {
    metrics.recordInterval('PhotoImport.Scan');
    var videos = results.filter(FileType.isVideo);
    var videoLabel = this.dom_.querySelector('label[for=watch-single-video]');
    if (videos.length == 1) {
      videoLabel.textContent = loadTimeData.getStringF(
          'ACTION_CHOICE_WATCH_SINGLE_VIDEO', videos[0].name);
      this.singleVideo_ = videos[0];
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

  this.sourceEntry_ = null;
  metrics.startInterval('PhotoImport.Scan');
  util.resolvePath(this.filesystem_.root, source, onEntry, function() {
    this.recordAction_('error');
    this.close_();
  }.bind(this));
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
            box, ThumbnailLoader.FillMode.FILL, onSuccess, onError, onError);
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
      this.onOk_();
      return;
    case '27':
      this.recordAction_('close');
      this.close_();
      return;
  }
};

/**
 * Called when OK button clicked.
 * @param {Event} event The event object.
 * @private
 */
ActionChoice.prototype.onOk_ = function(event) {
  // Check for click on the disabled choice.
  var input = event.currentTarget.querySelector('input');
  if (input && input.disabled) return;

  if (this.document_.querySelector('#import-photos-to-drive').checked) {
    var url = util.platform.getURL('photo_import.html') +
        '#' + this.sourceEntry_.fullPath;
    var width = 728;
    var height = 656;
    var top = Math.round((window.screen.availHeight - height) / 2);
    var left = Math.round((window.screen.availWidth - width) / 2);
    util.platform.createWindow(url,
        {height: height, width: width, left: left, top: top});
    this.recordAction_('import-photos-to-drive');
  } else if (this.document_.querySelector('#view-files').checked) {
    this.viewFiles_();
    this.recordAction_('view-files');
  } else if (this.document_.querySelector('#watch-single-video').checked) {
    chrome.fileBrowserPrivate.viewFiles([this.singleVideo_.toURL()], 'watch',
        function(success) {});
    this.recordAction_('watch-single-video');
  }
  this.close_();
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
    util.platform.createWindow(url);
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

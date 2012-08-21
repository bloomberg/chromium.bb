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
  this.closeBound_ = this.close_.bind(this);

  this.initDom_();
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

  var hash = location.hash ? location.hash.substr(1) : '';
  var params = opt_params || {};
  if (!params.source) params.source = hash;
  if (!params.metadataCache) params.metadataCache = MetadataCache.createFull();

  function onFilesystem(filesystem) {
    var dom = document.querySelector('.action-choice');
    new ActionChoice(dom, filesystem, params);
  }

  chrome.fileBrowserPrivate.getStrings(function(strings) {
    loadTimeData.data = strings;

    // TODO(dgozman): remove when all strings finalized.
    var original = loadTimeData.getString;
    loadTimeData.getString = function(s) {
      return original.call(loadTimeData, s) || s;
    };
    var originalF = loadTimeData.getStringF;
    loadTimeData.getStringF = function() {
      return originalF.apply(loadTimeData, arguments) || arguments[0];
    };

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
  this.title_ = this.document_.querySelector('.title-bar');
  this.previews_ = this.document_.querySelector('.previews');
  this.counter_ = this.document_.querySelector('.counter');

  this.document_.querySelector('button.ok').addEventListener('click',
      this.onOk_.bind(this));

  var choices = this.document_.querySelectorAll('.choices div');
  for (var index = 0; index < choices.length; index++) {
    choices[index].addEventListener('dblclick', this.onOk_.bind(this));
  }

  this.document_.addEventListener('keydown', this.onKeyDown_.bind(this));

  this.previews_.setAttribute('loading', '');

  this.document_.querySelectorAll('.choices input')[0].focus();
};

/**
 * Load the source contents.
 * @param {string} source Path to source.
 * @private
 */
ActionChoice.prototype.loadSource_ = function(source) {
  var onTraversed = function(results) {
    this.previews_.removeAttribute('loading');
    var mediaFiles = results.filter(FileType.isImageOrVideo);

    if (mediaFiles.length < ActionChoice.PREVIEW_COUNT) {
      this.counter_.textContent = loadTimeData.getStringF(
          'ACTION_CHOICE_COUNTER_NO_MEDIA', results.length);
    } else {
      this.counter_.textContent = loadTimeData.getStringF(
          'ACTION_CHOICE_COUNTER', results.length, mediaFiles.length);
      results = mediaFiles;
    }

    var count = Math.min(results.length, ActionChoice.PREVIEW_COUNT);
    for (var index = 0; index < count; index++) {
      this.renderPreview_(results[index]);
    }
  }.bind(this);

  var onEntry = function(entry) {
    this.sourceEntry_ = entry;
    // TODO(dgozman): add icon.
    this.title_.textContent = entry.name;
    util.traverseTree(entry, onTraversed, 0 /* infinite depth */);
  }.bind(this);

  this.sourceEntry_ = null;
  util.resolvePath(this.filesystem_.root, source, onEntry, this.closeBound_);
};

/**
 * Renders a preview for a media entry.
 * @param {FileEntry} entry The entry.
 * @private
 */
ActionChoice.prototype.renderPreview_ = function(entry) {
  var box = this.document_.createElement('div');
  box.className = 'img-container';
  this.metadataCache_.get(entry, 'thumbnail|filesystem',
      function(metadata) {
        new ThumbnailLoader(entry.toURL(), metadata).
            load(box, true /* fill, not fit */);
      });
  this.previews_.appendChild(box);
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
      this.close_();
      return;
  }
};

/**
 * Called when OK button clicked.
 * @private
 */
ActionChoice.prototype.onOk_ = function() {
  if (this.document_.querySelector('#import-photos-to-drive').checked) {
    var url = chrome.extension.getURL('photo_import.html') +
        '#' + this.sourceEntry_.fullPath;
    chrome.windows.create({url: url, type: 'popup'});
  } else if (this.document_.querySelector('#view-files').checked) {
    var url = chrome.extension.getURL('main.html') +
        '#' + this.sourceEntry_.fullPath;
    chrome.windows.create({url: url, type: 'popup'});
  }
  this.close_();
};

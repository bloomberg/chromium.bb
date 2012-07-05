// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

document.addEventListener('DOMContentLoaded', function() {
  PhotoImport.load();
});

/**
 * The main Photo App object.
 * @param {HTMLElement} dom Container.
 * @param {FileSystem} filesystem Local file system.
 * @param {Object} params Parameters.
 * @constructor
 */
function PhotoImport(dom, filesystem, params) {
  this.filesystem_ = filesystem;
  this.dom_ = dom;
  this.document_ = this.dom_.ownerDocument;
  this.metadataCache_ = params.metadataCache;

  this.initDom_();
  this.initAlbums_();
  this.loadSource_(params.source);
}

PhotoImport.prototype = { __proto__: cr.EventTarget.prototype };

/**
 * Loads app in the document body.
 * @param {FileSystem=} opt_filesystem Local file system.
 * @param {Object} opt_params Parameters.
 */
PhotoImport.load = function(opt_filesystem, opt_params) {
  ImageUtil.metrics = metrics;

  var hash = location.hash ? location.hash.substr(1) : '';
  var params = opt_params || {};
  if (!params.source) params.source = hash;
  if (!params.metadataCache) params.metadataCache = MetadataCache.createFull();

  function onFilesystem(filesystem) {
    var dom = document.querySelector('.photo-import');
    new PhotoImport(dom, filesystem, params);
  }

  var api = chrome.fileBrowserPrivate || window.top.chrome.fileBrowserPrivate;
  api.getStrings(function(strings) {
    loadTimeData.data = strings;

    // TODO: add strings.
    loadTimeData.getString = function(s) { return s; };

    if (opt_filesystem) {
      onFilesystem(opt_filesystem);
    } else {
      api.requestLocalFileSystem(onFilesystem);
    }
  });
};

/**
 * One-time initialization of dom elements.
 * @private
 */
PhotoImport.prototype.initDom_ = function() {
  this.dom_.setAttribute('loading', '');

  this.spinner_ = this.dom_.querySelector('.spinner');

  this.title_ = this.dom_.querySelector('.title');
  this.importButton_ = this.dom_.querySelector('button.import');
  this.importButton_.textContent =
      loadTimeData.getString('PHOTO_IMPORT_IMPORT_BUTTON');
  this.importButton_.addEventListener('click', this.onImportClick_.bind(this));

  // TODO(dgozman): add shades at top and bottom of the list.
  this.grid_ = this.dom_.querySelector('grid');
  cr.ui.Grid.decorate(this.grid_);
  this.grid_.itemConstructor =
      GridItem.bind(null, this);
  this.fileList_ = new cr.ui.ArrayDataModel([]);
  this.grid_.selectionModel = new cr.ui.ListSelectionModel();
  this.grid_.dataModel = this.fileList_;

  this.grid_.selectionModel.addEventListener('change',
      this.onSelectionChanged_.bind(this));
  this.onSelectionChanged_();

  this.selectAlbumDialog_ = new SelectAlbumDialog(this.dom_);
};

/**
 * One-time initialization of albums.
 * @private
 */
PhotoImport.prototype.initAlbums_ = function() {
  // TODO
};

/**
 * Load the source contents.
 * @param {string} source Path to source.
 * @private
 */
PhotoImport.prototype.loadSource_ = function(source) {
  var onTraversed = function(results) {
    this.dom_.removeAttribute('loading');
    var mediaFiles = results.filter(FileType.isImageOrVideo);
    this.fileList_.push.apply(this.fileList_, mediaFiles);
  }.bind(this);

  var onEntry = function(entry) {
    this.title_.textContent = entry.name;
    util.traverseTree(entry, onTraversed, 0 /* infinite depth */);
  }.bind(this);

  var onError = this.onError_.bind(
      this, loadTimeData.getString('PHOTO_IMPORT_SOURCE_ERROR'));

  util.resolvePath(this.filesystem_.root, source, onEntry, onError);
};

/**
 * Decorates grid item.
 * @param {HTMLLIElement} li The item.
 * @param {Entry} entry File entry.
 * @private
 */
PhotoImport.prototype.decorateGridItem_ = function(li, entry) {
  li.className = 'grid-item';

  var frame = this.document_.createElement('div');
  frame.className = 'grid-frame';
  li.appendChild(frame);

  var box = this.document_.createElement('div');
  box.className = 'img-container';
  this.metadataCache_.get(entry, 'thumbnail|filesystem',
      function(metadata) {
        new ThumbnailLoader(entry.toURL(), metadata).
            load(box, false /* fit, not fill*/);
      });
  frame.appendChild(box);
};

/**
 * Show error message.
 * @param {string} message Error message.
 * @private
 */
PhotoImport.prototype.onError_ = function(message) {
  // TODO
};

/**
 * Event handler for selection change.
 * @param {Event} event The event.
 * @private
 */
PhotoImport.prototype.onSelectionChanged_ = function(event) {
  this.importButton_.disabled =
      this.grid_.selectionModel.selectedIndexes.length == 0;
};

/**
 * Event handler for import button click.
 * @param {Event} event The event.
 * @private
 */
PhotoImport.prototype.onImportClick_ = function(event) {
  var indexes = this.grid_.selectionModel.selectedIndexes;

  // TODO: use albums instead.
  var albums = [
    {name: 'Album 1', url:
        chrome.extension.getURL('../../images/filetype_large_audio.png')},
    {name: 'Album 2', url:
        chrome.extension.getURL('../../images/filetype_large_video.png')},
    {name: 'Album 3', url:
        chrome.extension.getURL('../../images/filetype_large_image.png')}
  ];

  var onAlbumSelected = function(album) {
  }.bind(this);

  this.selectAlbumDialog_.show(
      indexes.length == 1 ?
          loadTimeData.getString('PHOTO_IMPORT_SELECT_ALBUM_CAPTION') :
          loadTimeData.getStringF('PHOTO_IMPORT_SELECT_ALBUM_CAPTION_PLURAL',
                                  indexes.length),
      albums,
      this.title_.textContent,
      loadTimeData.getString('PHOTO_IMPORT_IMPORT_BUTTON'),
      onAlbumSelected
  );
};

/**
 * Item in the grid.
 * @param {PhotoImport} app Application instance.
 * @param {Entry} entry File entry.
 * @constructor
 */
function GridItem(app, entry) {
  var li = app.document_.createElement('li');
  li.__proto__ = GridItem.prototype;
  app.decorateGridItem_(li, entry);
  return li;
}

GridItem.prototype = {
  __proto__: cr.ui.ListItem.prototype,
  get label() {},
  set label(value) {}
};

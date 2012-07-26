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
  this.volumeManager_ = new VolumeManager();
  this.copyManager_ = FileCopyManagerWrapper.getInstance(this.filesystem_.root);
  this.mediaFilesList_ = null;
  this.albums_ = null;
  this.albumsDir_ = null;

  this.initDom_();
  this.initAlbums_();
  this.loadSource_(params.source);
}

PhotoImport.prototype = { __proto__: cr.EventTarget.prototype };

/**
 * Single item width.
 * Keep in sync with .grid-item rule in photo_import.css.
 */
PhotoImport.ITEM_WIDTH = 164 + 8;

/**
 * Directory name on the GData containing the imported photos.
 * TODO(dgozman): localize
 */
PhotoImport.GDATA_PHOTOS_DIR = 'My Photos';

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

    // TODO(dgozman): remove when all strings finalized.
    var original = loadTimeData.getString;
    loadTimeData.getString = function(s) {
      return original.call(loadTimeData, s) || s;
    };
    var originalF = loadTimeData.getStringF;
    loadTimeData.getStringF = function() {
      return originalF.apply(loadTimeData, arguments) || arguments[0];
    };

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
  this.dom_.ownerDocument.defaultView.addEventListener(
      'resize', this.onResize_.bind(this));

  this.spinner_ = this.dom_.querySelector('.spinner');

  this.title_ = this.dom_.querySelector('.title');
  this.importButton_ = this.dom_.querySelector('button.import');
  this.importButton_.textContent =
      loadTimeData.getString('PHOTO_IMPORT_IMPORT_BUTTON');
  this.importButton_.addEventListener('click', this.onImportClick_.bind(this));

  // TODO(dgozman): add shades at top and bottom of the list.
  this.grid_ = this.dom_.querySelector('grid');
  cr.ui.Grid.decorate(this.grid_);
  this.grid_.redraw = cr.ui.List.prototype.redraw;
  this.grid_.createSelectionController = function(sm) {
    return new GridSelectionController(sm, this);
  };

  this.onResize_();  // To set columns number.
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
  var onError = this.onError_.bind(
      this, loadTimeData.getString('PHOTO_IMPORT_GDATA_ERROR'));

  var albums = [];

  var onEntry = function(entry) {
    if (entry != null) {
      albums.push(entry);
    } else {
      this.albums_ = albums;
    }
  }.bind(this);

  var onGData = function(gdata) {
    this.albumsDir_ = gdata;
    util.forEachDirEntry(gdata, onEntry);
  }.bind(this);

  var onMounted = function() {
    var dir = PathUtil.join(RootDirectory.GDATA, PhotoImport.GDATA_PHOTOS_DIR);
    util.resolvePath(this.filesystem_.root, dir, onGData, onError);
  }.bind(this);

  if (this.volumeManager_.isMounted(RootDirectory.GDATA)) {
    onMounted();
  } else {
    this.volumeManager_.mountGData(onMounted, onError);
  }
};

/**
 * Load the source contents.
 * @param {string} source Path to source.
 * @private
 */
PhotoImport.prototype.loadSource_ = function(source) {
  var onTraversed = function(results) {
    this.dom_.removeAttribute('loading');
    this.mediaFilesList_ = results.filter(FileType.isImageOrVideo);
    this.makeFileGroups_();
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
 * Divides files into groups by the modification date and pass them to the grid.
 * @private
 */
PhotoImport.prototype.makeFileGroups_ = function() {
  var files = this.mediaFilesList_;
  if (!files) return;

  var dateFormatter = v8Intl.DateTimeFormat(
      [] /* default locale */,
      {year: 'numeric', month: 'short', day: 'numeric'});

  var columns = this.grid_.columns;

  var onMetadata = function(filesystem) {
    var unknownGroup = {
      type: 'group',
      date: 0,
      title: loadTimeData.getString('PHOTO_IMPORT_UNKNOWN_DATE'),
      items: []
    };

    var groupsMap = {};

    for (var index = 0; index < files.length; index++) {
      var props = filesystem[index];
      var item = { type: 'entry', entry: files[index] };

      if (!props || !props.modificationTime) {
        item.group = unknownGroup;
        unknownGroup.items.push(item);
        continue;
      }

      var date = new Date(props.modificationTime);
      date.setHours(0);
      date.setMinutes(0);
      date.setSeconds(0);
      date.setMilliseconds(0);

      var time = date.getTime();
      if (!(time in groupsMap)) {
        groupsMap[time] = {
          type: 'group',
          date: date,
          title: dateFormatter.format(date),
          items: []
        };
      }

      var group = groupsMap[time];
      group.items.push(item);
      item.group = group;
    }

    var groups = [];
    for (var time in groupsMap) {
      if (groupsMap.hasOwnProperty(time)) {
        groups.push(groupsMap[time]);
      }
    }
    if (unknownGroup.items.length > 0)
      groups.push(unknownGroup);

    groups.sort(function(a, b) {
      return b.date.getTime() - a.date.getTime();
    });

    var list = [];
    for (var index = 0; index < groups.length; index++) {
      var group = groups[index];

      list.push(group);
      for (var t = 1; t < columns; t++) {
        list.push({ type: 'empty' });
      }

      for (var j = 0; j < group.items.length; j++) {
        list.push(group.items[j]);
      }

      var count = group.items.length;
      while (count % columns != 0) {
        list.push({ type: 'empty' });
        count++;
      }
    }

    this.fileList_.splice(0, this.fileList_.length);
    this.fileList_.push.apply(this.fileList_, list);
  }.bind(this);

  this.metadataCache_.get(files, 'filesystem', onMetadata);
};

/**
 * Decorates grid item.
 * @param {HTMLLIElement} li The list item.
 * @param {Object} item The model item.
 * @private
 */
PhotoImport.prototype.decorateGridItem_ = function(li, item) {
  li.className = 'grid-item';

  if (item.type == 'empty') {
    li.classList.add('empty');
    return;
  }

  if (item.type == 'group') {
    li.classList.add('group');
    var content = this.document_.createElement('div');
    content.textContent = item.title;
    li.appendChild(content);
    return;
  }

  var frame = this.document_.createElement('div');
  frame.className = 'grid-frame';
  li.appendChild(frame);

  var box = this.document_.createElement('div');
  box.className = 'img-container';
  this.metadataCache_.get(item.entry, 'thumbnail|filesystem',
      function(metadata) {
        new ThumbnailLoader(item.entry.toURL(), metadata).
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
 * Resize event handler.
 * @private
 */
PhotoImport.prototype.onResize_ = function() {
  var columns =
      Math.floor((this.dom_.clientWidth - 20) / PhotoImport.ITEM_WIDTH);
  if (columns != this.grid_.columns) {
    this.grid_.columns = columns;
    this.makeFileGroups_();
  }
};

/**
 * @return {Array.<Object>} The list of selected entries.
 * @private
 */
PhotoImport.prototype.getSelectedItems_ = function() {
  return this.grid_.selectedItems.filter(function(item) {
    return item.type == 'entry';
  });
};

/**
 * Event handler for selection change.
 * @param {Event} event The event.
 * @private
 */
PhotoImport.prototype.onSelectionChanged_ = function(event) {
  this.importButton_.disabled = this.getSelectedItems_().length == 0 ||
      this.albums_ == null;
};

/**
 * Event handler for import button click.
 * @param {Event} event The event.
 * @private
 */
PhotoImport.prototype.onImportClick_ = function(event) {
  var items = this.getSelectedItems_();

  var defaultTitle = loadTimeData.getString('PHOTO_IMPORT_NEW_ALBUM_NAME');
  var group = items[0].group;
  if (items.filter(function(i) { return i.group !== group }).length == 0)
    defaultTitle = group.title;

  // TODO: use albums instead.
  var dialogAlbums = [];
  for (var index = 0; index < this.albums_.length; index++) {
    dialogAlbums.push({
      name: this.albums_[index].name,
      entry: this.albums_[index],
      url: chrome.extension.getURL('../../images/filetype_large_audio.png')
    });
  }

  this.selectAlbumDialog_.show(
      items.length == 1 ?
          loadTimeData.getString('PHOTO_IMPORT_SELECT_ALBUM_CAPTION') :
          loadTimeData.getStringF('PHOTO_IMPORT_SELECT_ALBUM_CAPTION_PLURAL',
                                  items.length),
      dialogAlbums,
      defaultTitle,
      loadTimeData.getString('PHOTO_IMPORT_IMPORT_BUTTON'),
      this.onAlbumSelected_.bind(this, items)
  );
};

/**
 * Called when album is selected.
 * @param {Array.<Object>} items List of items to import.
 * @param {Object} album Album description.
 * @private
 */
PhotoImport.prototype.onAlbumSelected_ = function(items, album) {
  var entries = items.map(function(i) { return i.entry });

  if (album.create) {
    var onError = this.onError_.bind(this,
        loadTimeData.getString('PHOTO_IMPORT_IMPORTING_ERROR'));
    this.createAlbum_(album.name,
        this.startImport_.bind(this, entries),
        onError);
  } else {
    this.startImport_(entries, album.entry);
  }
};

/**
 * Starts importing process.
 * @param {Array.<FileEntry>} entries List of entries to import.
 * @param {DirectoryEntry} dir Where to import.
 * @private
 */
PhotoImport.prototype.startImport_ = function(entries, dir) {
  var files = entries.map(function(e) { return e.fullPath }).join('\n');
  var operationInfo = {
    isCut: false,
    isOnGData: PathUtil.getRootType(entries[0].fullPath) == RootType.GDATA,
    sourceDir: null,
    directories: '',
    files: files
  };
  this.copyManager_.paste(operationInfo, dir.fullPath, true);
};

/**
 * Creates a directory for an album.
 * @param {string} name Album name.
 * @param {function(DirectoryEntry)} onSuccess Success callback.
 * @param {function} onError Failure callback.
 * @private
 */
PhotoImport.prototype.createAlbum_ = function(name, onSuccess, onError) {
  var callback = function(entry) {
    this.initAlbums_();
    onSuccess(entry);
  }.bind(this);

  this.albumsDir_.getDirectory(name, { create: true, exclusive: true},
      callback, onError);
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

/**
 * Creates a selection controller that is to be used with grid.
 * @param {cr.ui.ListSelectionModel} selectionModel The selection model to
 *     interact with.
 * @param {cr.ui.Grid} grid The grid to interact with.
 * @constructor
 * @extends {!cr.ui.ListSelectionController}
 */
function GridSelectionController(selectionModel, grid) {
  this.selectionModel_ = selectionModel;
  this.grid_ = grid;
}

/**
 * Extends cr.ui.ListSelectionController.
 */
GridSelectionController.prototype.__proto__ =
    cr.ui.ListSelectionController.prototype;

/** @inheritDoc */
GridSelectionController.prototype.getIndexBelow = function(index) {
  if (index == this.getLastIndex()) {
    return -1;
  }

  var dm = this.grid_.dataModel;
  var columns = this.grid_.columns;
  var min = (Math.floor(index / columns) + 1) * columns;

  for (var row = 1; true; row++) {
    var end = index + columns * row;
    var start = Math.max(min, index + columns * (row - 1));
    if (start > dm.length) break;

    for (var i = end; i > start; i--) {
      if (i < dm.length && dm.item(i).type == 'entry')
        return i;
    }
  }

  return this.getLastIndex();
};

/** @inheritDoc */
GridSelectionController.prototype.getIndexAbove = function(index) {
  if (index == this.getFirstIndex()) {
    return -1;
  }

  var dm = this.grid_.dataModel;
  index -= this.grid_.columns;
  while (index >= 0 && dm.item(index).type != 'entry') {
    index--;
  }

  return index < 0 ? this.getFirstIndex() : index;
};

/** @inheritDoc */
GridSelectionController.prototype.getIndexBefore = function(index) {
  var dm = this.grid_.dataModel;
  index--;
  while (index >= 0 && dm.item(index).type != 'entry') {
    index--;
  }
  return index;
};

/** @inheritDoc */
GridSelectionController.prototype.getIndexAfter = function(index) {
  var dm = this.grid_.dataModel;
  index++;
  while (index < dm.length && dm.item(index).type != 'entry') {
    index++;
  }
  return index == dm.length ? -1 : index;
};

/** @inheritDoc */
GridSelectionController.prototype.getFirstIndex = function() {
  var dm = this.grid_.dataModel;
  for (var index = 0; index < dm.length; index++) {
    if (dm.item(index).type == 'entry')
      return index;
  }
  return -1;
};

/** @inheritDoc */
GridSelectionController.prototype.getLastIndex = function() {
  var dm = this.grid_.dataModel;
  for (var index = dm.length - 1; index >= 0; index--) {
    if (dm.item(index).type == 'entry')
      return index;
  }
  return -1;
};

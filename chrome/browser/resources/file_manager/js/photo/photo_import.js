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
  this.destination_ = null;
  this.myPhotosDirectory_ = null;

  this.initDom_();
  this.initMyPhotos_();
  this.loadSource_(params.source);
}

PhotoImport.prototype = { __proto__: cr.EventTarget.prototype };

/**
 * Single item width.
 * Keep in sync with .grid-item rule in photo_import.css.
 */
PhotoImport.ITEM_WIDTH = 164 + 8;

/**
 * Number of tries in creating a destination directory.
 */
PhotoImport.CREATE_DESTINATION_TRIES = 100;

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

  this.document_.querySelector('title').textContent =
      loadTimeData.getString('PHOTO_IMPORT_TITLE');
  this.dom_.querySelector('.caption').textContent =
      loadTimeData.getString('PHOTO_IMPORT_CAPTION');
  this.selectAllNone_ = this.dom_.querySelector('.select');
  this.selectAllNone_.addEventListener('click',
      this.onSelectAllNone_.bind(this));

  this.dom_.querySelector('label[for=delete-after-checkbox]').textContent =
      loadTimeData.getString('PHOTO_IMPORT_DELETE_AFTER');
  this.selectedCount_ = this.dom_.querySelector('.selected-count');

  this.importButton_ = this.dom_.querySelector('button.import');
  this.importButton_.textContent =
      loadTimeData.getString('PHOTO_IMPORT_IMPORT_BUTTON');
  this.importButton_.addEventListener('click', this.onImportClick_.bind(this));

  this.grid_ = this.dom_.querySelector('grid');
  cr.ui.Grid.decorate(this.grid_);
  this.grid_.itemConstructor = GridItem.bind(null, this);
  this.fileList_ = new cr.ui.ArrayDataModel([]);
  this.grid_.selectionModel = new cr.ui.ListSelectionModel();
  this.grid_.dataModel = this.fileList_;
  this.grid_.selectionModel.addEventListener('change',
      this.onSelectionChanged_.bind(this));
  this.onSelectionChanged_();

  this.importingDialog_ = new ImportingDialog(this.dom_, this.copyManager_,
      this.metadataCache_);
};

/**
 * One-time initialization of the My Photos directory.
 * @private
 */
PhotoImport.prototype.initMyPhotos_ = function() {
  var onError = this.onError_.bind(
      this, loadTimeData.getString('PHOTO_IMPORT_DRIVE_ERROR'));

  var onDirectory = function(dir) {
    // This may enable the import button, so check that.
    this.myPhotosDirectory_ = dir;
    this.onSelectionChanged_();
  }.bind(this);

  var onMounted = function() {
    var dir = PathUtil.join(
        RootDirectory.DRIVE,
        loadTimeData.getString('PHOTO_IMPORT_MY_PHOTOS_DIRECTORY_NAME'));
    util.getOrCreateDirectory(this.filesystem_.root, dir, onDirectory, onError);
  }.bind(this);

  if (this.volumeManager_.isMounted(RootDirectory.DRIVE)) {
    onMounted();
  } else {
    this.volumeManager_.mountDrive(onMounted, onError);
  }
};

/**
 * Creates the destination directory.
 * @param {function} onSuccess Callback on success.
 * @private
 */
PhotoImport.prototype.createDestination_ = function(onSuccess) {
  var onError = this.onError_.bind(
      this, loadTimeData.getString('PHOTO_IMPORT_DRIVE_ERROR'));

  var dateFormatter = v8Intl.DateTimeFormat(
      [] /* default locale */,
      {year: 'numeric', month: 'short', day: 'numeric'});

  var baseName = PathUtil.join(
      RootDirectory.DRIVE,
      loadTimeData.getString('PHOTO_IMPORT_MY_PHOTOS_DIRECTORY_NAME'),
      dateFormatter.format(new Date()));

  var createDirectory = function(directoryName) {
    this.filesystem_.root.getDirectory(
        directoryName,
        { create: true },
        function(dir) {
          this.destination_ = dir;
          onSuccess();
        }.bind(this),
        onError);
  };

  // Try to create a directory: Name, Name (2), Name (3)...
  var tryNext = function(tryNumber) {
    if (tryNumber > PhotoImport.CREATE_DESTINATION_TRIES) {
      console.error('Too many directories with the same base name exist.');
      onError();
      return;
    }
    var directoryName = baseName;
    if (tryNumber > 1)
      directoryName += ' (' + (tryNumber) + ')';
    this.filesystem_.root.getDirectory(
        directoryName,
        { create: false },
        tryNext.bind(this, tryNumber + 1),
        createDirectory.bind(this, directoryName));
  }.bind(this);

  tryNext(1);
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
    this.fillGrid_();
  }.bind(this);

  var onEntry = function(entry) {
    util.traverseTree(entry, onTraversed, 0 /* infinite depth */,
        FileType.isVisible);
  }.bind(this);

  var onError = this.onError_.bind(
      this, loadTimeData.getString('PHOTO_IMPORT_SOURCE_ERROR'));

  util.resolvePath(this.filesystem_.root, source, onEntry, onError);
};

/**
 * Renders files into grid.
 * @private
 */
PhotoImport.prototype.fillGrid_ = function() {
  if (!this.mediaFilesList_) return;
  this.fileList_.splice(0, this.fileList_.length);
  this.fileList_.push.apply(this.fileList_, this.mediaFilesList_);
};

/**
 * Creates groups for files based on modification date.
 * @param {Array.<Entry>} files File list.
 * @param {Object} filesystem Filesystem metadata.
 * @return {Array.<Object>} List of grouped items.
 * @private
 */
PhotoImport.prototype.createGroups_ = function(files, filesystem) {
  var dateFormatter = v8Intl.DateTimeFormat(
      [] /* default locale */,
      {year: 'numeric', month: 'short', day: 'numeric'});

  var columns = this.grid_.columns;

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

  return list;
};

/**
 * Decorates grid item.
 * @param {HTMLLIElement} li The list item.
 * @param {FileEntry} entry The file entry.
 * @private
 */
PhotoImport.prototype.decorateGridItem_ = function(li, entry) {
  li.className = 'grid-item';
  li.entry = entry;

  var frame = this.document_.createElement('div');
  frame.className = 'grid-frame';
  li.appendChild(frame);

  var box = this.document_.createElement('div');
  box.className = 'img-container';
  this.metadataCache_.get(entry, 'thumbnail|filesystem',
      function(metadata) {
        new ThumbnailLoader(entry.toURL(),
                            ThumbnailLoader.LoaderType.IMAGE,
                            metadata).
            load(box, ThumbnailLoader.FillMode.FIT);
      });
  frame.appendChild(box);

  var check = this.document_.createElement('div');
  check.className = 'check';
  li.appendChild(check);
};

/**
 * Handles the 'pick all/none' action.
 * @private
 */
PhotoImport.prototype.onSelectAllNone_ = function() {
  var sm = this.grid_.selectionModel;
  if (sm.selectedIndexes.length == this.fileList_.length) {
    sm.unselectAll();
  } else {
    sm.selectAll();
  }
};

/**
 * Show error message.
 * @param {string} message Error message.
 * @private
 */
PhotoImport.prototype.onError_ = function(message) {
  // TODO(mtomasz): Implement error handling. crbug.com/177164.
  console.error(message);
};

/**
 * Resize event handler.
 * @private
 */
PhotoImport.prototype.onResize_ = function() {
  var g = this.grid_;
  g.startBatchUpdates();
  setTimeout(function() {
    g.columns = 0;
    g.redraw();
    g.endBatchUpdates();
  }, 0);
};

/**
 * @return {Array.<Object>} The list of selected entries.
 * @private
 */
PhotoImport.prototype.getSelectedItems_ = function() {
  var indexes = this.grid_.selectionModel.selectedIndexes;
  var list = [];
  for (var i = 0; i < indexes.length; i++) {
    list.push(this.fileList_.item(indexes[i]));
  }
  return list;
};

/**
 * Event handler for picked items change.
 * @private
 */
PhotoImport.prototype.onSelectionChanged_ = function() {
  var count = this.grid_.selectionModel.selectedIndexes.length;
  this.selectedCount_.textContent = count == 0 ? '' :
      count == 1 ? loadTimeData.getString('PHOTO_IMPORT_ONE_SELECTED') :
                   loadTimeData.getStringF('PHOTO_IMPORT_MANY_SELECTED', count);
  this.importButton_.disabled = count == 0 || this.myPhotosDirectory_ == null;
  this.selectAllNone_.textContent = loadTimeData.getString(
      count == this.fileList_.length && count > 0 ?
          'PHOTO_IMPORT_SELECT_NONE' : 'PHOTO_IMPORT_SELECT_ALL');
};

/**
 * Event handler for import button click.
 * @param {Event} event The event.
 * @private
 */
PhotoImport.prototype.onImportClick_ = function(event) {
  this.createDestination_(function() {
    var entries = this.getSelectedItems_();
    var move = this.dom_.querySelector('#delete-after-checkbox').checked;

    var percentage = Math.round(entries.length / this.fileList_.length * 100);
    metrics.recordMediumCount('PhotoImport.ImportCount', entries.length);
    metrics.recordSmallCount('PhotoImport.ImportPercentage', percentage);

    this.importingDialog_.show(entries, this.destination_, move);
  }.bind(this));
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

/** @override */
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

/** @override */
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

/** @override */
GridSelectionController.prototype.getIndexBefore = function(index) {
  var dm = this.grid_.dataModel;
  index--;
  while (index >= 0 && dm.item(index).type != 'entry') {
    index--;
  }
  return index;
};

/** @override */
GridSelectionController.prototype.getIndexAfter = function(index) {
  var dm = this.grid_.dataModel;
  index++;
  while (index < dm.length && dm.item(index).type != 'entry') {
    index++;
  }
  return index == dm.length ? -1 : index;
};

/** @override */
GridSelectionController.prototype.getFirstIndex = function() {
  var dm = this.grid_.dataModel;
  for (var index = 0; index < dm.length; index++) {
    if (dm.item(index).type == 'entry')
      return index;
  }
  return -1;
};

/** @override */
GridSelectionController.prototype.getLastIndex = function() {
  var dm = this.grid_.dataModel;
  for (var index = dm.length - 1; index >= 0; index--) {
    if (dm.item(index).type == 'entry')
      return index;
  }
  return -1;
};

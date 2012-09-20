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

  this.initDom_();
  this.initDestination_();
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

  this.document_.querySelector('title').textContent =
      loadTimeData.getString('PHOTO_IMPORT_TITLE');
  this.dom_.querySelector('.caption').textContent =
      loadTimeData.getString('PHOTO_IMPORT_CAPTION');

  this.dom_.querySelector('label[for=delete-after-checkbox]').textContent =
      loadTimeData.getString('PHOTO_IMPORT_DELETE_AFTER');
  this.pickedCount_ = this.dom_.querySelector('.picked-count');

  this.importButton_ = this.dom_.querySelector('button.import');
  this.importButton_.textContent =
      loadTimeData.getString('PHOTO_IMPORT_IMPORT_BUTTON');
  this.importButton_.addEventListener('click', this.onImportClick_.bind(this));

  this.grid_ = this.dom_.querySelector('grid');
  cr.ui.Grid.decorate(this.grid_);
  this.grid_.itemConstructor =
      GridItem.bind(null, this);
  this.fileList_ = new cr.ui.ArrayDataModel([]);
  this.grid_.selectionModel = new cr.ui.ListSelectionModel();
  this.grid_.dataModel = this.fileList_;
  this.grid_.activateItemAtIndex = this.onActivateItemAtIndex_.bind(this);
  this.grid_.addEventListener('keypress', this.onGridKeyPress_.bind(this));
  this.onPickedItemsChanged_();

  this.importingDialog_ = new ImportingDialog(this.dom_, this.copyManager_,
      this.metadataCache_);
};

/**
 * One-time initialization of destination directory.
 * @private
 */
PhotoImport.prototype.initDestination_ = function() {
  var onError = this.onError_.bind(
      this, loadTimeData.getString('PHOTO_IMPORT_GDATA_ERROR'));

  var onDirectory = function(dir) {
    this.destination_ = dir;
    // This may enable the import button, so check that.
    this.onPickedItemsChanged_();
  }.bind(this);

  var onMounted = function() {
    var dir = PathUtil.join(RootDirectory.GDATA, PhotoImport.GDATA_PHOTOS_DIR);
    util.getOrCreateDirectory(this.filesystem_.root, dir, onDirectory, onError);
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
    this.fillGrid_();
  }.bind(this);

  var onEntry = function(entry) {
    util.traverseTree(entry, onTraversed, 0 /* infinite depth */);
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
  var files = this.mediaFilesList_;
  if (!files) return;

  var list = [];
  for (var index = 0; index < files.length; index++) {
    list.push({ entry: files[index], picked: false });
  }

  this.fileList_.splice(0, this.fileList_.length);
  this.fileList_.push.apply(this.fileList_, list);
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
 * @param {Object} item The model item.
 * @private
 */
PhotoImport.prototype.decorateGridItem_ = function(li, item) {
  li.className = 'grid-item';
  li.item = item;

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

  var checkbox = this.document_.createElement('input');
  checkbox.type = 'checkbox';
  checkbox.checked = item.picked;
  checkbox.addEventListener('click', this.onCheckboxClick_.bind(this, li));
  checkbox.addEventListener('mousedown', function(e) { e.stopPropagation(); });
  checkbox.addEventListener('mouseup', function(e) { e.stopPropagation(); });
  frame.appendChild(checkbox);
};

/**
 * Event handler for clicking on checkbox in grid.
 * @param {ListItem} li Grid item.
 * @param {Event} event Event.
 * @private
 */
PhotoImport.prototype.onCheckboxClick_ = function(li, event) {
  var checkbox = event.target;
  li.item.picked = checkbox.checked;
  this.onPickedItemsChanged_();
  event.stopPropagation();
};

/**
 * Handles the activate (double click) of grid item.
 * @param {number} index Item index.
 * @param {boolean=} opt_batch Whether this is a part of a batch. If yes, no
 *     picked event will be fired.
 * @private
 */
PhotoImport.prototype.onActivateItemAtIndex_ = function(index, opt_batch) {
  var item = this.fileList_.item(index);
  item.picked = !item.picked;
  var li = this.grid_.getListItemByIndex(index);
  if (li) {
    var checkbox = li.querySelector('input[type=checkbox]');
    checkbox.checked = item.picked;
  }
  if (!opt_batch) this.onPickedItemsChanged_();
};

/**
 * Event handler for keypress on grid.
 * @param {Evevnt} event Event.
 * @private
 */
PhotoImport.prototype.onGridKeyPress_ = function(event) {
  if (event.keyCode == 32) {
    this.grid_.selectionModel.selectedIndexes.forEach(function(index) {
      this.onActivateItemAtIndex_(index, true);
    }.bind(this));
    this.onPickedItemsChanged_();
    event.stopPropagation();
    event.preventDefault();
  }
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
  var g = this.grid_;
  g.startBatchUpdates();
  setTimeout(function() {
    g.columns = 0;
    g.redraw();
    g.endBatchUpdates();
  }, 0);
};

/**
 * @return {Array.<Object>} The list of picked entries.
 * @private
 */
PhotoImport.prototype.getPickedItems_ = function() {
  var list = [];
  for (var i = 0; i < this.fileList_.length; i++) {
    var item = this.fileList_.item(i);
    if (item.picked)
      list.push(item);
  }
  return list;
};

/**
 * Event handler for picked items change.
 * @private
 */
PhotoImport.prototype.onPickedItemsChanged_ = function() {
  var count = this.getPickedItems_().length;
  this.pickedCount_.textContent =
      count == 0 ?
          loadTimeData.getString('PHOTO_IMPORT_NOTHING_PICKED') :
          count == 1 ?
              loadTimeData.getString('PHOTO_IMPORT_ONE_PICKED') :
              loadTimeData.getStringF('PHOTO_IMPORT_MANY_PICKED', count);
  this.importButton_.disabled = count == 0 || this.destination_ == null;
};

/**
 * Event handler for import button click.
 * @param {Event} event The event.
 * @private
 */
PhotoImport.prototype.onImportClick_ = function(event) {
  var items = this.getPickedItems_();
  var entries = items.map(function(item) { return item.entry; });
  var move = this.dom_.querySelector('#delete-after-checkbox').checked;
  this.importingDialog_.show(entries, this.destination_, move);
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

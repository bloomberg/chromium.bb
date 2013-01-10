// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * FileGrid constructor.
 *
 * Represents grid for the Grid Vew in the File Manager.
 * @constructor
 * @extends {cr.ui.Grid}
 */

function FileGrid() {
  throw new Error('Use FileGrid.decorate');
}

/**
 * Inherits from cr.ui.Grid.
 */
FileGrid.prototype.__proto__ = cr.ui.Grid.prototype;

/**
 * Decorates an HTML element to be a FileGrid.
 * @param {HTMLElement} self The grid to decorate.
 * @param {MetadataCache} metadataCache Metadata cache to find entries
 *                                      metadata.
 */
FileGrid.decorate = function(self, metadataCache) {
  cr.ui.Grid.decorate(self);
  self.__proto__ = FileGrid.prototype;
  self.metadataCache_ = metadataCache;

  self.itemConstructor = function(entry) {
    var item = self.ownerDocument.createElement('LI');
    FileGrid.Item.decorate(item, entry, self);
    return item;
  };
};

/**
 * Updates items to reflect metadata changes.
 * @param {string} type Type of metadata changed.
 * @param {Object<string, Object>} props Map from entry URLs to metadata props.
 */
FileGrid.prototype.updateListItemsMetadata = function(type, props) {
  var boxes = this.querySelectorAll('.img-container');
  for (var i = 0; i < boxes.length; i++) {
    var box = boxes[i];
    var entry = this.dataModel.item(this.getListItemAncestor(box));
    if (!entry || !(entry.toURL() in props))
      continue;

    FileGrid.decorateThumbnailBox(box, entry, this.metadataCache_,
                                  false /* fit, not fill */);
  }
};

/**
 * Renders a thumbnail for Drag And Drop operations.
 * @param {HTMLDocument} doc Owner document.
 * @param {MetadataCache} metadataCache To retrieve metadata.
 * @param {Entry} entry Entry to render a thumbnail for.
 * @return {HTMLElement} Created element.
 */
FileGrid.renderDragThumbnail = function(doc, metadataCache, entry) {
  var item = doc.createElement('div');
  FileGrid.decorateThumbnail(item, entry, metadataCache, true);
  return item;
};

/**
 * Decorates thumbnail.
 * @param {HTMLElement} li List item.
 * @param {Entry} entry Entry to render a thumbnail for.
 * @param {MetadataCache} metadataCache To retrieve metadata.
 */
FileGrid.decorateThumbnail = function(li, entry, metadataCache) {
  li.className = 'thumbnail-item';
  filelist.decorateListItem(li, entry, metadataCache);

  var frame = li.ownerDocument.createElement('div');
  frame.className = 'thumbnail-frame';
  li.appendChild(frame);

  var box = li.ownerDocument.createElement('div');
  FileGrid.decorateThumbnailBox(box, entry, metadataCache, false);
  frame.appendChild(box);

  var bottom = li.ownerDocument.createElement('div');
  bottom.className = 'thumbnail-bottom';
  frame.appendChild(bottom);

  bottom.appendChild(filelist.renderFileNameLabel(li.ownerDocument, entry));
};

/**
 * Decorates the box containing a centered thumbnail image.
 *
 * @param {HTMLDivElement} box Box to decorate.
 * @param {Entry} entry Entry which thumbnail is generating for.
 * @param {MetadataCache} metadataCache To retrieve metadata.
 * @param {boolean} fill True if fill, false if fit.
 * @param {function(HTMLElement)} opt_imageLoadCallback Callback called when
 *                                the image has been loaded before inserting
 *                                it into the DOM.
 */
FileGrid.decorateThumbnailBox = function(
    box, entry, metadataCache, fill, opt_imageLoadCallback) {
  var self = this;

  box.className = 'img-container';
  if (entry.isDirectory) {
    box.setAttribute('generic-thumbnail', 'folder');
    if (opt_imageLoadCallback)
      setTimeout(opt_imageLoadCallback, 0, null /* callback parameter */);
    return;
  }

  var imageUrl = entry.toURL();

  // Failing to fetch a thumbnail likely means that the thumbnail URL
  // is now stale. Request a refresh of the current directory, to get
  // the new thumbnail URLs. Once the directory is refreshed, we'll get
  // notified via onDirectoryChanged event.
  var onImageLoadError = function() {
    metadataCache.refreshFileMetadata(imageUrl);
  };

  var metadataTypes = 'thumbnail|filesystem';

  if (FileType.isOnGDrive(imageUrl)) {
    metadataTypes += '|gdata';
  } else {
    // TODO(dgozman): If we ask for 'media' for a GDrive file we fall into an
    // infinite loop.
    metadataTypes += '|media';
  }

  metadataCache.get(imageUrl, metadataTypes,
      function(metadata) {
        new ThumbnailLoader(imageUrl, metadata).
            load(box, fill, opt_imageLoadCallback, onImageLoadError);
      });
};

/**
 * Item for the Grid View.
 * @constructor
 */
FileGrid.Item = function() {
  throw new Error();
};

/**
 * Inherits from cr.ui.ListItem.
 */
FileGrid.Item.prototype.__proto__ = cr.ui.ListItem.prototype;

Object.defineProperty(FileGrid.Item.prototype, 'label', {
  get: function() {
    return this.querySelector('filename-label').textContent;
  }
});

/**
 * @param {Element} li List item element.
 * @param {Entry} entry File entry.
 * @param {FileGrid} grid Owner.
 */
FileGrid.Item.decorate = function(li, entry, grid) {
  li.__proto__ = FileGrid.Item.prototype;
  FileGrid.decorateThumbnail(li, entry, grid.metadataCache_, true);

  if (grid.selectionModel.multiple) {
    var checkBox = li.ownerDocument.createElement('input');
    filelist.decorateSelectionCheckbox(checkBox, entry, grid);
    checkBox.classList.add('white');
    var bottom = li.querySelector('.thumbnail-bottom');
    bottom.appendChild(checkBox);
    bottom.classList.add('show-checkbox');
  }

  // Override the default role 'listitem' to 'option' to match the parent's
  // role (listbox).
  li.setAttribute('role', 'option');
};


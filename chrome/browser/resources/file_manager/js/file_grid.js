// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

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
 * Thumbnail quality.
 * @enum {number}
 */
FileGrid.ThumbnailQuality = {
  LOW: 0,
  HIGH: 1
};

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

  self.scrollBar_ = new MainPanelScrollBar();
  self.scrollBar_.initialize(self.parentNode, self);

  self.itemConstructor = function(entry) {
    var item = self.ownerDocument.createElement('LI');
    FileGrid.Item.decorate(item, entry, self);
    return item;
  };

  self.relayoutAggregation_ =
      new AsyncUtil.Aggregation(self.relayoutImmediately_.bind(self));
};

/**
 * Updates items to reflect metadata changes.
 * @param {string} type Type of metadata changed.
 * @param {Object.<string, Object>} props Map from entry URLs to metadata props.
 */
FileGrid.prototype.updateListItemsMetadata = function(type, props) {
  var boxes = this.querySelectorAll('.img-container');
  for (var i = 0; i < boxes.length; i++) {
    var box = boxes[i];
    var entry = this.dataModel.item(this.getListItemAncestor(box));
    if (!entry || !(entry.toURL() in props))
      continue;

    FileGrid.decorateThumbnailBox(box,
                                  entry,
                                  this.metadataCache_,
                                  ThumbnailLoader.FillMode.FIT,
                                  FileGrid.ThumbnailQuality.HIGH);
  }
};

/**
 * Redraws the UI. Skips multiple consecutive calls.
 */
FileGrid.prototype.relayout = function() {
  this.relayoutAggregation_.run();
};

/**
 * Redraws the UI immediately.
 * @private
 */
FileGrid.prototype.relayoutImmediately_ = function() {
  this.startBatchUpdates();
  this.columns = 0;
  this.redraw();
  this.endBatchUpdates();
  cr.dispatchSimpleEvent(this, 'relayout');
};

/**
 * Decorates thumbnail.
 * @param {HTMLElement} li List item.
 * @param {Entry} entry Entry to render a thumbnail for.
 * @param {MetadataCache} metadataCache To retrieve metadata.
 */
FileGrid.decorateThumbnail = function(li, entry, metadataCache) {
  li.className = 'thumbnail-item';
  if (entry)
    filelist.decorateListItem(li, entry, metadataCache);

  var frame = li.ownerDocument.createElement('div');
  frame.className = 'thumbnail-frame';
  li.appendChild(frame);

  var box = li.ownerDocument.createElement('div');
  if (entry) {
    FileGrid.decorateThumbnailBox(box,
                                  entry,
                                  metadataCache,
                                  ThumbnailLoader.FillMode.AUTO,
                                  FileGrid.ThumbnailQuality.HIGH);
  }
  frame.appendChild(box);

  var bottom = li.ownerDocument.createElement('div');
  bottom.className = 'thumbnail-bottom';
  frame.appendChild(bottom);

  bottom.appendChild(filelist.renderFileNameLabel(
      li.ownerDocument, entry ? entry.name : ''));
};

/**
 * Decorates the box containing a centered thumbnail image.
 *
 * @param {HTMLDivElement} box Box to decorate.
 * @param {Entry} entry Entry which thumbnail is generating for.
 * @param {MetadataCache} metadataCache To retrieve metadata.
 * @param {ThumbnailLoader.FillMode} fillMode Fill mode.
 * @param {FileGrid.ThumbnailQuality} quality Thumbnail quality.
 * @param {function(HTMLElement)=} opt_imageLoadCallback Callback called when
 *     the image has been loaded before inserting it into the DOM.
 */
FileGrid.decorateThumbnailBox = function(
    box, entry, metadataCache, fillMode, quality, opt_imageLoadCallback) {
  box.className = 'img-container';
  if (entry.isDirectory) {
    box.setAttribute('generic-thumbnail', 'folder');
    if (opt_imageLoadCallback)
      setTimeout(opt_imageLoadCallback, 0, null /* callback parameter */);
    return;
  }

  var imageUrl = entry.toURL();
  var metadataTypes = 'thumbnail|filesystem';

  if (FileType.isOnDrive(imageUrl)) {
    metadataTypes += '|drive';
  } else {
    // TODO(dgozman): If we ask for 'media' for a Drive file we fall into an
    // infinite loop.
    metadataTypes += '|media';
  }

  // Drive provides high quality thumbnails via USE_EMBEDDED, however local
  // images usually provide very tiny thumbnails, therefore USE_EMBEDDE can't
  // be used to obtain high quality output.
  var useEmbedded;
  switch (quality) {
    case FileGrid.ThumbnailQuality.LOW:
      useEmbedded = ThumbnailLoader.UseEmbedded.USE_EMBEDDED;
      break;
    case FileGrid.ThumbnailQuality.HIGH:
      useEmbedded = FileType.isOnDrive(imageUrl) ?
          ThumbnailLoader.UseEmbedded.USE_EMBEDDED :
          ThumbnailLoader.UseEmbedded.NO_EMBEDDED;
      break;
  }

  metadataCache.get(imageUrl, metadataTypes,
      function(metadata) {
        new ThumbnailLoader(imageUrl,
                            ThumbnailLoader.LoaderType.IMAGE,
                            metadata,
                            undefined,  // opt_mediaType
                            useEmbedded).
            load(box,
                fillMode,
                ThumbnailLoader.OptimizationMode.DISCARD_DETACHED,
                opt_imageLoadCallback);
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
  /**
   * @this {FileGrid.Item}
   * @return {string} Label of the item.
   */
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

  // Override the default role 'listitem' to 'option' to match the parent's
  // role (listbox).
  li.setAttribute('role', 'option');
};

/**
 * Sets the margin height for the transparent preview panel at the bottom.
 * @param {number} margin Margin to be set in px.
 */
FileGrid.prototype.setBottomMarginForPanel = function(margin) {
  this.style.paddingBottom = margin + 'px';
  this.scrollBar_.setBottomMarginForPanel(margin);
};

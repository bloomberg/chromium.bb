// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Scrollable thumbnail ribbon at the bottom of the Gallery in the Slide mode.
 *
 * @param {Document} document Document.
 * @param {MetadataCache} metadataCache MetadataCache instance.
 * @param {function(number)} selectFunc Selection function.
 * @return {Element} Ribbon element.
 * @constructor
 */
function Ribbon(document, metadataCache, selectFunc) {
  var self = document.createElement('div');
  Ribbon.decorate(self, metadataCache, selectFunc);
  return self;
}

/**
 * Inherit from HTMLDivElement.
 */
Ribbon.prototype.__proto__ = HTMLDivElement.prototype;

/**
 * Decorate a Ribbon instance.
 *
 * @param {Ribbon} self Self pointer.
 * @param {MetadataCache} metadataCache MetadataCache instance.
 * @param {function(number)} selectFunc Selection function.
 */
Ribbon.decorate = function(self, metadataCache, selectFunc) {
  self.__proto__ = Ribbon.prototype;
  self.metadataCache_ = metadataCache;
  self.selectFunc_ = selectFunc;

  self.className = 'ribbon';

  self.firstVisibleIndex_ = 0;
  self.lastVisibleIndex_ = -1;  // Zero thumbnails

  self.renderCache_ = {};
};

/**
 * Max number of thumbnails in the ribbon.
 * @type {Number}
 */
Ribbon.ITEMS_COUNT = 5;

/**
 * Update the ribbon.
 *
 * @param {Array.<Gallery.Item>} items Array of items.
 * @param {number} selectedIndex Selected index.
 */
Ribbon.prototype.update = function(items, selectedIndex) {
  // Never show a single thumbnail.
  if (items.length == 1)
    return;

  // TODO(dgozman): use margin instead of 2 here.
  var itemWidth = this.clientHeight - 2;
  var fullItems = Ribbon.ITEMS_COUNT;
  fullItems = Math.min(fullItems, items.length);
  var right = Math.floor((fullItems - 1) / 2);

  var fullWidth = fullItems * itemWidth;
  this.style.width = fullWidth + 'px';

  var lastIndex = selectedIndex + right;
  lastIndex = Math.max(lastIndex, fullItems - 1);
  lastIndex = Math.min(lastIndex, items.length - 1);
  var firstIndex = lastIndex - fullItems + 1;

  if (this.firstVisibleIndex_ != firstIndex ||
      this.lastVisibleIndex_ != lastIndex) {

    if (this.lastVisibleIndex_ == -1) {
      this.firstVisibleIndex_ = firstIndex;
      this.lastVisibleIndex_ = lastIndex;
    }

    this.textContent = '';
    var startIndex = Math.min(firstIndex, this.firstVisibleIndex_);
    var toRemove = [];
    // All the items except the first one treated equally.
    for (var index = startIndex + 1;
         index <= Math.max(lastIndex, this.lastVisibleIndex_);
         ++index) {
      var box = this.renderThumbnail_(index, items);
      box.style.marginLeft = '0';
      this.appendChild(box);
      if (index < firstIndex || index > lastIndex) {
        toRemove.push(box);
      }
    }

    var margin = itemWidth * Math.abs(firstIndex - this.firstVisibleIndex_);
    var startBox = this.renderThumbnail_(startIndex, items);
    if (startIndex == firstIndex) {
      // Sliding to the right.
      startBox.style.marginLeft = -margin + 'px';
      if (this.firstChild)
        this.insertBefore(startBox, this.firstChild);
      else
        this.appendChild(startBox);
      setTimeout(function() {
        startBox.style.marginLeft = '0';
      }, 0);
    } else {
      // Sliding to the left. Start item will become invisible and should be
      // removed afterwards.
      toRemove.push(startBox);
      startBox.style.marginLeft = '0';
      if (this.firstChild)
        this.insertBefore(startBox, this.firstChild);
      else
        this.appendChild(startBox);
      setTimeout(function() {
        startBox.style.marginLeft = -margin + 'px';
      }, 0);
    }

    ImageUtil.setClass(this, 'fade-left',
        firstIndex > 0 && selectedIndex != firstIndex);

    ImageUtil.setClass(this, 'fade-right',
        lastIndex < items.length - 1 && selectedIndex != lastIndex);

    this.firstVisibleIndex_ = firstIndex;
    this.lastVisibleIndex_ = lastIndex;

    if (this.removeTimeout_)
      clearTimeout(this.removeTimeout_);

    this.removeTimeout_ = setTimeout(function() {
      this.removeTimeout_ = null;
      for (var i = 0; i < toRemove.length; i++) {
        var box = toRemove[i];
        if (box.parentNode == this)
          this.removeChild(box);
      }
    }.bind(this), 200);
  }

  var oldSelected = this.querySelector('[selected]');
  if (oldSelected) oldSelected.removeAttribute('selected');

  var newSelected = this.getThumbnail_(selectedIndex);
  if (newSelected) newSelected.setAttribute('selected', true);
};

/**
 * Create a DOM element for a thumbnail.
 *
 * @param {number} index Item index.
 * @param {Array.<Gallery.Item>} items Items array.
 * @return {Element} Newly created element.
 * @private
 */
Ribbon.prototype.renderThumbnail_ = function(index, items) {
  var url = items[index].getUrl();

  var cached = this.renderCache_[url];
  if (cached)
    return cached;

  var thumbnail = this.ownerDocument.createElement('div');
  thumbnail.className = 'ribbon-image';
  thumbnail.addEventListener('click', this.selectFunc_.bind(null, index));
  thumbnail.setAttribute('index', index);

  util.createChild(thumbnail, 'image-wrapper');

  this.metadataCache_.get(url, Gallery.METADATA_TYPE,
      this.setThumbnailImage_.bind(this, thumbnail, url));

  this.renderCache_[url] = thumbnail;
  return thumbnail;
};

/**
 * Set the thumbnail image.
 *
 * @param {Element} thumbnail Thumbnail element
 * @param {string} url Image url.
 * @param {Object} metadata Metadata.
 * @private
 */
Ribbon.prototype.setThumbnailImage_ = function(thumbnail, url, metadata) {
  new ThumbnailLoader(url, metadata).load(
     thumbnail.querySelector('.image-wrapper'), true /* fill */);
};

/**
 * Update the thumbnail image.
 *
 * @param {number} index Item index.
 * @param {string} url Image url.
 * @param {Object} metadata Metadata.
 */
Ribbon.prototype.updateThumbnail = function(index, url, metadata) {
  var thumbnail = this.getThumbnail_(index) || this.renderCache_[url];
  if (thumbnail)
    this.setThumbnailImage_(thumbnail, url, metadata);
};

/**
 * @param {number} index Thumbnail index.
 * @return {Element} Thumbnail element or null if not rendered.
 * @private
 */
Ribbon.prototype.getThumbnail_ = function(index) {
  return this.querySelector('[index="' + index + '"]');
};

/**
 * Update the thumbnail element cache.
 *
 * @param {string} oldUrl Old url.
 * @param {string} newUrl New url.
 */
Ribbon.prototype.remapCache = function(oldUrl, newUrl) {
  if (oldUrl != newUrl && (oldUrl in this.renderCache_)) {
    this.renderCache_[newUrl] = this.renderCache_[oldUrl];
    delete this.renderCache_[oldUrl];
  }
};

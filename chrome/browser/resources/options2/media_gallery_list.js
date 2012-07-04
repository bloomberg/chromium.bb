// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var DeletableItem = options.DeletableItem;
  /** @const */ var DeletableItemList = options.DeletableItemList;

  /**
   * @constructor
   * @extends {DeletableItem}
   */
  function MediaGalleryListItem(galleryInfo) {
    var el = cr.doc.createElement('div');
    el.galleryInfo_ = galleryInfo;
    el.__proto__ = MediaGalleryListItem.prototype;
    el.decorate();
    return el;
  }

  MediaGalleryListItem.prototype = {
    __proto__: DeletableItem.prototype,

    decorate: function() {
      DeletableItem.prototype.decorate.call(this);

      var span = this.ownerDocument.createElement('span');
      span.textContent = this.galleryInfo_.displayName;
      this.contentElement.appendChild(span);
      this.contentElement.title = this.galleryInfo_.path;
    },
  };

  var MediaGalleryList = cr.ui.define('list');

  MediaGalleryList.prototype = {
    __proto__: DeletableItemList.prototype,

    /** @inheritDoc */
    decorate: function() {
      DeletableItemList.prototype.decorate.call(this);
      this.autoExpands_ = true;
    },

    /** @inheritDoc */
    createItem: function(galleryInfo) {
      return new MediaGalleryListItem(galleryInfo);
    },

    /** @inheritDoc */
    deleteItemAtIndex: function(index) {
      chrome.send('forgetGallery', [this.dataModel.item(index).id]);
    },
  };

  return {
    MediaGalleryList: MediaGalleryList
  };
});

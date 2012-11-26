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
  function MediaGalleriesListItem(galleryInfo) {
    var el = cr.doc.createElement('div');
    el.galleryInfo_ = galleryInfo;
    el.__proto__ = MediaGalleriesListItem.prototype;
    el.decorate();
    return el;
  }

  MediaGalleriesListItem.prototype = {
    __proto__: DeletableItem.prototype,

    decorate: function() {
      DeletableItem.prototype.decorate.call(this);

      var span = this.ownerDocument.createElement('span');
      span.textContent = this.galleryInfo_.displayName;
      this.contentElement.appendChild(span);
      this.contentElement.title = this.galleryInfo_.path;
    },
  };

  var MediaGalleriesList = cr.ui.define('list');

  MediaGalleriesList.prototype = {
    __proto__: DeletableItemList.prototype,

    /** @override */
    decorate: function() {
      DeletableItemList.prototype.decorate.call(this);
      this.autoExpands_ = true;
    },

    /** @override */
    createItem: function(galleryInfo) {
      return new MediaGalleriesListItem(galleryInfo);
    },

    /** @override */
    deleteItemAtIndex: function(index) {
      chrome.send('forgetGallery', [this.dataModel.item(index).id]);
    },
  };

  return {
    MediaGalleriesList: MediaGalleriesList
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('wallpapers', function() {
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;
  /** @const */ var Grid = cr.ui.Grid;
  /** @const */ var GridItem = cr.ui.GridItem;
  /** @const */ var GridSelectionController = cr.ui.GridSelectionController;
  /** @const */ var ListSingleSelectionModel = cr.ui.ListSingleSelectionModel;
  /** @const */ var ThumbnailSuffix = '_thumbnail.png';

  /**
   * Creates a new wallpaper thumbnails grid item.
   * @param {{baseURL: string, dynamicURL: string, layout: string,
   *          author: string, authorWebsite: string, availableOffline: boolean}}
   *     wallpaperInfo Wallpaper baseURL, dynamicURL, layout, author and
   *     author website.
   * @constructor
   * @extends {cr.ui.GridItem}
   */
  function WallpaperThumbnailsGridItem(wallpaperInfo) {
    var el = new GridItem(wallpaperInfo);
    el.__proto__ = WallpaperThumbnailsGridItem.prototype;
    return el;
  }

  WallpaperThumbnailsGridItem.prototype = {
    __proto__: GridItem.prototype,

    /** @override */
    decorate: function() {
      GridItem.prototype.decorate.call(this);
      // Removes garbage created by GridItem.
      this.innerText = '';
      var imageEl = cr.doc.createElement('img');
      imageEl.classList.add('thumbnail');
      cr.defineProperty(imageEl, 'offline', cr.PropertyKind.BOOL_ATTR);
      imageEl.offline = this.dataItem.availableOffline;
      this.appendChild(imageEl);
      var self = this;
      chrome.wallpaperPrivate.getThumbnail(this.dataItem.baseURL,
                                           function(data) {
        if (data) {
          var blob = new Blob([new Int8Array(data)], {'type' : 'image\/png'});
          imageEl.src = window.URL.createObjectURL(blob);
          imageEl.addEventListener('load', function(e) {
            window.URL.revokeObjectURL(this.src);
          });
        } else {
          var xhr = new XMLHttpRequest();
          xhr.open('GET', self.dataItem.baseURL + ThumbnailSuffix, true);
          xhr.responseType = 'arraybuffer';
          xhr.send(null);
          xhr.addEventListener('load', function(e) {
            if (xhr.status === 200) {
              chrome.wallpaperPrivate.saveThumbnail(self.dataItem.baseURL,
                                                    xhr.response);
              var blob = new Blob([new Int8Array(xhr.response)],
                                  {'type' : 'image\/png'});
              imageEl.src = window.URL.createObjectURL(blob);
              // TODO(bshe): We currently use empty div to reserve space for
              // thumbnail. Use a placeholder like "loading" image may better.
              imageEl.addEventListener('load', function(e) {
                window.URL.revokeObjectURL(this.src);
              });
            }
          });
        }
      });
    },
  };

  /**
   * Creates a selection controller that wraps selection on grid ends
   * and translates Enter presses into 'activate' events.
   * @param {cr.ui.ListSelectionModel} selectionModel The selection model to
   *     interact with.
   * @param {cr.ui.Grid} grid The grid to interact with.
   * @constructor
   * @extends {cr.ui.GridSelectionController}
   */
  function WallpaperThumbnailsGridSelectionController(selectionModel, grid) {
    GridSelectionController.call(this, selectionModel, grid);
  }

  WallpaperThumbnailsGridSelectionController.prototype = {
    __proto__: GridSelectionController.prototype,

    /** @override */
    getIndexBefore: function(index) {
      var result =
          GridSelectionController.prototype.getIndexBefore.call(this, index);
      return result == -1 ? this.getLastIndex() : result;
    },

    /** @override */
    getIndexAfter: function(index) {
      var result =
          GridSelectionController.prototype.getIndexAfter.call(this, index);
      return result == -1 ? this.getFirstIndex() : result;
    },

    /** @override */
    handleKeyDown: function(e) {
      if (e.keyIdentifier == 'Enter')
        cr.dispatchSimpleEvent(this.grid_, 'activate');
      else
        GridSelectionController.prototype.handleKeyDown.call(this, e);
    },
  };

  /**
   * Creates a new user images grid element.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {cr.ui.Grid}
   */
  var WallpaperThumbnailsGrid = cr.ui.define('grid');

  WallpaperThumbnailsGrid.prototype = {
    __proto__: Grid.prototype,

    /** @override */
    createSelectionController: function(sm) {
      return new WallpaperThumbnailsGridSelectionController(sm, this);
    },

    /** @override */
    decorate: function() {
      Grid.prototype.decorate.call(this);
      this.dataModel = new ArrayDataModel([]);
      this.itemConstructor = WallpaperThumbnailsGridItem;
      this.selectionModel = new ListSingleSelectionModel();
      this.inProgramSelection_ = false;
    },

    /**
     * Should only be queried from the 'change' event listener, true if the
     * change event was triggered by a programmatical selection change.
     * @type {boolean}
     */
    get inProgramSelection() {
      return this.inProgramSelection_;
    },

    /**
     * Set index to the image selected.
     * @type {number} index The index of selected image.
     */
    set selectedItemIndex(index) {
      this.inProgramSelection_ = true;
      this.selectionModel.selectedIndex = index;
      this.inProgramSelection_ = false;
    },

    /**
     * The selected item.
     * @type {!Object} Wallpaper information inserted into the data model.
     */
    get selectedItem() {
      var index = this.selectionModel.selectedIndex;
      return index != -1 ? this.dataModel.item(index) : null;
    },
    set selectedItem(selectedItem) {
      var index = this.dataModel.indexOf(selectedItem);
      this.inProgramSelection_ = true;
      this.selectionModel.leadIndex = index;
      this.selectionModel.selectedIndex = index;
      this.inProgramSelection_ = false;
    },

    /**
     * Forces re-display, size re-calculation and focuses grid.
     */
    updateAndFocus: function() {
      // Recalculate the measured item size.
      this.measured_ = null;
      this.columns = 0;
      this.redraw();
      this.focus();
    }
  };

  return {
    WallpaperThumbnailsGrid: WallpaperThumbnailsGrid
  };
});

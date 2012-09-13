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
   *          author: string, authorWebsite: string}}
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

    /** @inheritDoc */
    decorate: function() {
      GridItem.prototype.decorate.call(this);
      var imageEl = cr.doc.createElement('img');
      // Thumbnail
      imageEl.src = this.dataItem.baseURL + ThumbnailSuffix;
      // Remove any garbage added by GridItem and ListItem decorators.
      this.textContent = '';
      this.appendChild(imageEl);
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

    /** @inheritDoc */
    getIndexBefore: function(index) {
      var result =
          GridSelectionController.prototype.getIndexBefore.call(this, index);
      return result == -1 ? this.getLastIndex() : result;
    },

    /** @inheritDoc */
    getIndexAfter: function(index) {
      var result =
          GridSelectionController.prototype.getIndexAfter.call(this, index);
      return result == -1 ? this.getFirstIndex() : result;
    },

    /** @inheritDoc */
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

    /** @inheritDoc */
    createSelectionController: function(sm) {
      return new WallpaperThumbnailsGridSelectionController(sm, this);
    },

    /** @inheritDoc */
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

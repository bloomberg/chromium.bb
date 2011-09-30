// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const ArrayDataModel = cr.ui.ArrayDataModel;
  const Grid = cr.ui.Grid;
  const GridItem = cr.ui.GridItem;
  const GridSelectionController = cr.ui.GridSelectionController;
  const ListSingleSelectionModel = cr.ui.ListSingleSelectionModel;

  /**
   * Creates a new user images grid item.
   * @param {{url: string, title: string=, clickHandler: function=}} imageInfo
   *     User image URL, title and optional click handler.
   * @constructor
   * @extends {cr.ui.GridItem}
   */
  function UserImagesGridItem(imageInfo) {
    var el = new GridItem(imageInfo);
    el.__proto__ = UserImagesGridItem.prototype;
    return el;
  }

  UserImagesGridItem.prototype = {
    __proto__: GridItem.prototype,

    /** @inheritDoc */
    decorate: function() {
      GridItem.prototype.decorate.call(this);
      var imageEl = cr.doc.createElement('img');
      imageEl.src = this.dataItem.url;
      imageEl.title = this.dataItem.title || '';
      imageEl.setAttribute(
          'aria-label',
          imageEl.src.replace(/(.*\/|\.png)/g, '').replace(/_/g, ' '));
      if (typeof this.dataItem.clickHandler == 'function')
        imageEl.addEventListener('click', this.dataItem.clickHandler);
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
  function UserImagesGridSelectionController(selectionModel, grid) {
    GridSelectionController.call(this, selectionModel, grid);
  }

  UserImagesGridSelectionController.prototype = {
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
  var UserImagesGrid = cr.ui.define('grid');

  UserImagesGrid.prototype = {
    __proto__: Grid.prototype,

    /** @inheritDoc */
    createSelectionController: function(sm) {
      return new UserImagesGridSelectionController(sm, this);
    },

    /** @inheritDoc */
    decorate: function() {
      Grid.prototype.decorate.call(this);
      this.dataModel = new ArrayDataModel([]);
      this.itemConstructor = UserImagesGridItem;
      this.selectionModel = new ListSingleSelectionModel();
    },

    /**
     * URL of the image selected.
     * @type {string?}
     */
    get selectedItemUrl() {
      var selectedItem = this.selectedItem;
      return selectedItem ? selectedItem.url : null;
    },
    set selectedItemUrl(url) {
      for (var i = 0, el; el = this.dataModel.item(i); i++) {
        if (el.url === url) {
          this.selectionModel.selectedIndex = i;
          this.selectionModel.leadIndex = i;
        }
      }
    },

    /** @inheritDoc */
    get selectedItem() {
      var index = this.selectionModel.selectedIndex;
      return index != -1 ? this.dataModel.item(index) : null;
    },
    set selectedItem(selectedItem) {
      var index = this.dataModel.indexOf(selectedItem);
      this.selectionModel.selectedIndex = index;
      this.selectionModel.leadIndex = index;
    },

    /**
     * Adds new image to the user image grid.
     * @param {string} src Image URL.
     * @param {string=} opt_title Image tooltip.
     * @param {function=} opt_clickHandler Image click handler.
     * @param {number=} opt_position If given, inserts new image into
     *     that position (0-based) in image list.
     * @return {!Object} Image data inserted into the data model.
     */
    addItem: function(url, opt_title, opt_clickHandler, opt_position) {
      var imageInfo = {
        url: url,
        title: opt_title,
        clickHandler: opt_clickHandler
      };
      if (opt_position)
        this.dataModel.splice(opt_position, 0, imageInfo);
      else
        this.dataModel.push(imageInfo);
      return imageInfo;
    },

    /**
     * Replaces an image in the grid.
     * @param {Object} imageInfo Image data returned from addItem() call.
     * @param {string} imageUrl New image URL.
     * @return {!Object} Image data of the added or updated image.
     */
    updateItem: function(imageInfo, imageUrl) {
      var imageIndex = this.dataModel.indexOf(imageInfo);
      this.removeItem(imageInfo);
      return this.addItem(
          imageUrl,
          imageInfo.title,
          imageInfo.clickHandler,
          imageIndex);
    },

    /**
     * Removes previously added image from the grid.
     * @param {Object} imageInfo Image data returned from the addItem() call.
     */
    removeItem: function(imageInfo) {
      var index = this.dataModel.indexOf(imageInfo);
      if (index != -1)
        this.dataModel.splice(index, 1);
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

  /**
   * URLs of special button images.
   * @enum {string}
   */
  UserImagesGrid.ButtonImages = {
    TAKE_PHOTO: 'chrome://theme/IDR_BUTTON_USER_IMAGE_TAKE_PHOTO',
    CHOOSE_FILE: 'chrome://theme/IDR_BUTTON_USER_IMAGE_CHOOSE_FILE',
    PROFILE_PICTURE: 'chrome://theme/IDR_INCOGNITO_GUY'
  };

  return {
    UserImagesGrid: UserImagesGrid
  };
});

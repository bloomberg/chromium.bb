// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;
  /** @const */ var Grid = cr.ui.Grid;
  /** @const */ var GridItem = cr.ui.GridItem;
  /** @const */ var GridSelectionController = cr.ui.GridSelectionController;
  /** @const */ var ListSingleSelectionModel = cr.ui.ListSingleSelectionModel;

  /**
   * Creates a new user images grid item.
   * @param {{url: string, title: string=, decorateFn: function=,
   *     clickHandler: function=}} imageInfo User image URL, optional title,
   *     decorator callback and click handler.
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
      var label = imageEl.src.replace(/(.*\/|\.png)/g, '');
      imageEl.setAttribute('aria-label', label.replace(/_/g, ' '));
      if (typeof this.dataItem.clickHandler == 'function')
        imageEl.addEventListener('click', this.dataItem.clickHandler);
      // Remove any garbage added by GridItem and ListItem decorators.
      this.textContent = '';
      this.appendChild(imageEl);
      if (typeof this.dataItem.decorateFn == 'function')
        this.dataItem.decorateFn(this);
    }
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
     * URL of the image selected.
     * @type {string?}
     */
    get selectedItemUrl() {
      var selectedItem = this.selectedItem;
      return selectedItem ? selectedItem.url : null;
    },
    set selectedItemUrl(url) {
      for (var i = 0, el; el = this.dataModel.item(i); i++) {
        if (el.url === url)
          this.selectedItemIndex = i;
      }
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

    /** @inheritDoc */
    get selectedItem() {
      var index = this.selectionModel.selectedIndex;
      return index != -1 ? this.dataModel.item(index) : null;
    },
    set selectedItem(selectedItem) {
      var index = this.indexOf(selectedItem);
      this.inProgramSelection_ = true;
      this.selectionModel.selectedIndex = index;
      this.selectionModel.leadIndex = index;
      this.inProgramSelection_ = false;
    },

    /**
     * Adds new image to the user image grid.
     * @param {string} src Image URL.
     * @param {string=} opt_title Image tooltip.
     * @param {function=} opt_clickHandler Image click handler.
     * @param {number=} opt_position If given, inserts new image into
     *     that position (0-based) in image list.
     * @param {function=} opt_decorateFn Function called with the list element
     *     as argument to do any final decoration.
     * @return {!Object} Image data inserted into the data model.
     */
    // TODO(ivankr): this function needs some argument list refactoring.
    addItem: function(url, opt_title, opt_clickHandler, opt_position,
                      opt_decorateFn) {
      var imageInfo = {
        url: url,
        title: opt_title,
        clickHandler: opt_clickHandler,
        decorateFn: opt_decorateFn
      };
      this.inProgramSelection_ = true;
      if (opt_position !== undefined)
        this.dataModel.splice(opt_position, 0, imageInfo);
      else
        this.dataModel.push(imageInfo);
      this.inProgramSelection_ = false;
      return imageInfo;
    },

    /**
     * Returns index of an image in grid.
     * @param {Object} imageInfo Image data returned from addItem() call.
     * @return {number} Image index (0-based) or -1 if image was not found.
     */
    indexOf: function(imageInfo) {
      return this.dataModel.indexOf(imageInfo);
    },

    /**
     * Replaces an image in the grid.
     * @param {Object} imageInfo Image data returned from addItem() call.
     * @param {string} imageUrl New image URL.
     * @param {string=} opt_title New image tooltip (if undefined, tooltip
     *     is left unchanged).
     * @return {!Object} Image data of the added or updated image.
     */
    updateItem: function(imageInfo, imageUrl, opt_title) {
      var imageIndex = this.indexOf(imageInfo);
      var wasSelected = this.selectionModel.selectedIndex == imageIndex;
      this.removeItem(imageInfo);
      var newInfo = this.addItem(
          imageUrl,
          opt_title === undefined ? imageInfo.title : opt_title,
          imageInfo.clickHandler,
          imageIndex,
          imageInfo.decorateFn);
      if (wasSelected)
        this.selectedItem = newInfo;
      return newInfo;
    },

    /**
     * Removes previously added image from the grid.
     * @param {Object} imageInfo Image data returned from the addItem() call.
     */
    removeItem: function(imageInfo) {
      var index = this.indexOf(imageInfo);
      if (index != -1) {
        this.inProgramSelection_ = true;
        this.dataModel.splice(index, 1);
        this.inProgramSelection_ = false;
      }
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
    PROFILE_PICTURE: 'chrome://theme/IDR_PROFILE_PICTURE_LOADING'
  };

  return {
    UserImagesGrid: UserImagesGrid
  };
});

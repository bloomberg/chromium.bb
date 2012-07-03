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
   * Dimensions for camera capture.
   * @const
   */
  var CAPTURE_SIZE = {
    height: 480,
    width: 480
  };

  /**
   * Interval between consecutive camera presence checks in msec while camera is
   * not present.
   * @const
   */
  var CAMERA_CHECK_INTERVAL_MS = 3000;

  /**
   * Interval between consecutive camera liveness checks in msec.
   * @const
   */
  var CAMERA_LIVENESS_CHECK_MS = 1000;

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
        imageEl.addEventListener('mousedown', this.dataItem.clickHandler);
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
    }
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
      this.addEventListener('dblclick', this.handleDblClick_.bind(this));
      this.addEventListener('change', this.handleChange_.bind(this));
    },

    /**
     * Handles double click on the image grid.
     * @param {Event} e Double click Event.
     * @private
     */
    handleDblClick_: function(e) {
      // If a child element is double-clicked and not the grid itself, handle
      // this as 'Enter' keypress.
      if (e.target != this)
        cr.dispatchSimpleEvent(this, 'activate');
    },

    /**
     * Handles selection change.
     * @param {Event} e Double click Event.
     * @private
     */
    handleChange_: function(e) {
      if (this.selectedItem === null)
        return;

      // Update current selection type.
      this.selectionType = this.selectedItem.type;

      // Show grey silhouette with the same border as stock images.
      if (/^chrome:\/\/theme\//.test(this.selectedItemUrl))
        this.previewElement.classList.add('default-image');

      this.updatePreview_();

      cr.dispatchSimpleEvent(this, 'select');
    },

    /**
     * Updates the preview image, if present.
     * @private
     */
    updatePreview_: function() {
      var url = this.selectedItemUrl;
      if (url && this.previewImage_)
        this.previewImage_.src = url;
    },

    /**
     * @param {boolean} present Whether a camera is present or not.
     */
    get cameraPresent() {
      return this.cameraPresent_;
    },
    set cameraPresent(value) {
      this.cameraPresent_ = value;
      if (this.cameraLive)
        this.cameraImage = null;
    },

    /**
     * Start camera presence check.
     * @param {boolean} autoplay Whether to start capture immediately.
     * @param {boolean} preselect Whether to select camera automatically.
     */
    checkCameraPresence: function(autoplay, preselect) {
      this.previewElement.classList.remove('online');
      if (!this.cameraVideo_)
        return;
      navigator.webkitGetUserMedia(
          {video: true},
          this.handleCameraAvailable_.bind(this, autoplay, preselect),
          // When ready to capture camera, poll regularly for camera presence.
          this.handleCameraAbsent_.bind(this, /* recheck= */ autoplay));
    },

    /**
     * Stops camera capture, if it's currently active.
     */
    stopCamera: function() {
      if (this.cameraLiveCheckTimer_) {
        clearInterval(this.cameraLiveCheckTimer_);
        this.cameraLiveCheckTimer_ = null;
      }
      if (this.cameraVideo_)
        this.cameraVideo_.src = '';
    },

    /**
     * Handles successful camera check.
     * @param {boolean} autoplay Whether to start capture immediately.
     * @param {boolean} preselect Whether to select camera automatically.
     * @param {MediaStream} stream Stream object as returned by getUserMedia.
     * @private
     */
    handleCameraAvailable_: function(autoplay, preselect, stream) {
      if (autoplay)
        this.cameraVideo_.src = window.webkitURL.createObjectURL(stream);
      this.cameraPresent = true;
      if (preselect)
        this.selectedItem = this.cameraImage;
    },

    /**
     * Handles camera check failure.
     * @param {boolean} recheck Whether to check for camera again.
     * @param {NavigatorUserMediaError=} err Error object.
     * @private
     */
    handleCameraAbsent_: function(recheck, err) {
      this.cameraPresent = false;
      this.previewElement.classList.remove('online');
      // |preselect| is |false| in this case to not override user's selection.
      if (recheck) {
        setTimeout(this.checkCameraPresence.bind(this, true, false),
                   CAMERA_CHECK_INTERVAL_MS);
      }
      if (this.cameraLiveCheckTimer_) {
        clearInterval(this.cameraLiveCheckTimer_);
        this.cameraLiveCheckTimer_ = null;
      }
    },

    /**
     * Handles successful camera capture start.
     * @private
     */
    handleVideoStarted_: function() {
      this.previewElement.classList.add('online');
      this.cameraLiveCheckTimer_ = setInterval(this.checkCameraLive_.bind(this),
                                               CAMERA_LIVENESS_CHECK_MS);
      this.handleVideoUpdate_();
    },

    /**
     * Handles camera stream update. Called regularly (at rate no greater then
     * 4/sec) while camera stream is live.
     * @private
     */
    handleVideoUpdate_: function() {
      this.lastFrameTime_ = new Date().getTime();
    },

    /**
     * Checks if camera is still live by comparing the timestamp of the last
     * 'timeupdate' event with the current time.
     * @private
     */
    checkCameraLive_: function() {
      if (new Date().getTime() - this.lastFrameTime_ > CAMERA_LIVENESS_CHECK_MS)
        this.handleCameraAbsent_(true, null);
    },

    /**
     * Type of the selected image (one of 'default', 'profile', 'camera').
     * Setting it will update class list of |previewElement|.
     * @type {string}
     */
    get selectionType() {
      return this.selectionType_;
    },
    set selectionType(value) {
      this.selectionType_ = value;
      var previewClassList = this.previewElement.classList;
      previewClassList[value == 'default' ? 'add' : 'remove']('default-image');
      previewClassList[value == 'profile' ? 'add' : 'remove']('profile-image');
      previewClassList[value == 'camera' ? 'add' : 'remove']('camera');
    },

    /**
     * Current image captured from camera as data URL. Setting to null will
     * return to the live camera stream.
     * @type {string=}
     */
    get cameraImage() {
      return this.cameraImage_;
    },
    set cameraImage(imageUrl) {
      this.cameraLive = !imageUrl;
      if (this.cameraPresent && !imageUrl)
        imageUrl = UserImagesGrid.ButtonImages.TAKE_PHOTO;
      if (imageUrl) {
        this.cameraImage_ = this.cameraImage_ ?
            this.updateItem(this.cameraImage_, imageUrl) :
            this.addItem(imageUrl, this.cameraTitle_, undefined, 0);
        this.cameraImage_.type = 'camera';
      } else {
        this.removeItem(this.cameraImage_);
        this.cameraImage_ = null;
      }
      this.focus();
    },

    /**
     * Title for the camera element. Must be set before setting |cameraImage|
     * for the first time.
     * @type {string}
     */
    set cameraTitle(value) {
      return this.cameraTitle_ = value;
    },

    /**
     * True when camera is in live mode (i.e. no still photo selected).
     * @type {boolean}
     */
    get cameraLive() {
      return this.cameraLive_;
    },
    set cameraLive(value) {
      this.cameraLive_ = value;
      this.previewElement.classList[value ? 'add' : 'remove']('live');
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
     * Element containing the preview image (the first IMG element) and the
     * camera live stream (the first VIDEO element).
     * @type {HTMLElement}
     */
    get previewElement() {
      // TODO(ivankr): temporary hack for non-HTML5 version.
      return this.previewElement_ || this;
    },
    set previewElement(value) {
      this.previewElement_ = value;
      this.previewImage_ = value.querySelector('img');
      this.cameraVideo_ = value.querySelector('video');
      this.cameraVideo_.addEventListener('canplay',
                                         this.handleVideoStarted_.bind(this));
      this.cameraVideo_.addEventListener('timeupdate',
                                         this.handleVideoUpdate_.bind(this));
      this.updatePreview_();
    },

    /**
     * Whether the camera live stream and photo should be flipped horizontally.
     * @type {boolean}
     */
    get flipPhoto() {
      return this.flipPhoto_ || false;
    },
    set flipPhoto(value) {
      this.flipPhoto_ = value;
      this.previewElement.classList[value ? 'add' : 'remove']('flip-x');
    },

    /**
     * Performs photo capture from the live camera stream.
     * @param {function=} opt_callback Callback that receives taken photo as
     *     data URL.
     */
    takePhoto: function(opt_callback) {
      var self = this;
      var photoURL = this.captureFrame_(this.cameraVideo_, CAPTURE_SIZE);
      if (opt_callback && typeof opt_callback == 'function')
        opt_callback(photoURL);
      // Wait until image is loaded before displaying it.
      var previewImg = new Image();
      previewImg.addEventListener('load', function(e) {
        self.cameraImage = this.src;
      });
      previewImg.src = photoURL;
    },

    /**
     * Discard current photo and return to the live camera stream.
     */
    discardPhoto: function() {
      this.cameraImage = null;
    },

    /**
     * Capture a single still frame from a <video> element.
     * @param {HTMLVideoElement} video Video element to capture from.
     * @param {{width: number, height: number}} destSize Capture size.
     * @return {string} Captured frame as a data URL.
     * @private
     */
    captureFrame_: function(video, destSize) {
      var canvas = document.createElement('canvas');
      canvas.width = destSize.width;
      canvas.height = destSize.height;
      var ctx = canvas.getContext('2d');
      var width = video.videoWidth;
      var height = video.videoHeight;
      if (width < destSize.width || height < destSize.height) {
        console.error('Video capture size too small: ' +
                      width + 'x' + height + '!');
      }
      var src = {};
      if (width / destSize.width > height / destSize.height) {
        // Full height, crop left/right.
        src.height = height;
        src.width = height * destSize.width / destSize.height;
      } else {
        // Full width, crop top/bottom.
        src.width = width;
        src.height = width * destSize.height / destSize.width;
      }
      src.x = (width - src.width) / 2;
      src.y = (height - src.height) / 2;
      if (this.flipPhoto) {
        ctx.translate(canvas.width, 0);
        ctx.scale(-1.0, 1.0);
      }
      ctx.drawImage(video, src.x, src.y, src.width, src.height,
                    0, 0, destSize.width, destSize.height);
      return canvas.toDataURL('image/png');
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
      // Update image data with the reset of the keys from the old data.
      for (k in imageInfo) {
        if (!(k in newInfo))
          newInfo[k] = imageInfo[k];
      }
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
        var wasSelected = this.selectionModel.selectedIndex == index;
        this.inProgramSelection_ = true;
        this.dataModel.splice(index, 1);
        if (wasSelected) {
          // If item removed was selected, select the item next to it.
          this.selectedItem = this.dataModel.item(
              Math.min(this.dataModel.length - 1, index));
        }
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

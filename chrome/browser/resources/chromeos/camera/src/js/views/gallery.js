// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var camera = camera || {};

/**
 * Namespace for views.
 */
camera.views = camera.views || {};

/**
 * Creates the Gallery view controller.
 *
 * @param {camera.View.Context} context Context object.
 * @extends {camera.View}
 * @implements {camera.models.Gallery.Observer}
 * @constructor
 */
camera.views.Gallery = function(context) {
  camera.View.call(this, context);

  /**
   * @type {camera.models.Gallery}
   * @private
   */
  this.model_ = null;

  /**
   * Contains pictures' views.
   * @type {Array.<camera.views.Gallery.DOMPicture>}
   * @private
   */
  this.pictures_ = [];

  /**
   * @type {camera.util.SmoothScroller}
   * @private
   */
  this.scroller_ = new camera.util.SmoothScroller(
      document.querySelector('#gallery'));

  // End of properties, seal the object.
  Object.seal(this);
};

/**
 * Represents a picture attached to the DOM by combining the picture data
 * object with the DOM element.
 *
 * @param {camera.models.Gallery.Picture} picture Picture data.
 * @param {HTMLElement} element DOM element holding the picture.
 * @constructor
 */
camera.views.Gallery.DOMPicture = function(picture, element) {
  /**
   * @type {camera.models.Gallery.Picture}
   * @private
   */
  this.picture_ = picture;

  /**
   * @type {HTMLElement}
   * @private
   */
  this.element_ = element;

  // End of properties. Freeze the object.
  Object.freeze(this);
};

camera.views.Gallery.DOMPicture.prototype = {
  get picture() {
    return this.picture_;
  },
  get element() {
    return this.element_;
  }
};

camera.views.Gallery.prototype = {
  __proto__: camera.View.prototype
};

/**
 * @override
 */
camera.views.Gallery.prototype.initialize = function(callback) {
  camera.models.Gallery.getInstance(function(model) {
    this.model_ = model;
    this.model_.addObserver(this);
    this.renderPictures_();
    callback();
  }.bind(this), function() {
    // TODO(mtomasz): Add error handling.
    console.error('Unable to initialize the file system.');
    callback();
  });
};

/**
 * Renders pictures from the model onto the DOM.
 * @private
 */
camera.views.Gallery.prototype.renderPictures_ = function() {
  for (var index = 0; index < this.model_.length; index++) {
    this.addPictureToDOM_(this.model_.pictures[index]);
  }
}

/**
 * Enters the view.
 * @override
 */
camera.views.Gallery.prototype.onEnter = function() {
  this.onResize();
  document.body.classList.add('gallery');
};

/**
 * Leaves the view.
 * @override
 */
camera.views.Gallery.prototype.onLeave = function() {
  document.body.classList.remove('gallery');
};

/**
 * Returns a picture view by index.
 *
 * @param {number} index Index of the picture.
 * @return {camera.views.Gallery.Picture}
 * @private
 */
camera.views.Gallery.prototype.pictureByIndex_ = function(index) {
  return this.pictures_[index];
};

/**
 * Returns the currently selected picture view.
 *
 * @return {camera.views.Gallery.Picture}
 * @private
 */
camera.views.Gallery.prototype.currentPicture_ = function() {
  return this.pictureByIndex_(this.model_.currentIndex);
};

/**
 * @override
 */
camera.views.Gallery.prototype.onCurrentIndexChanged = function(
    oldIndex, newIndex) {
  if (oldIndex !== null && oldIndex < this.model_.length)
    this.pictureByIndex_(oldIndex).element.classList.remove('selected');
  if (newIndex !== null && newIndex < this.model_.length)
    this.pictureByIndex_(newIndex).element.classList.add('selected');

  camera.util.ensureVisible(this.currentPicture_().element,
                            this.scroller_);
};

/**
 * @override
 */
camera.views.Gallery.prototype.onPictureDeleting = function(index) {
  this.pictures_[index].element.parentNode.removeChild(
    this.pictures_[index].element);
  this.pictures_.splice(index, 1);
};

/**
 * Saves the picture with the specified index to the external storage.
 * @private
 */
camera.views.Gallery.prototype.exportPicture_ = function() {
  var accepts = [{
    description: '*.jpg',
    extensions: ['jpg", "jpeg'],
    mimeTypes: ['image/jpeg']
  }];

  var fileName = this.currentPicture_().picture.imageEntry.name;

  var onError = function() {
    // TODO(mtomasz): Check if it works.
    this.context_.onError(
        'gallery-export-error',
        chrome.i18n.getMessage('errorMsgGalleryExportFailed'));
  }.bind(this);

  chrome.fileSystem.chooseEntry({
    type: 'saveFile',
    suggestedName: fileName,
    accepts: accepts
  }, function(fileEntry) {
      if (!fileEntry)
        return;
      this.model_.exportPicture(fileName,
                fileEntry,
                function() {},
                onError);
  }.bind(this));
};

/**
 * @override
 */
camera.views.Gallery.prototype.onKeyPressed = function(event) {
  var currentPicture = this.currentPicture_();
  switch (event.keyIdentifier) {
    case 'Right':
      var index = (this.model_.currentIndex + this.model_.length - 1) %
          this.model_.length;
      this.model_.currentIndex = index;
      break;
    case 'Left':
      var index = (this.model_.currentIndex + this.model_.length + 1) %
          this.model_.length;
      this.model_.currentIndex = index;
      break;
    case 'Down':
      for (var offset = 1; offset < this.model_.length; offset++) {
        var index = (this.model_.currentIndex + this.model_.length - offset) %
            this.model_.length;
        if (currentPicture.element.offsetLeft ==
            this.pictures_[index].element.offsetLeft) {
          this.model_.currentIndex = index;
          break;
        }
      }
      event.preventDefault();
      break;
    case 'Up':
      for (var offset = 1; offset < this.model_.length; offset++) {
        var index = (this.model_.currentIndex + this.model_.length + offset) %
            this.model_.length;
        if (currentPicture.element.offsetLeft ==
            this.pictures_[index].element.offsetLeft) {
          this.model_.currentIndex = index;
          break;
        }
      }
      event.preventDefault();
      break;
    case 'End':
      this.model_.currentIndex = 0;
      break;
    case 'Home':
      this.model_.currentIndex = this.model_.length - 1;
      break;
    case 'U+007F':
      this.model_.deletePicture(currentPicture.picture,
                function() {},
                function() {
                  // TODO(mtomasz): Handle errors.
                });
      break;
    case 'Enter':
      this.exportPicture_();
      break;
  }
};

/**
 * @override
 */
camera.views.Gallery.prototype.onPictureAdded = function(index) {
  this.addPictureToDOM_(this.model_.pictures[index]);
};

/**
 * Adds a picture represented by a model to DOM, and stores in the internal
 * mapping hash array (model -> view).
 *
 * @param {camera.model.Gallery.Picture} picture
 * @private
 */
camera.views.Gallery.prototype.addPictureToDOM_ = function(picture) {
  var gallery = document.querySelector('#gallery .padder');
  var img = document.createElement('img');
  img.src = picture.thumbnailURL;
  gallery.insertBefore(img, gallery.firstChild);

  // Add to the collection.
  this.pictures_.push(new camera.views.Gallery.DOMPicture(picture, img));

  // Add handlers.
  img.addEventListener('click', function() {
    // TODO(mtomasz): Implement exporting and zoom-in.
    this.model_.currentIndex = this.model_.pictures.indexOf(picture);
  }.bind(this));
};


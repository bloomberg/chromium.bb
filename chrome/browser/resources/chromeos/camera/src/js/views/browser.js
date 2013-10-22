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
 * Creates the Browser view controller.
 *
 * @param {camera.View.Context} context Context object.
 * @param {camera.Router} router View router to switch views.
 * @extends {camera.View}
 * @implements {camera.models.Gallery.Observer}
 * @constructor
 */
camera.views.Browser = function(context, router) {
  camera.View.call(this, context, router);

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
      document.querySelector('#browser'),
      document.querySelector('#browser .padder'));

  /**
   * @type {camera.HorizontalScrollBar}
   * @private
   */
  this.scrollBar_ = new camera.HorizontalScrollBar(this.scroller_);

  /**
   * Monitores when scrolling is ended.
   * @type {camera.util.ScrollTracker}
   * @private
   */
  this.scrollTracker_ = new camera.util.ScrollTracker(
      this.scroller_,
      function() {},  // onScrollStarted
      this.onScrollEnded_.bind(this));

  // End of properties, seal the object.
  Object.seal(this);

  // Register clicking on the background to close the browser view.
  document.querySelector('#browser').addEventListener('click',
      this.onBackgroundClicked_.bind(this));

  // Listen for clicking on the browser buttons.
  document.querySelector('#browser-print').addEventListener(
      'click', this.onPrintButtonClicked_.bind(this));
  document.querySelector('#browser-export').addEventListener(
      'click', this.onExportButtonClicked_.bind(this));
};

camera.views.Browser.prototype = {
  __proto__: camera.View.prototype
};

/**
 * @override
 */
camera.views.Browser.prototype.initialize = function(callback) {
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
camera.views.Browser.prototype.renderPictures_ = function() {
  for (var index = 0; index < this.model_.length; index++) {
    this.addPictureToDOM_(this.model_.pictures[index]);
  }
};

/**
 * Enters the view.
 * @override
 */
camera.views.Browser.prototype.onEnter = function() {
  this.onResize();
  this.scrollTracker_.start();
  this.updateButtons_();
  document.body.classList.add('browser');
};

/**
 * Leaves the view.
 * @override
 */
camera.views.Browser.prototype.onLeave = function() {
  this.scrollTracker_.stop();
  document.body.classList.remove('browser');
};

/**
 * @override
 */
camera.views.Browser.prototype.onResize = function() {
  if (this.currentPicture_()) {
    camera.util.scrollToCenter(this.currentPicture_().element,
                               this.scroller_,
                               camera.util.SmoothScroller.Mode.INSTANT);
  }
};

/**
 * Handles clicking (or touching) on the background.
 * @param {Event} event Click event.
 * @private
 */
camera.views.Browser.prototype.onBackgroundClicked_ = function(event) {
  this.router.switchView(camera.Router.ViewIdentifier.GALLERY);
};

/**
 * Handles clicking on the print button.
 * @param {Event} event Click event.
 * @private
 */
camera.views.Browser.prototype.onPrintButtonClicked_ = function(event) {
  window.print();
};

/**
 * Handles clicking on the export button.
 * @param {Event} event Click event.
 * @private
 */
camera.views.Browser.prototype.onExportButtonClicked_ = function(event) {
  this.exportSelection_();
};


/**
 * Handles ending of scrolling.
 * @private
 */
camera.views.Browser.prototype.onScrollEnded_ = function() {
  var center = this.scroller_.scrollLeft + this.scroller_.clientWidth / 2;

  // Find the closest picture.
  var minDistance = -1;
  var minIndex = -1;
  for (var index = 0; index < this.model_.length; index++) {
    var element = this.pictures_[index].element;
    var distance = Math.abs(
        element.offsetLeft + element.offsetWidth / 2 - center);
    if (minIndex == -1 || distance < minDistance) {
      minDistance = distance;
      minIndex = index;
    }
  }

  // Select the closest picture to the center of the window.
  if (minIndex != -1)
    this.model_.currentIndex = minIndex;

  this.updatePicturesResolutions_();
};

/**
 * Updates visibility of the browser buttons.
 * @private
 */
camera.views.Browser.prototype.updateButtons_ = function() {
  var pictureSelected = this.model_.currentIndex !== null;
  if (pictureSelected) {
    document.querySelector('#browser-print').removeAttribute('disabled');
    document.querySelector('#browser-export').removeAttribute('disabled');
  } else {
    document.querySelector('#browser-print').setAttribute('disabled', '');
    document.querySelector('#browser-export').setAttribute('disabled', '');
  }
};

/**
 * Updates resolutions of the pictures. The selected picture will be high
 * resolution, and all the rest low. This method waits until CSS transitions
 * are finished (if any).
 *
 * @private
 */
camera.views.Browser.prototype.updatePicturesResolutions_ = function() {
 var updateResolutions = function() {
    for (var index = 0; index < this.pictures_.length; index++) {
      this.pictures_[index].displayResolution =
          (index == this.model_.currentIndex) ?
              camera.views.Gallery.DOMPicture.DisplayResolution.HIGH :
              camera.views.Gallery.DOMPicture.DisplayResolution.LOW;
    }
  }.bind(this);

  // Wait until the zoom-in transition is finished, and then update pictures'
  // resolutions.
  if (this.model_.currentIndex !== null) {
    camera.util.waitForTransitionCompletion(
        this.pictures_[this.model_.currentIndex].element,
        250,  // Timeout in ms.
        updateResolutions);
  } else {
    // No selection.
    updateResolutions();
  }
};

/**
 * Returns a picture view by index.
 *
 * @param {number} index Index of the picture.
 * @return {camera.views.Gallery.Picture}
 * @private
 */
camera.views.Browser.prototype.pictureByIndex_ = function(index) {
  return this.pictures_[index];
};

/**
 * Returns the currently selected picture view.
 *
 * @return {camera.views.Gallery.Picture}
 * @private
 */
camera.views.Browser.prototype.currentPicture_ = function() {
  return this.pictureByIndex_(this.model_.currentIndex);
};

/**
 * @override
 */
camera.views.Browser.prototype.onCurrentIndexChanged = function(
    oldIndex, newIndex) {
  if (oldIndex !== null && oldIndex < this.model_.length)
    this.pictureByIndex_(oldIndex).element.classList.remove('selected');
  if (newIndex !== null && newIndex < this.model_.length)
    this.pictureByIndex_(newIndex).element.classList.add('selected');

  if (newIndex !== null) {
   camera.util.scrollToCenter(this.currentPicture_().element,
                              this.scroller_);
  }

  this.updateButtons_();
};

/**
 * @override
 */
camera.views.Browser.prototype.onPictureDeleting = function(index) {
  this.pictures_[index].element.parentNode.removeChild(
    this.pictures_[index].element);
  this.pictures_.splice(index, 1);

  // Update the resolutions, since the current image might have changed
  // without scrolling and without scrolling. Use a timer, to wait until
  // the picture is deleted from the model.
  setTimeout(this.updatePicturesResolutions_.bind(this), 0);

  // TODO(mtomasz): Introduce a onPictureDeleted callback to avoid these
  // timers.
  setTimeout(this.updateButtons_.bind(this), 0);
};

/**
 * Exports the selected picture. If nothing selected, then nothing happens.
 * @private
 */
camera.views.Browser.prototype.exportSelection_ = function() {
  if (!this.currentPicture_())
    return;

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
camera.views.Browser.prototype.onKeyPressed = function(event) {
  var currentPicture = this.currentPicture_();
  switch (event.keyIdentifier) {
    case 'Right':
      this.model_.currentIndex = Math.max(0, this.model_.currentIndex - 1);
      event.preventDefault();
      break;
    case 'Left':
      this.model_.currentIndex =
          Math.min(this.model_.length - 1, this.model_.currentIndex + 1);
      event.preventDefault();
      break;
    case 'End':
      this.model_.currentIndex = 0;
      event.preventDefault();
      break;
    case 'Home':
      this.model_.currentIndex = this.model_.length - 1;
      event.preventDefault();
      break;
    case 'U+007F':
      this.model_.deletePicture(currentPicture.picture,
                function() {},
                function() {
                  // TODO(mtomasz): Handle errors.
                });
      break;
    case 'Enter':
      this.exportSelection_();
      break;
    case 'U+001B':
      this.router.switchView(camera.Router.ViewIdentifier.GALLERY);
      break;
  }
};

/**
 * @override
 */
camera.views.Browser.prototype.onPictureAdded = function(index) {
  this.addPictureToDOM_(this.model_.pictures[index]);
};

/**
 * Adds a picture represented by a model to DOM, and stores in the internal
 * mapping hash array (model -> view).
 *
 * @param {camera.model.Gallery.Picture} picture
 * @private
 */
camera.views.Browser.prototype.addPictureToDOM_ = function(picture) {
  var gallery = document.querySelector('#browser .padder');
  var boundsPadder = gallery.querySelector('.bounds-padder');
  var img = document.createElement('img');
  gallery.insertBefore(img, boundsPadder.nextSibling);

  // Add to the collection.
  this.pictures_.push(new camera.views.Gallery.DOMPicture(picture, img));

  // Add handlers.
  img.addEventListener('click', function(event) {
    // TODO(mtomasz): Implement exporting and zoom-in.
    this.model_.currentIndex = this.model_.pictures.indexOf(picture);
    event.stopPropagation();
  }.bind(this));
};


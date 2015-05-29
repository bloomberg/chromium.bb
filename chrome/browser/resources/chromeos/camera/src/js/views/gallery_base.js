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
 * Creates the Gallery Base view controller.
 *
 * @param {camera.View.Context} context Context object.
 * @param {camera.Router} router View router to switch views.
 * @param {HTMLElement} rootElement Root element containing elements of the
 *     view.
 * @param {string} name View name.
 * @extends {camera.View}
 * @implements {camera.models.Gallery.Observer}
 * @constructor
 */
camera.views.GalleryBase = function(context, router, rootElement, name) {
  camera.View.call(this, context, router, rootElement, name);

  /**
   * @type {camera.models.Gallery}
   * @protected
   */
  this.model = null;

  /**
   * Contains pictures' views.
   * @type {Array.<camera.views.GalleryBase.DOMPicture>}
   * @protected
   */
  this.pictures = [];

  /**
   * @type {boolean}
   * @private
   */
  this.forceUpcomingFocusSync_ = false;

};

/**
 * Represents a picture attached to the DOM by combining the picture data
 * object with the DOM element.
 *
 * @param {camera.models.Gallery.Picture} picture Picture data.
 * @param {HTMLImageElement} element DOM element holding the picture.
 * @constructor
 */
camera.views.GalleryBase.DOMPicture = function(picture, element) {
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

  /**
   * @type {camera.views.GalleryBase.DOMPicture.DisplayResolution}
   * @private
   */
  this.displayResolution_ =
      camera.views.GalleryBase.DOMPicture.DisplayResolution.LOW;

  // End of properties. Seal the object.
  Object.seal(this);

  // Load the image.
  this.updateImage_();
};

/**
 * Sets resolution of the picture to be displayed.
 * @enum {number}
 */
camera.views.GalleryBase.DOMPicture.DisplayResolution = {
  LOW: 0,
  HIGH: 1
};

camera.views.GalleryBase.DOMPicture.prototype = {
  get picture() {
    return this.picture_;
  },
  get element() {
    return this.element_;
  },
  set displayResolution(value) {
    if (this.displayResolution_ == value)
      return;
    this.displayResolution_ = value;
    this.updateImage_();
  },
  get displayResolution() {
    return this.displayResolution_;
  }
};

/**
 * Loads the picture into the DOM element.
 * @private
 */
camera.views.GalleryBase.DOMPicture.prototype.updateImage_ = function() {
  switch (this.displayResolution_) {
    case camera.views.GalleryBase.DOMPicture.DisplayResolution.LOW:
      this.element_.src = this.picture_.thumbnailURL;
      break;
    case camera.views.GalleryBase.DOMPicture.DisplayResolution.HIGH:
      this.element_.src = this.picture_.imageURL;
      break;
  }
};

camera.views.GalleryBase.prototype = {
  __proto__: camera.View.prototype
};

/**
 * @override
 */
camera.views.GalleryBase.prototype.initialize = function(callback) {
  camera.models.Gallery.getInstance(function(model) {
    this.model = model;
    this.model.addObserver(this);
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
camera.views.GalleryBase.prototype.renderPictures_ = function() {
  for (var index = 0; index < this.model.length; index++) {
    this.addPictureToDOM(this.model.pictures[index]);
  }
};

/**
 * Exports the selected picture. If nothing selected, then nothing happens.
 * @protected
 */
camera.views.GalleryBase.prototype.exportSelection = function() {
  if (!this.currentPicture())
    return;

  var accepts = [{
    description: '*.jpg',
    extensions: ['jpg', 'jpeg'],
    mimeTypes: ['image/jpeg']
  }];

  var picture = this.currentPicture().picture;

  var onError = function() {
    // TODO(mtomasz): Check if it works.
    this.context_.onError(
        'gallery-export-error',
        chrome.i18n.getMessage('errorMsgGalleryExportFailed'));
  }.bind(this);

  chrome.fileSystem.chooseEntry({
    type: 'saveFile',
    suggestedName: picture.imageEntry.name,
    accepts: accepts
  }, function(fileEntry) {
      if (!fileEntry)
        return;
      this.model.exportPicture(
          picture,
          fileEntry,
          function() {},
          onError);
  }.bind(this));
};

/**
 * Deletes the currently selected picture. If nothing selected, then nothing
 * happens.
 * @protected
 */
camera.views.GalleryBase.prototype.deleteSelection = function() {
  if (!this.currentPicture())
    return;

  this.router.navigate(camera.Router.ViewIdentifier.DIALOG, {
    type: camera.views.Dialog.Type.CONFIRMATION,
    message: chrome.i18n.getMessage('deleteConfirmationMsg')
  }, function(result) {
    if (!result.isPositive)
      return;
    this.model.deletePicture(this.currentPicture().picture,
        function() {
          if (this.entered && !this.currentPicture())
            this.router.back();
        }.bind(this),
        function() {
          // TODO(mtomasz): Handle errors.
        });
  }.bind(this));
};

/**
 * Returns the currently selected picture view.
 * @return {camera.views.GalleryBase.DOMPicture}
 * @protected
 */
camera.views.GalleryBase.prototype.currentPicture = function() {
  if (this.model.currentIndex === null)
    return null;

  return this.pictures[this.model.currentIndex];
};

/**
 * @override
 */
camera.views.GalleryBase.prototype.onActivate = function() {
  // Tab indexes have to be recalculated, since they might have changed while
  // the view wasn't active. Therefore, the restoring logic in the View class
  // might have restored old tabIndex values.
  for (var index = 0; index < this.pictures.length; index++) {
    this.pictures[index].element.tabIndex =
        (index == this.model.currentIndex) ? 0 : -1;
  }
};

/**
 * @override
 */
camera.views.GalleryBase.prototype.onCurrentIndexChanged = function(
    oldIndex, newIndex) {
  if (oldIndex !== null && oldIndex < this.model.length) {
    this.pictures[oldIndex].element.tabIndex = -1;
    this.pictures[oldIndex].element.classList.remove('selected');
    this.pictures[oldIndex].element.setAttribute('aria-selected', 'false');
  }

  if (newIndex !== null && newIndex < this.model.length) {
    this.pictures[newIndex].element.tabIndex = this.active ? 0 : -1;
    this.pictures[newIndex].element.classList.add('selected');
    this.pictures[newIndex].element.setAttribute('aria-selected', 'true');

    this.ariaListNode().setAttribute('aria-activedescendant',
        this.pictures[newIndex].element.id);
  }
};

/**
 * @override
 */
camera.views.GalleryBase.prototype.onPictureDeleting = function(index) {
  var element = this.pictures[index].element;
  this.pictures.splice(index, 1);

  // Hack to restore focus after removing an element. Note, that we restore
  // focus only if there was something focused before. However, if the focus
  // was on the selected element, then after removing it from DOM, there will
  // be nothing focused, while we still want to restore the focus.
  this.forceUpcomingFocusSync_ = document.activeElement == element;

  element.parentNode.removeChild(element);
};

/**
 * @override
 */
camera.views.GalleryBase.prototype.onKeyPressed = function(event) {
  var currentPicture = this.currentPicture();
  switch (camera.util.getShortcutIdentifier(event)) {
    case 'Right':
      if (this.model.length) {
        if (!currentPicture)
          this.model.currentIndex = this.model.length - 1;
        else
          this.model.currentIndex = Math.max(0, this.model.currentIndex - 1);
      }
      event.preventDefault();
      break;
    case 'Left':
      if (this.model.length) {
        if (!currentPicture) {
          this.model.currentIndex = 0;
        } else {
          this.model.currentIndex =
              Math.min(this.model.length - 1, this.model.currentIndex + 1);
        }
      }
      event.preventDefault();
      break;
     case 'End':
      if (this.model.length)
        this.model.currentIndex = 0;
      event.preventDefault();
      break;
    case 'Home':
      if (this.model.length)
        this.model.currentIndex = this.model.length - 1;
      event.preventDefault();
      break;
    case 'U+007F':  // Delete.
      this.deleteSelection();
      event.preventDefault();
      break;
    case 'U+001B':  // Escape.
      this.router.back();
      event.preventDefault();
      break;
  }
};

/**
 * @override
 */
camera.views.GalleryBase.prototype.onPictureAdded = function(index) {
  this.addPictureToDOM(this.model.pictures[index]);
};

/**
 * Adds the picture to DOM. Should be overriden by inheriting classes.
 * @param {camera.models.Gallery.Picture} picture Model of the picture to be
 *     added.
 * @protected
 */
camera.views.GalleryBase.prototype.addPictureToDOM = function(picture) {
  throw new Error('Not implemented.');
};

/**
 * Provides node for the picture list to be used to set list aria attributes.
 * @return {HTMLElement}
 * @protected
 */
camera.views.GalleryBase.prototype.ariaListNode = function() {
  throw new Error('Not implemented.');
};

/**
 * Synchronizes focus with the selection, if it was previously set on anything
 * else.
 * @protected
 */
camera.views.GalleryBase.prototype.synchronizeFocus = function() {
  // Force focusing only once, after deleting a picture. This is because, the
  // focus might be lost since the deleted picture is removed from DOM.
  var force = this.forceUpcomingFocusSync_;
  this.forceUpcomingFocusSync_ = false;

  var currentPicture = this.currentPicture();
  if (currentPicture && (document.activeElement != document.body || force) &&
      currentPicture.element.getAttribute('tabindex') !== undefined &&
      currentPicture.element.getAttribute('tabindex') != -1) {
    currentPicture.element.focus();
  }
};

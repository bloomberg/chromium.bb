// Copyright 2018 The Chromium OS Authors. All rights reserved./
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
 * Namespace for Camera view.
 */
camera.views.camera = camera.views.camera || {};

/**
 * Creates a controller for the gallery-button of Camera view.
 * @param {camera.Router} router View router to switch views.
 * @param {camera.models.Gallery} model Model object.
 * @implements {camera.models.Gallery.Observer}
 * @constructor
 */
camera.views.camera.GalleryButton = function(router, model) {
  /**
   * @type {camera.Router}
   * @private
   */
  this.router_ = router;

  /**
   * @type {camera.models.Gallery}
   * @private
   */
  this.model_ = model;

  /**
   * @type {camera.models.Gallery.Picture}
   * @private
   */
  this.lastPicture_ = null;

  /**
   * @type {HTMLButtonElement}
   * @private
   */
  this.button_ = document.querySelector('#gallery-enter');

  // End of properties, seal the object.
  Object.seal(this);

  this.button_.addEventListener('click', event => {
    // Check if the last picture still exists before opening it in the gallery.
    // TODO(yuli): Remove this workaround for unable watching changed-files.
    this.model_.checkLastPicture().then(picture => {
      this.openGallery_(picture);
    });
  });
};

camera.views.camera.GalleryButton.prototype = {
  /*
   * @param {boolean} value Whether disabled.
   */
  set disabled(value) {
    this.button_.disabled = value;
  }
};

/**
 * Updates the button for the model changes.
 * @private
 */
camera.views.camera.GalleryButton.prototype.updateButton_ = function() {
  this.model_.lastPicture().then(picture => {
    if (picture != this.lastPicture_) {
      this.lastPicture_ = picture;
      return this.button_;
    }
  }).then(button => {
    // Update the button if the last picture changes.
    if (!button) {
      return;
    }
    button.hidden = !this.lastPicture_;
    if (!this.lastPicture_ && button.firstElementChild) {
      button.removeChild(button.firstElementChild);
      return;
    }
    var img = document.createElement('img');
    img.tabIndex = -1;
    img.onload = function() {
      camera.util.updateElementSize(
          button, button.clientWidth, button.clientHeight, true);
    };
    img.src = this.lastPicture_.thumbnailURL;

    if (button.firstElementChild) {
      button.replaceChild(img, button.firstElementChild);
    } else {
      button.appendChild(img);
    }
  });
};

/**
 * Opens the gallery to browse the picture.
 * @param {camera.models.Gallery.Picture} picture Picture to be browsed.
 * @private
 */
camera.views.camera.GalleryButton.prototype.openGallery_ = function(picture) {
  if (!picture) {
    return;
  }
  if (camera.models.FileSystem.externalFs && chrome.fileManagerPrivate) {
    // TODO(yuli): Don't open video-player app here.
    const id = picture.isMotionPicture ?
        'jcgeabjmjgoblfofpppfkcoakmfobdko|app|video' :
        'nlkncpkkdoccmpiclbokaimcnedabhhm|app|open';
    chrome.fileManagerPrivate.executeTask(
        id, [picture.pictureEntry], result => {
      if (result != 'opened' && result != 'message_sent') {
        console.warn('Unable to open picture: ' + result);
      }
    });
  } else {
    this.router_.navigate(camera.Router.ViewIdentifier.BROWSER,
        {picture: picture});
  }
};

/**
 * @override
 */
camera.views.camera.GalleryButton.prototype.onPictureDeleted = function(
    picture) {
  if (this.lastPicture_ == picture) {
    this.updateButton_();
  }
};

/**
 * @override
 */
camera.views.camera.GalleryButton.prototype.onPictureAdded = function(picture) {
  if (!this.lastPicture_ || this.lastPicture_.timestamp <= picture.timestamp) {
    this.updateButton_();
  }
};

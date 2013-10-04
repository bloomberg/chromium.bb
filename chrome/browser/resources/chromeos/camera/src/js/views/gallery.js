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
 * @param {camera.View.Context} context Context object.
 * @constructor
 */
camera.views.Gallery = function(context) {
  camera.View.call(this, context);

  /**
   * @type {Array.<camera.views.Gallery.DOMPicture>}
   * @private
   */
  this.pictures_ = [];

  /**
   * @type {?number}
   * @private
   */
  this.currentIndex_ = null;

  /**
   * @type {FileSystem}
   * @private
   */
  this.fileSystem_ = null;

  /**
   * @type {number}
   * @private
   */
  this.lastFileId_ = 0;

  // End of properties, seal the object.
  Object.seal(this);
};

/**
 * Wraps an image file as well as the thumbnail file of a picture in the
 * gallery.
 *
 * @param {FileEntry} thumbnailEntry Thumbnail file entry.
 * @param {FileEntry} imageEntry Image file entry.
 * @constructor
 */
camera.views.Gallery.Picture = function(thumbnailEntry, imageEntry) {
  /**
   * @type {FileEntry}
   * @private
   */
  this.thumbnailEntry_ = thumbnailEntry;

  /**
   * @type {FileEntry}
   * @private
   */
  this.imageEntry_ = imageEntry;

  // End of properties. Freeze the object.
  Object.freeze(this);
};

camera.views.Gallery.Picture.prototype = {
  get thumbnailURL() {
    return this.thumbnailEntry_.toURL();
  },
  get imageURL() {
    return this.imageEntry_.toURL();
  },
  get thumbnailEntry() {
    return this.thumbnailEntry_;
  },
  get imageEntry() {
    return this.imageEntry_;
  }
};


/**
 * Represents a picture attached to the DOM by combining the picture data
 * object with the DOM element..
 *
 * @param {camera.views.Gallery.Picture} picture Picture data.
 * @param {HTMLElement} element DOM element holding the picture.
 * @constructor
 */
camera.views.Gallery.DOMPicture = function(picture, element) {
  /**
   * @type {camera.views.Gallery.Picture}
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
  webkitRequestFileSystem(window.PERSISTENT,
      512 * 1024 * 1024 /* 512MB */,
      function(inFileSystem) {
        this.fileSystem_ = inFileSystem;
        this.loadStoredPictures_(callback);
      }.bind(this),
      function() {
        // TODO(mtomasz): Add error handling.
        console.error('Unable to initialize the file system.');
        callback();
      });
};

/**
 * Loads the pictures from the internal storage and adds them to DOM.
 * @param {function()} callback Completion callback.
 * @private
 */
camera.views.Gallery.prototype.loadStoredPictures_ = function(callback) {
  var dirReader = this.fileSystem_.root.createReader();
  var entries = [];

  var onScanFinished = function() {
    var entriesByName = {};
    for (var index = 0; index < entries.length; index++) {
      entriesByName[entries[index].name] = entries[index];
    }
    for (var index = entries.length - 1; index >= 0; index--) {
      var entry = entries[index];
      if (!entry.name) {
        console.warn('TODO(mtomasz): Temporary fix for a strange issue.');
        continue;
      }
      if (entry.name.indexOf('thumb-') !== 0) {
        this.addPictureToDOM_(new camera.views.Gallery.Picture(
            entriesByName['thumb-' + entry.name], entry));
      }
    }
    callback();
  }.bind(this);

  var onError = function() {
    // TODO(mtomasz): Check.
    this.context.onError(
        'gallery-load-failed',
        chrome.i18n.getMessage('errorMsgGalleryLoadFailed'));
    callback();
  }.bind(this);

  var readEntries = function() {
    dirReader.readEntries(function(inEntries) {
      if (inEntries.length == 0) {
        onScanFinished();
        return;
      }
      entries = entries.concat(inEntries);
      readEntries();
    }, onError);
  };

  readEntries();
};

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
 * Sets the index of the currently selected picture.
 * @param {number} index Index of the picture.
 * @private
 */
camera.views.Gallery.prototype.setCurrentIndex_ = function(index) {
  if (this.currentIndex_ !== null)
    this.pictures_[this.currentIndex_].element.classList.remove('selected');
  this.currentIndex_ = index;
  if (index !== null)
    this.pictures_[index].element.classList.add('selected');

  camera.util.ensureVisible(this.pictures_[index].element,
                            document.querySelector('#gallery'));
};

/**
 * Deletes the picture with the specified index by removing it from DOM and
 * removing from the internal storage.
 * @param {number} index Index of the picture.
 * @private
 */
camera.views.Gallery.prototype.deletePicture_ = function(index) {
  // TODO(mtomasz): Improve class names, having DOMPicture and Picutre is
  // very confusing.
  var picture = this.pictures_[index];

  var onFileRemoved = function(index) {
    this.pictures_[index].element.parentNode.removeChild(
        this.pictures_[index].element);
    this.pictures_.splice(index, 1);
    for (var index = 0; index < this.pictures_.length; index++) {
      this.pictures_[index].element.setAttribute('data-index', index);
    }
    if (this.currentIndex_ !== null) {
      var index = null;
      if (this.pictures_.length > 0) {
        if (this.currentIndex_ > 0)
          index = this.currentIndex_ - 1;
        else
          index = 0;
      }
      this.currentIndex_ = null;
      this.setCurrentIndex_(index);
    }
  }.bind(this, index);

  var onError = function() {
    // TODO(mtomasz): Check.
    this.context.onError(
        'gallery-delete-failed',
        chrome.i18n.getMessage('errorMsgGalleryDeleteFailed'));
  }.bind(this);

  // Remove both files from the file system, then from DOM.
  picture.picture.imageEntry.remove(function() {
    picture.picture.thumbnailEntry.remove(function() {
      onFileRemoved();
    },
    onError);
  }, onError);
};

/**
 * Saves the picture with the specified index to the external storage.
 * @param {number} index Index of the picture.
 * @private
 */
camera.views.Gallery.prototype.exportPicture_ = function(index) {
  var picture = this.pictures_[index];

  var accepts = [{
    description: '*.jpg',
    extensions: ['jpg", "jpeg'],
    mimeTypes: ['image/jpeg']
  }];

  var fileName = picture.picture.imageEntry.name;

  var onError = function() {
    // TODO(mtomasz): Check if it works.
    this.context_.onError(
        'gallery-export-error',
        chrome.i18n.getMessage('errorMsgGalleryExportFailed'));
  }.bind(this);

  var onDataLoaded = function(data) {
    chrome.fileSystem.chooseEntry({
      type: 'saveFile',
      suggestedName: fileName,
      accepts: accepts
    }, function(fileEntry) {
      if (!fileEntry)
        return;
      fileEntry.createWriter(function(fileWriter) {
        fileWriter.onwrite = function() {};
        fileWriter.onerror = onError;
        // Blob has to be a Blob instance to be saved.
        var array = new Uint8Array(data.length);
        for (var index = 0; index < data.length; index++) {
          array[index] = data.charCodeAt(index);
        }
        var blob = new Blob([array], {type: 'image/jpeg'});
        fileWriter.write(blob);
      }.bind(this),
      onError);
    }.bind(this));
  }.bind(this);

  picture.picture.imageEntry.file(function(file) {
    var reader = new FileReader;
    reader.onloadend = function(e) {
      onDataLoaded(reader.result);
    };
    reader.onerror = onError;
    reader.readAsBinaryString(file);
  }.bind(this));
};

/**
 * @override
 */
camera.views.Gallery.prototype.onKeyPressed = function(event) {
  var currentPicture = this.pictures_[this.currentIndex_];
  switch (event.keyIdentifier) {
    case 'Right':
      var index = (this.currentIndex_ + this.pictures_.length - 1) %
          this.pictures_.length;
      this.setCurrentIndex_(index);
      break;
    case 'Left':
      var index = (this.currentIndex_ + 1) % this.pictures_.length;
      this.setCurrentIndex_(index);
      break;
    case 'Down':
      for (var offset = 1; offset < this.pictures_.length; offset++) {
        var index = (this.currentIndex_ + this.pictures_.length - offset) %
            this.pictures_.length;
        if (currentPicture.element.offsetLeft ==
            this.pictures_[index].element.offsetLeft) {
          this.setCurrentIndex_(index);
          break;
        }
      }
      event.preventDefault();
      break;
    case 'Up':
      for (var offset = 1; offset < this.pictures_.length; offset++) {
        var index = (this.currentIndex_ + this.pictures_.length + offset) %
            this.pictures_.length;
        if (currentPicture.element.offsetLeft ==
            this.pictures_[index].element.offsetLeft) {
          this.setCurrentIndex_(index);
          break;
        }
      }
      event.preventDefault();
      break;
    case 'End':
      this.setCurrentIndex_(0);
      break;
    case 'Home':
      this.setCurrentIndex_(this.pictures_.length - 1);
      break;
    case 'U+007F':
      this.deletePicture_(this.currentIndex_);
      break;
    case 'Enter':
      this.exportPicture_(this.currentIndex_);
      break;
  }
};

/**
 * Adds a picture to the gallery.
 * @param {camera.views.Gallery.Picture} picture Picture object to be added to
 *     DOM.
 * @private
 */
camera.views.Gallery.prototype.addPictureToDOM_ = function(picture) {
  var index = this.pictures_.length;

  var gallery = document.querySelector('#gallery .padder');
  var img = document.createElement('img');
  img.src = picture.thumbnailURL;
  img.setAttribute('data-index', index);
  gallery.insertBefore(img, gallery.firstChild);

  // Add to the collection.
  this.pictures_.push(new camera.views.Gallery.DOMPicture(picture, img));

  // Add handlers.
  img.addEventListener('click', function() {
    var index = parseInt(img.getAttribute('data-index'));
    if (this.currentIndex_ == index)
      this.exportPicture_(index);
    else
      this.setCurrentIndex_(index);
  }.bind(this));
};

/**
 * Saves the picture to the passed file name in the internal storage.
 *
 * @param {string} fileName Name of the file in the internal storage.
 * @param {string} dataURL Data of the image to be saved.
 * @param {function(FileEntry)} onSuccess Success callback with the entry of
 *     the saved picture.
 * @param {function(FileError)} onFailure Failure callback.
 * @private
 */
camera.views.Gallery.prototype.savePictureToFile_ = function(
    fileName, dataURL, onSuccess, onFailure) {
  this.fileSystem_.root.getFile(
      fileName,
      {create: true},
      function(fileEntry) {
        fileEntry.createWriter(function(fileWriter) {
          fileWriter.onwriteend = function() {
            onSuccess(fileEntry);
          }.bind(this);
          fileWriter.onerror = onFailure;
          var base64string = dataURL.substring(dataURL.indexOf(',') + 1);
          var data = atob(base64string);
          var array = new Uint8Array(data.length);
          for (var index = 0; index < data.length; index++) {
            array[index] = data.charCodeAt(index);
          }
          var blob = new Blob([array], {type: 'image/jpeg'});
          fileWriter.write(blob);
        }.bind(this),
        onFailure);
      }.bind(this),
      onFailure);
};

/**
 * Adds a picture to the gallery and saves it in the internal storage.
 * @param {string} dataURL Data of the picture to be added.
 */
camera.views.Gallery.prototype.addPicture = function(dataURL) {
  // TODO(mtomasz): Handle the error.
  if (!this.fileSystem_)
    return;

  // Create a thumbnail.
  var fullImg = document.createElement('img');
  fullImg.src = dataURL;
  var canvas = document.createElement('canvas');
  var context = canvas.getContext('2d');
  var thumbnailWidth = 480;  // Twice wider than in CSS for hi-dpi, like Pixel.
  var thumbnailHeight = Math.round((480 / fullImg.width) * fullImg.height);
  canvas.width = thumbnailWidth;
  canvas.height = thumbnailHeight;
  context.drawImage(fullImg, 0, 0, thumbnailWidth, thumbnailHeight);

  // Save the thumbnail as well as the full screen resolution picture.
  this.lastFileId_++;
  var fileNameBase = new Date().getTime() + '.' + this.lastFileId_;

  var onError = function(e) {
    this.context.onError(
        'gallery-save-failed',
        chrome.i18n.getMessage('errorMsgGallerySaveFailed'));
  }.bind(this);

  this.savePictureToFile_(
      'thumb-' + fileNameBase + '.jpg',
      canvas.toDataURL(),
      function(thumbnailEntry) {
        this.savePictureToFile_(
          fileNameBase + '.jpg',
          dataURL,
          function(imageEntry) {
            var picture = new camera.views.Gallery.Picture(
                thumbnailEntry, imageEntry);
            this.addPictureToDOM_(picture);
          }.bind(this),
          // TODO(mtomasz): Remove the thumbnail on error.
          onError);
      }.bind(this),
      onError);
};


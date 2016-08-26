// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var camera = camera || {};

/**
 * Namespace for models.
 */
camera.models = camera.models || {};

/**
 * Creates the Gallery view controller.
 * @constructor
 */
camera.models.Gallery = function() {
  /**
   * @type {Array.<camera.models.Gallery.Picture>}
   * @private
   */
  this.pictures_ = [];

  /**
   * @type {FileSystem}
   * @private
   */
  this.fileSystem_ = null;

  /**
   * @type {number}
   * @private
   */
  this.lastDateSecond_ = 0;

  /**
   * @type {number}
   * @private
   */
  this.sameSecondCount_ = 0;

  /**
   * @type {Array.<camera.models.Gallery.Observer>}
   * @private
   */
  this.observers_ = [];

  // End of properties, seal the object.
  Object.seal(this);
};

/**
 * Picture types.
 * @enum {number}
 */
camera.models.Gallery.PictureType = Object.freeze({
  STILL: 0,  // Still pictures (images).
  MOTION: 1  // Motion pictures (video).
});

/**
 * Instance of the singleton gallery model.
 * @type {camera.models.Gallery}
 * @private
 */
camera.models.Gallery.instance_ = null;

/**
 * Queue for the asynchronous getInstance() static method.
 * @type {camera.util.Queue}
 * @private
 */
camera.models.Gallery.queue_ = new camera.util.Queue();

/**
 * Wraps a picture file as well as the thumbnail file of a picture in the
 * gallery.
 *
 * @param {FileEntry} thumbnailEntry Thumbnail file entry.
 * @param {FileEntry} pictureEntry Picture file entry.
 * @param {camera.models.Gallery.PictureType} type Picture type.
 * @constructor
 */
camera.models.Gallery.Picture = function(thumbnailEntry, pictureEntry, type) {
  /**
   * @type {FileEntry}
   * @private
   */
  this.thumbnailEntry_ = thumbnailEntry;

  /**
   * @type {FileEntry}
   * @private
   */
  this.pictureEntry_ = pictureEntry;

  /**
   * @type {camera.models.Gallery.PictureType}
   * @private
   */
  this.pictureType_ = type;

  // End of properties. Freeze the object.
  Object.freeze(this);
};

camera.models.Gallery.Picture.prototype = {
  get thumbnailURL() {
    return this.thumbnailEntry_.toURL();
  },
  get pictureURL() {
    return this.pictureEntry_.toURL();
  },
  get thumbnailEntry() {
    return this.thumbnailEntry_;
  },
  get pictureEntry() {
    return this.pictureEntry_;
  },
  get pictureType() {
    return this.pictureType_;
  }
};

/**
 * Observer interface for the gallery model changes.
 * @constructor
 */
camera.models.Gallery.Observer = function() {
};

/**
 * Notifies about a picture being deleted.
 * @param {camera.models.Gallery.Picture} picture Picture to be deleted.
 */
camera.models.Gallery.Observer.prototype.onPictureDeleting = function(picture) {
};

/**
 * Notifies about an added picture.
 * @param {camera.models.Gallery.Picture} picture Picture to be added.
 */
camera.models.Gallery.Observer.prototype.onPictureAdded = function(picture) {
};

/**
 * Returns a singleton instance to the model.
 * @param {function(camera.models.Gallery)} onSuccess Success callback.
 * @param {function()} onFailure Failure callback.
 */
camera.models.Gallery.getInstance = function(onSuccess, onFailure) {
  camera.models.Gallery.queue_.run(function(callback) {
    if (camera.models.Gallery.instance_) {
      onSuccess(camera.models.Gallery.instance_);
      callback();
      return;
    }
    var model = new camera.models.Gallery();
    model.initialize_(function() {  // onSuccess.
      camera.models.Gallery.instance_ = model;
      onSuccess(model);
      callback();
    }, function() {  // onFailure.
      onFailure();
      callback();
    });
  });
};

camera.models.Gallery.prototype = {
  /**
   * @return {number} Number of pictures in the gallery.
   */
  get length() {
    return this.pictures_.length;
  },

  /**
   * @return {Array.<camera.models.Gallery.Picture>} Pictures' models.
   */
  get pictures() {
    return this.pictures_;
  }
};

/**
 * Initializes the gallery model.
 *
 * @param {function()} onSuccess Success callback.
 * @param {function()} onFailure Failure callback.
 * @private
 */
camera.models.Gallery.prototype.initialize_ = function(onSuccess, onFailure) {
  webkitRequestFileSystem(window.PERSISTENT,
      768 * 1024 * 1024 /* 768MB */,
      function(inFileSystem) {
        this.fileSystem_ = inFileSystem;
        this.loadStoredPictures_(onSuccess, onFailure);
      }.bind(this),
      function() {
        // TODO(mtomasz): Add error handling.
        console.error('Unable to initialize the file system.');
        onFailure();
      });
};

/**
 * Adds an observer.
 * @param {camera.models.Gallery.Observer} observer Observer object.
 */
camera.models.Gallery.prototype.addObserver = function(observer) {
  this.observers_.push(observer);
};

/**
 * Removes an observer.
 * @param {camera.models.Gallery.Observer} observer Observer object.
 */
camera.models.Gallery.prototype.removeObserver = function(observer) {
  var index = this.observers_.indexOf(observer);
  this.observers_.splice(index, 1);
};

/**
 * Loads the pictures from the internal storage and adds them to the internal
 * list.
 *
 * @param {function()} onSuccess Success callback.
 * @param {function()} onFailure Failure callback.
 * @private
 */
camera.models.Gallery.prototype.loadStoredPictures_ = function(
    onSuccess, onFailure) {
  var dirReader = this.fileSystem_.root.createReader();
  var entries = [];
  var queue = new camera.util.Queue();

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
      if (entry.name.indexOf('thumb-') === 0)
        continue;
      var type = (entry.name.indexOf('VID_') === 0) ?
          camera.models.Gallery.PictureType.MOTION :
          camera.models.Gallery.PictureType.STILL;
      queue.run(function(entry, callback) {
        var thumbnailName = 'thumb-' + entry.name;
        if (type == camera.models.Gallery.PictureType.MOTION) {
          thumbnailName = (thumbnailName.substr(0, thumbnailName.lastIndexOf('.')) ||
              thumbnailName) + '.jpg';
        }
        var thumbnailEntry = entriesByName[thumbnailName];
        // No thumbnail, most probably pictures taken by the old Camera App.
        // So, create the thumbnail now.
        if (!thumbnailEntry) {
          this.createThumbnail_(entry.toURL(), type, function(thumbnailBlob) {
            this.savePictureToFile_(
                thumbnailName,
                thumbnailBlob,
                function(recreatedThumbnailEntry) {
                  this.pictures_.push(new camera.models.Gallery.Picture(
                      recreatedThumbnailEntry, entry, type));
                  callback();
                }.bind(this), function() {
                  // Ignore this error, since it is better to load something
                  // than nothing.
                  console.warn('Unabled to save the recreated thumbnail.');
                  callback();
                });
          }.bind(this), function() {
            // Ignore this error, since it is better to load something than
            // nothing.
            console.warn('Unable to recreate the thumbnail.');
            callback();
          });
        } else {
          // The thumbnail is available.
          this.pictures_.push(new camera.models.Gallery.Picture(
              thumbnailEntry, entry, type));
          callback();
        }
      }.bind(this, entry));
    }
    queue.run(onSuccess);
  }.bind(this);

  var readEntries = function() {
    dirReader.readEntries(function(inEntries) {
      if (inEntries.length == 0) {
        onScanFinished();
        return;
      }
      entries = entries.concat(Array.prototype.slice.call(inEntries));
      readEntries();
    }, onFailure);
  };

  readEntries();
};

/**
 * Deletes the picture with the specified index by removing it from DOM and
 * removing from the internal storage.
 *
 * @param {camera.models.Gallery.Picture} picture Picture to be deleted.
 * @param {function()} onSuccess Success callback.
 * @param {function()} onFailure Failure callback.
 */
camera.models.Gallery.prototype.deletePicture = function(
    picture, onSuccess, onFailure) {
  picture.pictureEntry.remove(function() {
    picture.thumbnailEntry.remove(function() {
      // Notify observers.
      for (var i = 0; i < this.observers_.length; i++) {
        this.observers_[i].onPictureDeleting(picture);
      }
      var index = this.pictures_.indexOf(picture);
      this.pictures_.splice(index, 1);

      onSuccess();
    }.bind(this), onFailure);
  }.bind(this), onFailure);
};

/**
 * Saves the picture to the external storage.
 *
 * @param {camera.models.Gallery.Picture} picture Picture to be exported.
 * @param {FileEntry} fileEntry Target file entry.
 * @param {function()} onSuccess Success callback.
 * @param {function()} onFailure Failure callback,
 */
camera.models.Gallery.prototype.exportPicture = function(
    picture, fileEntry, onSuccess, onFailure) {
  // TODO(yuli): Handle insufficient storage.
  fileEntry.getParent(function(directory) {
    picture.pictureEntry.copyTo(
        directory, fileEntry.name, onSuccess, onFailure);
  }, onFailure);
};

/**
 * Saves the picture to the passed file name in the internal storage.
 *
 * @param {string} fileName Name of the file in the internal storage.
 * @param {Blob} blob Data of the picture to be saved.
 * @param {function(FileEntry)} onSuccess Success callback with the entry of
 *     the saved picture.
 * @param {function(FileError)} onFailure Failure callback.
 * @private
 */
camera.models.Gallery.prototype.savePictureToFile_ = function(
    fileName, blob, onSuccess, onFailure) {
  this.fileSystem_.root.getFile(
      fileName,
      {create: true},
      function(fileEntry) {
        fileEntry.createWriter(function(fileWriter) {
          fileWriter.onwriteend = function() {
            onSuccess(fileEntry);
          }.bind(this);
          fileWriter.onerror = onFailure;
          fileWriter.write(blob);
        }.bind(this),
        onFailure);
      }.bind(this),
      onFailure);
};

/**
 * Creates a thumbnail from the original picture.
 *
 * @param {string} url Original picture as an URL.
 * @param {camera.models.Gallery.PictureType} type Original picture's type.
 * @param {function(Blob)} onSuccess Success callback with the thumbnail as a
 *     blob.
 * @param {function()} onFailure Failure callback.
 * @private
 */
camera.models.Gallery.prototype.createThumbnail_ = function(
    url, type, onSuccess, onFailure) {
  var name = (type == camera.models.Gallery.PictureType.MOTION) ?
      'video' : 'img';
  var original = document.createElement(name);

  var drawThumbnail = function() {
    var canvas = document.createElement('canvas');
    var context = canvas.getContext('2d');
    var thumbnailWidth = 480;  // Twice wider than in CSS for hi-dpi,
                               // like Pixel.
    var ratio = (name == 'video') ?
        original.videoHeight / original.videoWidth :
        original.height / original.width;
    var thumbnailHeight = Math.round(480 * ratio);
    canvas.width = thumbnailWidth;
    canvas.height = thumbnailHeight;
    context.drawImage(original, 0, 0, thumbnailWidth, thumbnailHeight);
    canvas.toBlob(function(blob) {
      if (blob) {
        onSuccess(blob);
      } else {
        onFailure();
      }
    }, 'image/jpeg');
  };

  if (name == 'video') {
    original.onloadeddata = drawThumbnail;
  } else {
    original.onload = drawThumbnail;
  }

  original.onerror = function() {
    onFailure();
  };

  original.src = url;
};

/**
 * Generates a file name base for the picture.
 *
 * @param {camera.models.Gallery.PictureType} type Type of the picture.
 * @return {string} File name base.
 * @private
 */
camera.models.Gallery.prototype.generateFileNameBase_ = function(type) {
  function pad(n) {
    if (n < 10)
      n = '0' + n;
    return n;
  }

  // File name base will be formatted as IMG_yyyyMMdd_HHmmss.
  var prefix = (type == camera.models.Gallery.PictureType.MOTION) ?
      'VID_' : 'IMG_';
  var now = new Date();
  var result = prefix + now.getFullYear() + pad(now.getMonth() + 1) +
      pad(now.getDate()) + '_' + pad(now.getHours()) + pad(now.getMinutes()) +
      pad(now.getSeconds());

  // If the last name was generated for the same second,
  // _1, _2, etc will be appended to the name.
  var dateSecond = Math.floor(now.getTime() / 1000);
  if (dateSecond == this.lastDateSecond_) {
      this.sameSecondCount_++;
      result += "_" + this.sameSecondCount_;
  } else {
      this.lastDateSecond_ = dateSecond;
      this.sameSecondCount_ = 0;
  }

  return result;
};

/**
 * Generates a file name extension for the picture.
 *
 * @param {camera.models.Gallery.PictureType} type Type of the picture.
 * @return {string} File name extension.
 * @private
 */
camera.models.Gallery.prototype.generateFileNameExt_ = function(type) {
  return (type == camera.models.Gallery.PictureType.MOTION) ?
      '.webm' : '.jpg';
};

/**
 * Adds a picture to the gallery and saves it in the internal storage.
 *
 * @param {Blob} blob Data of the picture to be added.
 * @param {camera.models.Gallery.PictureType} type Type of the picture to be
 *     added.
 * @param {function()} onFailure Failure callback.
 */
camera.models.Gallery.prototype.addPicture = function(
    blob, type, onFailure) {
  this.createThumbnail_(
      URL.createObjectURL(blob), type, function(thumbnailBlob) {
    // Save the thumbnail as well as the full screen resolution picture.
    var fileNameBase = this.generateFileNameBase_(type);

    this.savePictureToFile_(
        'thumb-' + fileNameBase + '.jpg',
        thumbnailBlob,
        function(thumbnailEntry) {
          this.savePictureToFile_(
            fileNameBase + this.generateFileNameExt_(type),
            blob,
            function(pictureEntry) {
              var picture = new camera.models.Gallery.Picture(
                  thumbnailEntry, pictureEntry, type);
              this.pictures_.push(picture);
              // Notify observers.
              for (var i = 0; i < this.observers_.length; i++) {
                this.observers_[i].onPictureAdded(picture);
              }
            }.bind(this),
            // TODO(mtomasz): Remove the thumbnail on error.
            onFailure);
        }.bind(this),
        onFailure);
  }.bind(this), onFailure);
};


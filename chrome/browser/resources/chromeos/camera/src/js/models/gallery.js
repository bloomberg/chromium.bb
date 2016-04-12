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
 * Wraps an image file as well as the thumbnail file of a picture in the
 * gallery.
 *
 * @param {FileEntry} thumbnailEntry Thumbnail file entry.
 * @param {FileEntry} imageEntry Image file entry.
 * @constructor
 */
camera.models.Gallery.Picture = function(thumbnailEntry, imageEntry) {
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

camera.models.Gallery.Picture.prototype = {
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
      512 * 1024 * 1024 /* 512MB */,
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
      queue.run(function(entry, callback) {
        var thumbnailEntry = entriesByName['thumb-' + entry.name];
        // No thumbnail, most probably pictures taken by the old Camera App.
        // So, create the thumbnail now.
        if (!thumbnailEntry) {
          this.createThumbnail_(entry.toURL(), function(thumbnailDataURL) {
            this.savePictureToFile_(
                'thumb-' + entry.name,
                thumbnailDataURL,
                function(recreatedThumbnailEntry) {
                  this.pictures_.push(new camera.models.Gallery.Picture(
                      recreatedThumbnailEntry, entry));
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
              thumbnailEntry, entry));
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
  picture.imageEntry.remove(function() {
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
 * Saves the picture with the specified index to the external storage.
 *
 * @param {camera.models.Gallery.Picture} picture Picture to be exported.
 * @param {FileEntry} fileEntry Target file entry.
 * @param {function()} onSuccess Success callback.
 * @param {function()} onFailure Failure callback,
 */
camera.models.Gallery.prototype.exportPicture = function(
    picture, fileEntry, onSuccess, onFailure) {
  var onDataLoaded = function(data) {
    fileEntry.createWriter(function(fileWriter) {
      fileWriter.onwriteend = onSuccess;
      fileWriter.onerror = onFailure;
      // Blob has to be a Blob instance to be saved.
      var array = new Uint8Array(data.length);
      for (var index = 0; index < data.length; index++) {
        array[index] = data.charCodeAt(index);
      }
      var blob = new Blob([array], {type: 'image/jpeg'});
      fileWriter.write(blob);
    }.bind(this),
    onFailure);
  }.bind(this);

  picture.imageEntry.file(function(file) {
    var reader = new FileReader;
    reader.onloadend = function(e) {
      onDataLoaded(reader.result);
    };
    reader.onerror = onFailure;
    reader.readAsBinaryString(file);
  }.bind(this), onFailure);
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
camera.models.Gallery.prototype.savePictureToFile_ = function(
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
 * Creates a thumbnail from the original picture.
 *
 * @param {string} url Original picture as an URL.
 * @param {function(string)} onSuccess Success callback with the thumbnail as a
 *     data URL.
 * @param {function()} onFailure Failure callback.
 * @private
 */
camera.models.Gallery.prototype.createThumbnail_ = function(
    url, onSuccess, onFailure) {
  var fullImg = document.createElement('img');

  fullImg.onload = function() {
    var canvas = document.createElement('canvas');
    var context = canvas.getContext('2d');
    var thumbnailWidth = 480;  // Twice wider than in CSS for hi-dpi,
                               // like Pixel.
    var thumbnailHeight = Math.round((480 / fullImg.width) * fullImg.height);
    canvas.width = thumbnailWidth;
    canvas.height = thumbnailHeight;
    context.drawImage(fullImg, 0, 0, thumbnailWidth, thumbnailHeight);
    onSuccess(canvas.toDataURL());
  };

  fullImg.onerror = function() {
    onFailure();
  };

  fullImg.src = url;
};

/**
 * Generates a file name base for the picture.
 *
 * @return {string} File name base.
 * @private
 */
camera.models.Gallery.prototype.generateFileNameBase_ = function() {
  function pad(n) {
    if (n < 10)
      n = '0' + n;
    return n;
  }

  // File name base will be formatted as IMG_yyyyMMdd_HHmmss.
  var now = new Date();
  var result = 'IMG_' + now.getFullYear() + pad(now.getMonth() + 1) +
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
 * Adds a picture to the gallery and saves it in the internal storage.
 *
 * @param {string} dataURL Data of the picture to be added.
 * @param {function()} onSuccess Success callback.
 * @param {function()} onFailure Failure callback.
 */
camera.models.Gallery.prototype.addPicture = function(
    dataURL, onSuccess, onFailure) {
  this.createThumbnail_(dataURL, function(thumbnailDataURL) {
    // Save the thumbnail as well as the full screen resolution picture.
    var fileNameBase = this.generateFileNameBase_();

    this.savePictureToFile_(
        'thumb-' + fileNameBase + '.jpg',
        thumbnailDataURL,
        function(thumbnailEntry) {
          this.savePictureToFile_(
            fileNameBase + '.jpg',
            dataURL,
            function(imageEntry) {
              var picture = new camera.models.Gallery.Picture(
                  thumbnailEntry, imageEntry);
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


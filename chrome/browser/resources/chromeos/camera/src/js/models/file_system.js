// Copyright 2018 The Chromium OS Authors. All rights reserved.
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
 * Creates the file system controller.
 * @constructor
 */
camera.models.FileSystem = function() {
  // End of properties, seal the object.
  Object.seal(this);
};

/**
 * The prefix of image files.
 * @type {string}
 * @const
 */
camera.models.FileSystem.IMAGE_PREFIX = 'IMG_';

/**
 * The prefix of video files.
 * @type {string}
 * @const
 */
camera.models.FileSystem.VIDEO_PREFIX = 'VID_';

/**
 * The prefix of thumbnail files.
 * @type {string}
 * @const
 */
camera.models.FileSystem.THUMBNAIL_PREFIX = 'thumb-';

/**
 * The image width of thumbnail files.
 * @type {number}
 * @const
 */
camera.models.FileSystem.THUMBNAIL_WIDTH = 480;

/**
 * Internal file system.
 * @type {FileSystem}
 */
camera.models.FileSystem.internalFs = null;

/**
 * External file system.
 * @type {FileSystem}
 */
camera.models.FileSystem.externalFs = null;

/**
 * @type {boolean}
 */
camera.models.FileSystem.ackMigratePictures = false;

/**
 * @type {boolean}
 */
camera.models.FileSystem.doneMigratePictures = false;

/**
 * Initializes the file system. This function should be called only once
 * to initialize the file system in the beginning.
 * @param {function()} onSuccess Success callback.
 * @param {function(*=)} onFailure Failure callback.
 */
camera.models.FileSystem.initialize = function(onSuccess, onFailure) {
  var initExternalFs = function() {
    return new Promise(resolve => {
      if (!camera.util.isChromeOS()) {
        resolve();
        return;
      }
      chrome.fileSystem.getVolumeList(volumes => {
        if (volumes) {
          for (var i = 0; i < volumes.length; i++) {
            if (volumes[i].volumeId.indexOf('downloads:Downloads') !== -1) {
              chrome.fileSystem.requestFileSystem(volumes[i], fs => {
                camera.models.FileSystem.externalFs = fs;
                resolve();
              });
              return;
            }
          }
        }
        resolve();
      });
    });
  };

  var initInternalFs = function() {
    return new Promise((resolve, reject) => {
      webkitRequestFileSystem(
          window.PERSISTENT, 768 * 1024 * 1024 /* 768MB */, fs => {
        camera.models.FileSystem.internalFs = fs;
        resolve();
      }, reject);
    });
  };

  var readLocalStorage = function() {
    return new Promise(resolve => {
      chrome.storage.local.get({
        ackMigratePictures: 0,
        doneMigratePictures: 0,
      }, values => {
        // 1: User acknowledge to migrate pictures to Downloads.
        camera.models.FileSystem.ackMigratePictures =
            values.ackMigratePictures >= 1;
        // 1: All pictures have been migrated to Downloads.
        camera.models.FileSystem.doneMigratePictures =
            values.doneMigratePictures >= 1;
        resolve();
      });
    });
  };

  Promise.all([
    initInternalFs(), initExternalFs(),readLocalStorage()
  ]).then(onSuccess).catch(onFailure);
};

/**
 * Reloads the file entries from internal and external file system.
 * @param {function(Array.<FileEntry>, Array.<FileEntry>)} onSuccess Success
 *     callback with internal and external entries.
 * @param {function(*=)} onFailure Failure callback.
 * @private
 */
camera.models.FileSystem.reloadEntries_ = function(onSuccess, onFailure) {
  Promise.all([
    camera.models.FileSystem.readFs_(camera.models.FileSystem.internalFs),
    camera.models.FileSystem.readFs_(camera.models.FileSystem.externalFs)
  ]).then(values => {
    onSuccess(values[0], values[1]);
  }).catch(onFailure);
};

/**
 * Reads file entries from the file system.
 * @param {FileSystem} fs File system to be read.
 * @return {!Promise<!Array.<FileEntry>} Promise for the read file entries.
 * @private
 */
camera.models.FileSystem.readFs_ = function(fs) {
  return new Promise((resolve, reject) => {
    if (fs) {
      var dirReader = fs.root.createReader();
      var entries = [];
      var readEntries = () => {
        dirReader.readEntries(inEntries => {
          if (inEntries.length == 0) {
            resolve(entries);
            return;
          }
          entries = entries.concat(inEntries);
          readEntries();
        }, reject);
      };
      readEntries();
    } else {
      resolve([]);
    }
  });
};

/**
 * Checks if there is an image or video in the internal file system.
 * @return {!Promise<boolean>} Promise for the result.
 * @private
 */
camera.models.FileSystem.hasInternalPictures_ = function() {
  var fs = camera.models.FileSystem.internalFs;
  return camera.models.FileSystem.readFs_(fs).then(entries => {
    for (var index = entries.length - 1; index >= 0; index--) {
      // Check if there is any picture other than thumbnail in file entries.
      // Pictures taken by old Camera App may not have IMG_ or VID_ prefix.
      if (!camera.models.FileSystem.hasThumbnailPrefix_(entries[index])) {
        return true;
      }
    }
    return false;
  });
};

/**
 * Sets migration acknowledgement to true.
 * @privvate
 */
camera.models.FileSystem.ackMigrate_ = function() {
  camera.models.FileSystem.ackMigratePictures = true;
  chrome.storage.local.set({ackMigratePictures: 1});
};

/**
 * Sets migration status is done.
 * @private
 */
camera.models.FileSystem.doneMigrate_ = function() {
  camera.models.FileSystem.doneMigratePictures = true;
  chrome.storage.local.set({doneMigratePictures: 1});
};

/**
 * Checks migration is needed or not.
 * @param {function()} onYes Callback function to do migration.
 * @param {function()} onNo Callback function without migration.
 * @param {function(*=)} onFailure Failure callback.
 */
camera.models.FileSystem.needMigration = function(onYes, onNo, onFailure) {
  if (camera.models.FileSystem.doneMigratePictures ||
      !camera.models.FileSystem.externalFs) {
    onNo();
  } else {
    camera.models.FileSystem.hasInternalPictures_().then(result => {
      if (result) {
        onYes();
      } else {
        // If the external FS is supported and there is no picture in
        // the internal storage, it implies users acknowledge to use
        // the external FS from now without showing a prompt dialog.
        camera.models.FileSystem.ackMigrate_();
        camera.models.FileSystem.doneMigrate_();
        onNo();
      }
    }).catch(onFailure);
  }
};

/**
 * Migrates all files from internal storage to external storage.
 * @param {function()} onSuccess Success callback.
 * @param {function(*=)} onFailure Failure callback.
 */
camera.models.FileSystem.migratePictures = function(onSuccess, onFailure) {
  var existByName = {};

  var migratePicture = function(pictureEntry, thumbnailEntry) {
    return new Promise(function(resolve, reject) {
      // TODO(yuli): Check existing files that were added after loading entries.
      var fileName = camera.models.FileSystem.regulatePictureName(pictureEntry);
      while (existByName[fileName]) {
        fileName = camera.models.FileSystem.incrementFileName_(fileName);
      }
      pictureEntry.copyTo(
          camera.models.FileSystem.externalFs.root, fileName, newPicEntry => {
        if (newPicEntry.name != pictureEntry.name && thumbnailEntry) {
          thumbnailEntry.moveTo(
              camera.models.FileSystem.internalFs.root,
              camera.models.FileSystem.getThumbnailName(newPicEntry));
        }
        // Remove the internal picture even failing to rename thumbnail.
        pictureEntry.remove(function() {});
        resolve();
      }, reject);
    });
  };

  camera.models.FileSystem.ackMigrate_();
  camera.models.FileSystem.reloadEntries_(
      function(internalEntries, externalEntries) {
    var pictureEntries = [];
    var thumbnailEntriesByName = {};
    camera.models.FileSystem.parseInternalEntries_(
        internalEntries, thumbnailEntriesByName, pictureEntries);

    // Uses to check whether file name conflict or not.
    for (var index = 0; index < externalEntries.length; index++) {
      existByName[externalEntries[index].name] = true;
    }

    var migrated = [];
    for (var index = 0; index < pictureEntries.length; index++) {
      var entry = pictureEntries[index];
      var thumbnailName = camera.models.FileSystem.getThumbnailName(entry);
      var thumbnailEntry = thumbnailEntriesByName[thumbnailName];
      migrated.push(migratePicture(entry, thumbnailEntry));
    }
    Promise.all(migrated).then(function() {
      camera.models.FileSystem.doneMigrate_();
      onSuccess();
    }).catch(onFailure);
  }, onFailure);
};

/**
 * Generates a file name for the picture.
 * @param {boolean} isVideo Picture is a video.
 * @param {int} time Timestamp of the picture in millisecond.
 * @return {string} Picture's file name.
 * @private
 */
camera.models.FileSystem.generatePictureName_ = function(isVideo, time) {
  function pad(n) {
    return (n < 10 ? '0' : '') + n;
  }

  // File name base will be formatted as IMG/VID_yyyyMMdd_HHmmss.
  var prefix = isVideo ?
      camera.models.FileSystem.VIDEO_PREFIX :
      camera.models.FileSystem.IMAGE_PREFIX;
  var now = new Date(time);
  var result = prefix + now.getFullYear() + pad(now.getMonth() + 1) +
      pad(now.getDate()) + '_' + pad(now.getHours()) + pad(now.getMinutes()) +
      pad(now.getSeconds());

  return result + (isVideo ? '.mkv' : '.jpg');
};

/**
 * Regulates the picture name to the desired format if it's in legacy formats.
 * @param {FileEntry} entry Picture entry whose name to be regulated.
 * @return {string} Name in the desired format.
 */
camera.models.FileSystem.regulatePictureName = function(entry) {
  if (camera.models.FileSystem.hasVideoPrefix(entry) ||
      camera.models.FileSystem.hasImagePrefix_(entry)) {
    var match = entry.name.match(/(\w{3}_\d{8}_\d{6})(?:_(\d+))?(\..+)?$/);
    if (match) {
      var idx = match[2] ? ' (' + match[2] + ')' : '';
      var ext = match[3] ? match[3].replace(/\.webm$/, '.mkv') : '';
      return match[1] + idx + ext;
    }
  } else {
    // Early pictures are in legacy file name format (crrev.com/c/310064).
    var match = entry.name.match(/(\d+).(?:\d+)/);
    if (match) {
      return camera.models.FileSystem.generatePictureName_(
          false, parseInt(match[1], 10));
    }
  }
  return entry.name;
};

/**
 * Saves the blob to the given file name. Name of the actually saved file
 * might be different from the given file name if the file already exists.
 * @param {FileSystem} fs File system to be written.
 * @param {string} fileName Name of the file.
 * @param {Blob} blob Data of the file to be saved.
 * @param {function(FileEntry)} onSuccess Success callback with the entry of
 *     the saved file.
 * @param {function(*=)} onFailure Failure callback.
 * @private
 */
camera.models.FileSystem.saveToFile_ = function(
    fs, fileName, blob, onSuccess, onFailure) {
  fs.root.getFile(
      fileName, {create: true, exclusive: true}, function(fileEntry) {
    fileEntry.createWriter(function(fileWriter) {
      fileWriter.onwriteend = function() {
        onSuccess(fileEntry);
      };
      fileWriter.onerror = onFailure;
      fileWriter.write(blob);
    },
    onFailure);
  }, function(error) {
    if (error instanceof DOMException &&
        error.name == 'InvalidModificationError') {
      fileName = camera.models.FileSystem.incrementFileName_(fileName);
      camera.models.FileSystem.saveToFile_(
          fs, fileName, blob, onSuccess, onFailure);
    } else {
      onFailure(error);
    }
  });
};

/**
 * Saves the picture into the external or internal file system.
 * @param {boolean} isVideo Picture is a video.
 * @param {Blob} blob Data of the picture to be saved.
 * @param {function(FileEntry)} onSuccess Success callback with the entry of
 *     the saved picture.
 * @param {function(*=)} onFailure Failure callback.
 */
camera.models.FileSystem.savePicture = function(
    isVideo, blob, onSuccess, onFailure) {
  var fs = camera.models.FileSystem.externalFs ?
      camera.models.FileSystem.externalFs : camera.models.FileSystem.internalFs;
  var name = camera.models.FileSystem.generatePictureName_(isVideo, Date.now());
  camera.models.FileSystem.saveToFile_(fs, name, blob, onSuccess, onFailure);
};

/**
 * Creates a thumbnail from the picture.
 * @param {boolean} isVideo Picture is a video.
 * @param {string} url Picture as an URL.
 * @param {function(Blob)} onSuccess Success callback with the thumbnail as a
 *     blob.
 * @param {function(*=)} onFailure Failure callback.
 * @private
 */
camera.models.FileSystem.createThumbnail_ = function(
    isVideo, url, onSuccess, onFailure) {
  var element = document.createElement(isVideo ? 'video' : 'img');

  var drawThumbnail = function() {
    var canvas = document.createElement('canvas');
    var context = canvas.getContext('2d');
    var thumbnailWidth = camera.models.FileSystem.THUMBNAIL_WIDTH;
    var ratio = isVideo ?
        element.videoHeight / element.videoWidth :
        element.height / element.width;
    var thumbnailHeight = Math.round(thumbnailWidth * ratio);
    canvas.width = thumbnailWidth;
    canvas.height = thumbnailHeight;
    context.drawImage(element, 0, 0, thumbnailWidth, thumbnailHeight);
    canvas.toBlob(function(blob) {
      if (blob) {
        onSuccess(blob);
      } else {
        onFailure('Failed to create thumbnail.');
      }
    }, 'image/jpeg');
  };

  if (isVideo) {
    element.onloadeddata = drawThumbnail;
  } else {
    element.onload = drawThumbnail;
  }
  element.onerror = onFailure;
  element.src = url;
};

/**
 * Gets the thumbnail name of the given picture.
 * @param {FileEntry} entry Picture's file entry.
 * @return {string} Thumbnail name.
 */
camera.models.FileSystem.getThumbnailName = function(entry) {
  var thumbnailName = camera.models.FileSystem.THUMBNAIL_PREFIX + entry.name;
  return (thumbnailName.substr(0, thumbnailName.lastIndexOf('.')) ||
      thumbnailName) + '.jpg';
};

/**
 * Creates and saves the thumbnail of the given picture.
 * @param {FileEntry} entry Picture's file entry whose thumbnail to be saved.
 * @param {function(FileEntry)} onSuccess Success callback with the file entry
 *     of the saved thumbnail.
 * @param {function(*=)} onFailure Failure callback.
 */
camera.models.FileSystem.saveThumbnail = function(entry, onSuccess, onFailure) {
  camera.models.FileSystem.pictureURL(entry).then(url => {
    return new Promise((resolve, reject) => {
      camera.models.FileSystem.createThumbnail_(
          camera.models.FileSystem.hasVideoPrefix(entry), url, resolve, reject);
    });
  }).then(blob => {
    return new Promise((resolve, reject) => {
      var thumbnailName = camera.models.FileSystem.getThumbnailName(entry);
      camera.models.FileSystem.saveToFile_(camera.models.FileSystem.internalFs,
          thumbnailName, blob, resolve, reject);
    });
  }).then(onSuccess).catch(onFailure);
};

/**
 * Checks if the entry's name has the video prefix.
 * @param {FileEntry} entry File entry.
 * @return {boolean} Has the video prefix or not.
 * @private
 */
camera.models.FileSystem.hasVideoPrefix = function(entry) {
  return entry.name.startsWith(camera.models.FileSystem.VIDEO_PREFIX);
};

/**
 * Checks if the entry's name has the image prefix.
 * @param {FileEntry} entry File entry.
 * @return {boolean} Has the image prefix or not.
 * @private
 */
camera.models.FileSystem.hasImagePrefix_ = function(entry) {
  return entry.name.startsWith(camera.models.FileSystem.IMAGE_PREFIX);
};

/**
 * Checks if the entry's name has the thumbnail prefix.
 * @param {FileEntry} entry File entry.
 * @return {boolean} Has the thumbnail prefix or not.
 * @private
 */
camera.models.FileSystem.hasThumbnailPrefix_ = function(entry) {
  return entry.name.startsWith(camera.models.FileSystem.THUMBNAIL_PREFIX);
};

/**
 * Parse and filter the internal entries to thumbnail and picture entries.
 * @param {Array.<FileEntry>} internalEntries Internal file entries.
 * @param {Object{string, FileEntry}} thumbnailEntriesByName Result thumbanil
 *     entries mapped by thumbnail names, initially empty.
 * @param {Array.<FileEntry>=} pictureEntries Result picture entries,
 *     initially empty.
 * @private
 */
camera.models.FileSystem.parseInternalEntries_ = function(
    internalEntries, thumbnailEntriesByName, pictureEntries) {
  var isThumbnail = camera.models.FileSystem.hasThumbnailPrefix_;
  var thumbnailEntries = [];
  if (pictureEntries) {
    for (var index = 0; index < internalEntries.length; index++) {
      if (isThumbnail(internalEntries[index])) {
        thumbnailEntries.push(internalEntries[index]);
      } else {
        pictureEntries.push(internalEntries[index]);
      }
    }
  } else {
    thumbnailEntries = internalEntries.filter(isThumbnail);
  }
  for (var index = 0; index < thumbnailEntries.length; index++) {
    var thumbnailEntry = thumbnailEntries[index];
    thumbnailEntriesByName[thumbnailEntry.name] = thumbnailEntry;
  }
};

/**
 * Returns the picture and thumbnail entries.
 * @param {function(Array.<FileEntry>, Object{string, FileEntry})} onSuccess
 *     Success callback with the picture entries and the thumbnail entries
 *     mapped by thumbnail names.
 * @param {function(*=)} onFailure Failure callback
 */
camera.models.FileSystem.getEntries = function(onSuccess, onFailure) {
  camera.models.FileSystem.reloadEntries_(
      function(internalEntries, externalEntries) {
    var pictureEntries = [];
    var thumbnailEntriesByName = {};

    if (camera.models.FileSystem.externalFs) {
      pictureEntries = externalEntries.filter(entry => {
        if (!camera.models.FileSystem.hasVideoPrefix(entry) &&
            !camera.models.FileSystem.hasImagePrefix_(entry)) {
          return false;
        }
        return entry.name.match(/_(\d{8})_(\d{6})(?: \((\d+)\))?/);
      });
      camera.models.FileSystem.parseInternalEntries_(
          internalEntries, thumbnailEntriesByName);
    } else {
      camera.models.FileSystem.parseInternalEntries_(
          internalEntries, thumbnailEntriesByName, pictureEntries);
    }
    onSuccess(pictureEntries, thumbnailEntriesByName);
  }, onFailure);
};

/**
 * Returns an URL for a picture.
 * @param {FileEntry} entry File entry.
 * @return {!Promise<string>} Promise for the result.
 */
camera.models.FileSystem.pictureURL = function(entry) {
  return new Promise(resolve => {
    if (camera.models.FileSystem.externalFs) {
      entry.file(file => {
        resolve(URL.createObjectURL(file));
      });
    } else {
      resolve(entry.toURL());
    }
  });
};

/**
 * Increments the file index of a given file name to avoid name conflicts.
 * @param {string} fileName File name.
 * @return {string} File name with incremented index.
 * @private
 */
camera.models.FileSystem.incrementFileName_ = function(fileName) {
  var parseFileName = function(fileName) {
    var base = '', ext = '', index = 0;
    var match = fileName.match(/^([^.]+)(\..+)?$/);
    if (match) {
      base = match[1];
      ext = match[2];
      match = base.match(/ \((\d+)\)$/);
      if (match) {
        base = base.substring(0, match.index);
        index = parseInt(match[1], 10);
      }
    }
    return [base, ext, index];
  };

  var fileNameParts = parseFileName(fileName);
  var fileIndex = fileNameParts[2] + 1;
  return fileNameParts[0] + ' (' + fileIndex + ')' + fileNameParts[1];
};

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Object representing an image item (a photo or a video).
 *
 * @param {string} url Image url.
 * @constructor
 */
Gallery.Item = function(url) {
  this.url_ = url;
  this.original_ = true;
};

/**
 * @return {string} Image url.
 */
Gallery.Item.prototype.getUrl = function() { return this.url_ };

/**
 * @param {string} url New url.
 */
Gallery.Item.prototype.setUrl = function(url) { this.url_ = url };

/**
 * @return {string} File name.
 */
Gallery.Item.prototype.getFileName = function() {
  return ImageUtil.getFullNameFromUrl(this.url_);
};

/**
 * @return {boolean} True if this image has not been created in this session.
 */
Gallery.Item.prototype.isOriginal = function() { return this.original_ };

// TODO: Localize?
/**
 * @type {string} Prefix for a edited copy file name.
 */
Gallery.Item.COPY_SIGNATURE = 'Edited';

/**
 * Regular exression to match 'Edited - ...'.
 * @type {RegExp}
 */
Gallery.Item.REGEXP_COPY_0 =
    new RegExp('^' + Gallery.Item.COPY_SIGNATURE + '( - .+)$');

/**
 * Regular exression to match 'Edited (N) - ...'.
 * @type {RegExp}
 */
Gallery.Item.REGEXP_COPY_N =
    new RegExp('^' + Gallery.Item.COPY_SIGNATURE + ' \\((\\d+)\\)( - .+)$');

/**
 * Create a name for an edited copy of the file.
 *
 * @param {Entry} dirEntry Entry.
 * @param {function} callback Callback.
 * @private
 */
Gallery.Item.prototype.createCopyName_ = function(dirEntry, callback) {
  var name = this.getFileName();

  // If the item represents a file created during the current Gallery session
  // we reuse it for subsequent saves instead of creating multiple copies.
  if (!this.original_) {
    callback(name);
    return;
  }

  var ext = '';
  var index = name.lastIndexOf('.');
  if (index != -1) {
    ext = name.substr(index);
    name = name.substr(0, index);
  }

  if (!ext.match(/jpe?g/i)) {
    // Chrome can natively encode only two formats: JPEG and PNG.
    // All non-JPEG images are saved in PNG, hence forcing the file extension.
    ext = '.png';
  }

  function tryNext(tries) {
    // All the names are used. Let's overwrite the last one.
    if (tries == 0) {
      setTimeout(callback, 0, name + ext);
      return;
    }

    // If the file name contains the copy signature add/advance the sequential
    // number.
    var matchN = Gallery.Item.REGEXP_COPY_N.exec(name);
    var match0 = Gallery.Item.REGEXP_COPY_0.exec(name);
    if (matchN && matchN[1] && matchN[2]) {
      var copyNumber = parseInt(matchN[1], 10) + 1;
      name = Gallery.Item.COPY_SIGNATURE + ' (' + copyNumber + ')' + matchN[2];
    } else if (match0 && match0[1]) {
      name = Gallery.Item.COPY_SIGNATURE + ' (1)' + match0[1];
    } else {
      name = Gallery.Item.COPY_SIGNATURE + ' - ' + name;
    }

    dirEntry.getFile(name + ext, {create: false, exclusive: false},
        tryNext.bind(null, tries - 1),
        callback.bind(null, name + ext));
  }

  tryNext(10);
};

/**
 * Write the new item content to the file.
 *
 * @param {Entry} dirEntry Directory entry.
 * @param {boolean} overwrite True if overwrite, false if copy.
 * @param {HTMLCanvasElement} canvas Source canvas.
 * @param {ImageEncoder.MetadataEncoder} metadataEncoder MetadataEncoder.
 * @param {function(boolean)} opt_callback Callback accepting true for success.
 */
Gallery.Item.prototype.saveToFile = function(
    dirEntry, overwrite, canvas, metadataEncoder, opt_callback) {
  ImageUtil.metrics.startInterval(ImageUtil.getMetricName('SaveTime'));

  var name = this.getFileName();

  var onSuccess = function(url) {
    console.log('Saved from gallery', name);
    ImageUtil.metrics.recordEnum(ImageUtil.getMetricName('SaveResult'), 1, 2);
    ImageUtil.metrics.recordInterval(ImageUtil.getMetricName('SaveTime'));
    this.setUrl(url);
    if (opt_callback) opt_callback(true);
  }.bind(this);

  function onError(error) {
    console.log('Error saving from gallery', name, error);
    ImageUtil.metrics.recordEnum(ImageUtil.getMetricName('SaveResult'), 0, 2);
    if (opt_callback) opt_callback(false);
  }

  function doSave(newFile, fileEntry) {
    fileEntry.createWriter(function(fileWriter) {
      function writeContent() {
        fileWriter.onwriteend = onSuccess.bind(null, fileEntry.toURL());
        fileWriter.write(ImageEncoder.getBlob(canvas, metadataEncoder));
      }
      fileWriter.onerror = function(error) {
        onError(error);
        // Disable all callbacks on the first error.
        fileWriter.onerror = null;
        fileWriter.onwriteend = null;
      };
      if (newFile) {
        writeContent();
      } else {
        fileWriter.onwriteend = writeContent;
        fileWriter.truncate(0);
      }
    }, onError);
  }

  function getFile(newFile) {
    dirEntry.getFile(name, {create: newFile, exclusive: newFile},
        doSave.bind(null, newFile), onError);
  }

  function checkExistence() {
    dirEntry.getFile(name, {create: false, exclusive: false},
        getFile.bind(null, false /* existing file */),
        getFile.bind(null, true /* create new file */));
  }

  if (overwrite) {
    checkExistence();
  } else {
    this.createCopyName_(dirEntry, function(copyName) {
      this.original_ = false;
      name = copyName;
      checkExistence();
    }.bind(this));
  }
};

/**
 * Rename the file.
 *
 * @param {Entry} dir Directory entry.
 * @param {string} name New file name.
 * @param {function} onSuccess Success callback.
 * @param {function} onExists Called if the file with the new name exists.
 */
Gallery.Item.prototype.rename = function(dir, name, onSuccess, onExists) {
  var oldName = this.getFileName();
  if (ImageUtil.getExtensionFromFullName(name) ==
      ImageUtil.getExtensionFromFullName(oldName)) {
    name = ImageUtil.getFileNameFromFullName(name);
  }
  var newName = ImageUtil.replaceFileNameInFullName(oldName, name);
  if (oldName == newName) return;

  function onError() {
    console.log('Rename error: "' + oldName + '" to "' + newName + '"');
  }

  var onRenamed = function(entry) {
    this.setUrl(entry.toURL());
    onSuccess();
  }.bind(this);

  var doRename = function() {
    dir.getFile(
        this.getFileName(),
        {create: false},
        function(entry) { entry.moveTo(dir, newName, onRenamed, onError); },
        onError);
  }.bind(this);

  dir.getFile(newName, {create: false, exclusive: false}, onExists, doRename);
};

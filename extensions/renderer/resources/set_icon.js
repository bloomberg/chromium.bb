// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var SetIconCommon = requireNative('setIcon').SetIconCommon;
var sendRequest = require('sendRequest').sendRequest;

function loadImagePath(path, iconSize, callback) {
  var img = new Image();
  img.onerror = function() {
    console.error('Could not load action icon \'' + path + '\'.');
  };
  img.onload = function() {
    var canvas = document.createElement('canvas');
    canvas.width = img.width > iconSize ? iconSize : img.width;
    canvas.height = img.height > iconSize ? iconSize : img.height;

    var canvas_context = canvas.getContext('2d');
    canvas_context.clearRect(0, 0, canvas.width, canvas.height);
    canvas_context.drawImage(img, 0, 0, canvas.width, canvas.height);
    var imageData = canvas_context.getImageData(0, 0, canvas.width,
                                                canvas.height);
    callback(imageData);
  };
  img.src = path;
}

function verifyImageData(imageData, iconSize) {
  // Verify that this at least looks like an ImageData element.
  // Unfortunately, we cannot use instanceof because the ImageData
  // constructor is not public.
  //
  // We do this manually instead of using JSONSchema to avoid having these
  // properties show up in the doc.
  if (!('width' in imageData) ||
      !('height' in imageData) ||
      !('data' in imageData)) {
    throw new Error(
        'The imageData property must contain an ImageData object or' +
        ' dictionary of ImageData objects.');
  }

  if (imageData.width > iconSize ||
      imageData.height > iconSize) {
    throw new Error(
        'The imageData property must contain an ImageData object that ' +
        'is no larger than ' + iconSize + ' pixels square.');
  }
}

/**
 * Normalizes |details| to a format suitable for sending to the browser,
 * for example converting ImageData to a binary representation.
 *
 * @param {ImageDetails} details
 *   The ImageDetails passed into an extension action-style API.
 * @param {Function} callback
 *   The callback function to pass processed imageData back to. Note that this
 *   callback may be called reentrantly.
 */
function setIcon(details, callback) {
  var iconSizes = [19, 38];
  // Note that iconIndex is actually deprecated, and only available to the
  // pageAction API.
  // TODO(kalman): Investigate whether this is for the pageActions API, and if
  // so, delete it.
  if ('iconIndex' in details) {
    callback(details);
    return;
  }

  if ('imageData' in details) {
    var isEmpty = true;
    for (var i = 0; i < iconSizes.length; i++) {
      var sizeKey = iconSizes[i].toString();
      if (sizeKey in details.imageData) {
        verifyImageData(details.imageData[sizeKey], iconSizes[i]);
        isEmpty =false;
      }
    }

    if (isEmpty) {
      // If details.imageData is not dictionary with keys in set {'19', '38'},
      // it must be an ImageData object.
      var sizeKey = iconSizes[0].toString();
      var imageData = details.imageData;
      details.imageData = {};
      details.imageData[sizeKey] = imageData;
      verifyImageData(details.imageData[sizeKey], iconSizes[0]);
    }
    callback(SetIconCommon(details));
    return;
  }

  if ('path' in details) {
    if (typeof details.path == 'object') {
      details.imageData = {};
      var isEmpty = true;
      var processIconSize = function(index) {
        if (index == iconSizes.length) {
          delete details.path;
          if (isEmpty)
            throw new Error('The path property must not be empty.');
          callback(SetIconCommon(details));
          return;
        }
        var sizeKey = iconSizes[index].toString();
        if (!(sizeKey in details.path)) {
          processIconSize(index + 1);
          return;
        }
        isEmpty = false;
        loadImagePath(details.path[sizeKey], iconSizes[index],
          function(imageData) {
            details.imageData[sizeKey] = imageData;
            processIconSize(index + 1);
          });
      }
      processIconSize(0);
    } else if (typeof details.path == 'string') {
      var sizeKey = iconSizes[0].toString();
      details.imageData = {};
      loadImagePath(details.path, iconSizes[0],
          function(imageData) {
            details.imageData[sizeKey] = imageData;
            delete details.path;
            callback(SetIconCommon(details));
            return;
      });
    }
    return;
  }
  throw new Error('Either the path or imageData property must be specified.');
}

exports.setIcon = setIcon;

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var SetIconCommon = requireNative('setIcon').SetIconCommon;
var sendRequest = require('sendRequest').sendRequest;

function setIcon(details, callback, name, parameters, actionType) {
  var iconSize = 19;
  if ("iconIndex" in details) {
    sendRequest(name, [details, callback], parameters);
  } else if ("imageData" in details) {
    // Verify that this at least looks like an ImageData element.
    // Unfortunately, we cannot use instanceof because the ImageData
    // constructor is not public.
    //
    // We do this manually instead of using JSONSchema to avoid having these
    // properties show up in the doc.
    if (!("width" in details.imageData) ||
        !("height" in details.imageData) ||
        !("data" in details.imageData)) {
      throw new Error(
          "The imageData property must contain an ImageData object.");
    }

    if (details.imageData.width > iconSize ||
        details.imageData.height > iconSize) {
      throw new Error(
          "The imageData property must contain an ImageData object that " +
          "is no larger than " + iconSize + " pixels square.");
    }

    sendRequest(name, [details, callback], parameters,
                {noStringify: true, nativeFunction: SetIconCommon});
  } else if ("path" in details) {
    var img = new Image();
    img.onerror = function() {
      console.error("Could not load " + actionType + " icon '" +
                    details.path + "'.");
    };
    img.onload = function() {
      var canvas = document.createElement("canvas");
      canvas.width = img.width > iconSize ? iconSize : img.width;
      canvas.height = img.height > iconSize ? iconSize : img.height;

      var canvas_context = canvas.getContext('2d');
      canvas_context.clearRect(0, 0, canvas.width, canvas.height);
      canvas_context.drawImage(img, 0, 0, canvas.width, canvas.height);
      delete details.path;
      details.imageData = canvas_context.getImageData(0, 0, canvas.width,
                                                      canvas.height);
      sendRequest(name, [details, callback], parameters,
                  {noStringify: true, nativeFunction: SetIconCommon});
    };
    img.src = details.path;
  } else {
    throw new Error(
        "Either the path or imageData property must be specified.");
  }
}

exports.setIcon = setIcon;

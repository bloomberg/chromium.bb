// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function getConstraintsForDevice(deviceLabel) {
  return new Promise(function(resolve, reject) {
    navigator.mediaDevices.enumerateDevices()
    .then(function(devices) {
      for (var i = 0; i < devices.length; ++i) {
        if (deviceLabel == devices[i].label) {
          return resolve({video:{deviceId: {exact: devices[i].deviceId},
                                 width: {exact:96},
                                 height: {exact:96}}
                          });
        }
      }
      return reject("Expected to have a device with label:" + deviceLabel);
    })
  });
}

function getFake16bitStream() {
  return new Promise(function(resolve, reject) {
    getConstraintsForDevice("fake_device_1")
    .then(function(constraints) {
      if (!constraints)
        return reject("No fake device found");
      return navigator.mediaDevices.getUserMedia(constraints);
    }).then(function(stream) {
      return resolve(stream);
    });
  });
}

function testVideoToImageBitmap(videoElementName, success, error)
{
  var bitmaps = {};
  var video = $(videoElementName);
  var canvas = document.createElement('canvas');
  canvas.width = 96;
  canvas.height = 96;
  document.body.appendChild(canvas);
  var p1 = createImageBitmap(video).then(function(imageBitmap) {
    return runImageBitmapTest(imageBitmap, canvas, false); });
  var p2 = createImageBitmap(video,
      {imageOrientation: "none", premultiplyAlpha: "premultiply"}).then(
          function(imageBitmap) {
            return runImageBitmapTest(imageBitmap, canvas, false); });
  var p3 = createImageBitmap(video,
      {imageOrientation: "none", premultiplyAlpha: "default"}).then(
          function(imageBitmap) {
            return runImageBitmapTest(imageBitmap, canvas, false); });
  var p4 = createImageBitmap(video,
      {imageOrientation: "none", premultiplyAlpha: "none"}).then(
          function(imageBitmap) {
            return runImageBitmapTest(imageBitmap, canvas, false); });
  var p5 = createImageBitmap(video,
      {imageOrientation: "flipY", premultiplyAlpha: "premultiply"}).then(
          function(imageBitmap) {
            return runImageBitmapTest(imageBitmap, canvas, true); });
  var p6 = createImageBitmap(video,
      {imageOrientation: "flipY", premultiplyAlpha: "default"}).then(
          function(imageBitmap) {
            return runImageBitmapTest(imageBitmap, canvas, true); });
  var p7 = createImageBitmap(video,
      {imageOrientation: "flipY", premultiplyAlpha: "none"}).then(
          function(imageBitmap) {
            return runImageBitmapTest(imageBitmap, canvas, true); });
  return Promise.all([p1, p2, p3, p4, p5, p6, p7]).then(success(), reason => {
    return error({name: reason});
  });
}

function runImageBitmapTest(bitmap, canvas, flipped) {
  var context = canvas.getContext('2d');
  context.drawImage(bitmap,0,0);
  var imageData = context.getImageData(0, 0, canvas.width, canvas.height);
  // Fake capture device 96x96 depth image is gradient. See also
  // Draw16BitGradient in fake_video_capture_device.cc.
  var color_step = 255.0 / (canvas.width + canvas.height);
  var rowsColumnsToCheck = [[1, 1],
                            [0, canvas.width - 1],
                            [canvas.height - 1, 0],
                            [canvas.height - 1, canvas.width - 1],
                            [canvas.height - 3, canvas.width - 4]];

  // Same value is expected for all color components.
  if (imageData.data[0] != imageData.data[1] ||
      imageData.data[0] != imageData.data[2])
    return Promise.reject("Values " + imageData.data[0] + ", " +
        imageData.data[1] + ", " + imageData.data[2] + " differ at top left");

  // Calculate all reference points based on top left and compare.
  for (var j = 0; j < rowsColumnsToCheck.length; ++j) {
    var row = rowsColumnsToCheck[j][0];
    var column = rowsColumnsToCheck[j][1];
    var i = (canvas.width * row + column) * 4;
    if (imageData.data[i] != imageData.data[i + 1] ||
        imageData.data[i] != imageData.data[i + 2])
      return Promise.reject("Values " + imageData.data[i] + ", " +
          imageData.data[i + 1] + ", " + imageData.data[i + 2] +
          " differ at index " + i);
    var calculated = (imageData.data[0] +
                      color_step * ((flipped ? -row : row) + column)) % 255;
    if (Math.abs(calculated - imageData.data[i]) > 2)
      return Promise.reject("Reference value " + imageData.data[i] +
          " differs from calculated: " + calculated + " at index " + i +
          ". TopLeft value:" + imageData.data[0]);
  }
  return true;
}

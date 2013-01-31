// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testWindow = window.opener;

var video = document.getElementById("video");
var canvas = document.createElement("canvas");

var colors = [ [ 255, 0, 0 ], [ 0, 255, 0 ], [ 0, 0, 255 ] ];

function receiverLoop(colorsFoundSoFar) {
  var width = video.videoWidth;
  var height = video.videoHeight;

  // Grab a snapshot of the center pixel of the video using the canvas.
  canvas.width = width;
  canvas.height = height;
  var ctx = canvas.getContext("2d");
  ctx.drawImage(video, 0, 0, width, height);
  var imageData = ctx.getImageData(canvas.width / 2, canvas.height / 2, 1, 1);
  var pixel = [ imageData.data[0], imageData.data[1], imageData.data[2] ];

  // Check whether the pixel is of an unseen color value.
  var numColorsSeen = 0;
  for (var i = 0; i < colors.length; ++i) {
    if (colorsFoundSoFar[i]) {
      ++numColorsSeen;
      continue;
    }
    // Note: The video encode/decode process loses color accuracy, which is
    // accounted for here.
    var unseenColor = colors[i];
    if (Math.abs(pixel[0] - unseenColor[0]) < 10 &&
        Math.abs(pixel[1] - unseenColor[1]) < 10 &&
        Math.abs(pixel[2] - unseenColor[2]) < 10) {
      console.log("Observed an expected color RGB(" + unseenColor +
                  ") in the video as RGB(" + pixel + ")");
      colorsFoundSoFar[i] = true;
      ++numColorsSeen;
    }
  }

  // If all colors have been observed, indicate test success and exit the loop.
  if (numColorsSeen == colors.length) {
    testWindow.receivedAllColors();
    return;
  }

  // Wait a little bit and then check again.
  setTimeout(receiverLoop, 50, colorsFoundSoFar);
}

video.addEventListener('play', function () {
  var noColorsFoundSoFar = colors.map(function () { return false; });
  setTimeout(receiverLoop, 50, noColorsFoundSoFar);
}, false);
video.src = webkitURL.createObjectURL(testWindow.activeStream);
video.play();

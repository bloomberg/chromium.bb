// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Function to take the screenshot of the current screen.
 * @param {function(string)} callback Callback for returning the data URL to the
 *                           screenshot.
 */
function takeScreenshot(callback) {
  var streaming = false;
  var video = document.createElement('video');

  video.addEventListener('canplay', function(e) {
    if (!streaming) {
      streaming = true;

      var canvas = document.createElement('canvas');
      canvas.setAttribute('width', video.videoWidth);
      canvas.setAttribute('height', video.videoHeight);
      canvas.getContext('2d').drawImage(
          video, 0, 0, video.videoWidth, video.videoHeight);

      video.pause();
      video.src = '';

      callback(canvas.toDataURL('image/png'));
    }
  }, false);

  navigator.webkitGetUserMedia(
    {
      video: {mandatory: {chromeMediaSource: 'screen'}}
    },
    function(stream) {
      video.src = window.webkitURL.createObjectURL(stream);
      video.play();
    },
    function(err) {
      console.error('takeScreenshot failed: ' +
          err.name + '; ' + err.message + '; ' + err.constraintName);
    }
  );
}

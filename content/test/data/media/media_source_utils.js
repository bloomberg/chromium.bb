// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function loadMediaFromURL(video) {
  installTitleEventHandler(video, 'error');
  video.addEventListener('playing', function(event) {
    console.log('Video Playing.');
  });
  var source = loadMediaSource(QueryString.mediafile, QueryString.mediatype);
  video.src = window.URL.createObjectURL(source);
}

function loadMediaSource(mediaFiles, mediaTypes, appendSourceCallbackFn) {
  mediaFiles = convertToArray(mediaFiles);
  mediaTypes = convertToArray(mediaTypes);

  if (!mediaFiles || !mediaTypes)
    failTest('Missing parameters in loadMediaSource().');

  var totalAppended = 0;
  function onSourceOpen(e) {
    console.log('onSourceOpen', e);
    // We can load multiple media files using the same media type. However, if
    // more than one media type is used, we expect to have a media type entry
    // for each corresponding media file.
    var srcBuffer = null;
    for (var i = 0; i < mediaFiles.length; i++) {
      if (i == 0 || mediaFiles.length == mediaTypes.length) {
        console.log('Creating a source buffer for type ' + mediaTypes[i]);
        try {
          srcBuffer = mediaSource.addSourceBuffer(mediaTypes[i]);
        } catch (e) {
          failTest('Exception adding source buffer: ' + e.message);
          return;
        }
      }
      doAppend(mediaFiles[i], srcBuffer);
    }
  }

  function doAppend(mediaFile, srcBuffer) {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', mediaFile);
    xhr.responseType = 'arraybuffer';
    xhr.addEventListener('load', function(e) {
      var onUpdateEnd = function(e) {
        console.log('End of appending buffer from ' + mediaFile);
        srcBuffer.removeEventListener('updateend', onUpdateEnd);
        totalAppended++;
        if (totalAppended == mediaFiles.length) {
          if (appendSourceCallbackFn)
            appendSourceCallbackFn(mediaSource);
          else
            mediaSource.endOfStream();
        }
      };
      srcBuffer.addEventListener('updateend', onUpdateEnd);
      srcBuffer.appendBuffer(new Uint8Array(e.target.response));
    });
    xhr.send();
  }

  var mediaSource = new MediaSource();
  mediaSource.addEventListener('sourceopen', onSourceOpen);
  return mediaSource;
}

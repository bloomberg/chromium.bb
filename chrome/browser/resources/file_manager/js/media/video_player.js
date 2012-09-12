// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Display error message.
 * @param {string} opt_message Message id.
 */
function onError(opt_message) {
  var errorBanner = document.querySelector('#error');
  errorBanner.textContent =
      loadTimeData.getString(opt_message || 'GALLERY_VIDEO_ERROR');
  errorBanner.setAttribute('visible', 'true');
}

/**
 * @param {Element} playerContainer Main container.
 * @param {Element} videoContainer Container for the video element.
 * @param {Element} controlsContainer Container for video controls.
 * @constructor
 */
function FullWindowVideoControls(
    playerContainer, videoContainer, controlsContainer) {
  VideoControls.call(this,
      controlsContainer,
      onError.bind(null, null),
      function() { chrome.fileBrowserPrivate.toggleFullscreen() },
      videoContainer);

  window.addEventListener('unload', this.savePosition.bind(this));

  this.updateStyle();
  window.addEventListener('resize', this.updateStyle.bind(this));

  playerContainer.addEventListener('keydown', function(e) {
    if (e.keyIdentifier == 'U+0020')
      this.togglePlayStateWithFeedback();
  }.bind(this));

  videoContainer.addEventListener('click',
      this.togglePlayStateWithFeedback.bind(this));

  var inactivityWatcher = new MouseInactivityWatcher(playerContainer);
  inactivityWatcher.stopActivity();
}

FullWindowVideoControls.prototype = { __proto__: VideoControls.prototype };

/**
 * Save the current play state in the location hash so that it survives
 * the page reload.
 */
FullWindowVideoControls.prototype.onPlayStateChanged = function() {
  this.encodeStateIntoLocation();
};

/**
 * Restore the play state after the video is loaded.
 */
FullWindowVideoControls.prototype.restorePlayState = function() {
  if (!this.decodeStateFromLocation()) {
    VideoControls.prototype.restorePlayState.apply(this, arguments);
    this.play();
  }
};

/**
 * Initialize the video player window.
 */
function loadVideoPlayer() {
  document.oncontextmenu = function(e) { e.preventDefault(); };
  document.ondragstart = function(e) { e.preventDefault() };

  chrome.fileBrowserPrivate.getStrings(function(strings) {
    loadTimeData.data = strings;

    var src = document.location.search.substr(1);
    if (!src) {
      onError();
      return;
    }

    document.title = decodeURIComponent(src.split('/').pop());

    var controls = new FullWindowVideoControls(
       document.querySelector('#video-player'),
       document.querySelector('#video-container'),
       document.querySelector('#controls'));

    var metadataCache = MetadataCache.createFull();
    metadataCache.get(src, 'streaming', function(streaming) {
      if (streaming && streaming.url) {
        if (!navigator.onLine) {
          onError('GALLERY_VIDEO_OFFLINE');
          return;
        }
        src = streaming.url;
        console.log('Streaming: ' + src);
      } else {
        console.log('Playing local file: ' + src);
      }

      // If the video player is starting before the first instance of the File
      // Manager then it does not have access to filesystem URLs.
      // Request it now.
      chrome.fileBrowserPrivate.requestLocalFileSystem(function() {
        var video = document.querySelector('video');
        video.src = src;
        video.load();
        controls.attachMedia(video);
      });
    });
  });
}

document.addEventListener('DOMContentLoaded', loadVideoPlayer);

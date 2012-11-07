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

  this.updateStyle();
  window.addEventListener('resize', this.updateStyle.bind(this));

  document.addEventListener('keydown', function(e) {
    if (e.keyIdentifier == 'U+0020') {
      this.togglePlayStateWithFeedback();
      e.preventDefault();
    }
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

var controls;

var metadataCache;

/**
 * Initialize the video player window.
 */
function loadVideoPlayer() {
  if (!util.TEST_HARNESS)
    document.oncontextmenu = function(e) { e.preventDefault(); };
  document.ondragstart = function(e) { e.preventDefault() };

  chrome.fileBrowserPrivate.getStrings(function(strings) {
    loadTimeData.data = strings;

    controls = new FullWindowVideoControls(
       document.querySelector('#video-player'),
       document.querySelector('#video-container'),
       document.querySelector('#controls'));

    var video = document.querySelector('video');
    controls.attachMedia(video);

    if (!util.platform.v2())
      window.addEventListener('unload', unload);

    metadataCache = MetadataCache.createFull();

    // If the video player is starting before the first instance of the File
    // Manager then it does not have access to filesystem URLs. Request it now.
    chrome.fileBrowserPrivate.requestLocalFileSystem(function() {
      reload(window.launchData || { url: document.location.search.substr(1) });
    });
  });
}

/**
 * Unload the player.
 */
function unload() {
  controls.savePosition();
  controls.cleanup();
}

/**
 * Reload the player.
 * @param {Object} launchData Launch data.
 */
function reload(launchData) {
  var src = launchData.url;
  if (!src) {
    onError();
    return;
  }

  document.title = decodeURIComponent(src.split('/').pop());

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

    var video = document.querySelector('video');
    video.src = src;
    video.load();
    if (util.platform.v2()) {
      video.addEventListener('loadedmetadata', function() {
        var appWindow = chrome.app.window.current();
        var oldWidth = appWindow.contentWindow.outerWidth;
        var oldHeight = appWindow.contentWindow.outerHeight;

        // TODO: chrome.app.window soon will be able to resize the content area.
        // Until then use approximate title bar height.
        var TITLE_HEIGHT = 28;

        var aspect = video.videoWidth / video.videoHeight;
        var newWidth = video.videoWidth;
        var newHeight = video.videoHeight + TITLE_HEIGHT;

        var shrinkX = newWidth / window.screen.availWidth;
        var shrinkY = newHeight / window.screen.availHeight;
        if (shrinkX > 1 || shrinkY > 1) {
          if (shrinkY > shrinkX) {
            newHeight = newHeight / shrinkY;
            newWidth = (newHeight - TITLE_HEIGHT) * aspect;
          } else {
            newWidth = newWidth / shrinkX;
            newHeight = newWidth / aspect + TITLE_HEIGHT;
          }
        }
        appWindow.resizeTo(newWidth, newHeight);
        appWindow.moveTo(
            appWindow.contentWindow.screenX - (newWidth - oldWidth) / 2,
            appWindow.contentWindow.screenY - (newHeight - oldHeight) / 2);
        appWindow.show();
      });
    }
  });
}

util.addPageLoadHandler(loadVideoPlayer);
